// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanInstanceService.h"
#include "InstanceList.h"
#include "lan/LanNetwork.h"
#include "lan/LanProtocol.h"
#include "settings/INISettingsObject.h"

#include <QDir>
#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUdpSocket>

#include <optional>

namespace {

QString serviceId()
{
    return QStringLiteral("0123456789abcdef0123456789abcdef");
}
QString publicInstanceId()
{
    return QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
}
QString capability()
{
    return QStringLiteral("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
}

QNetworkDatagram receiveDatagram(QUdpSocket& socket)
{
    if (!socket.hasPendingDatagrams()) {
        QSignalSpy readyRead(&socket, &QUdpSocket::readyRead);
        readyRead.wait(5000);
    }
    return socket.receiveDatagram();
}

const Lan::Message& requireMessage(const std::optional<Lan::Message>& message)
{
    if (!message) {
        qFatal("Expected a valid LAN protocol message");
    }
    return message.value();
}

}  // namespace

class LanInstanceServiceTest : public QObject {
    Q_OBJECT

   private slots:
    void discoversAndNegotiatesWithAnIndependentControlPort();
};

void LanInstanceServiceTest::discoversAndNegotiatesWithAnIndependentControlPort()
{
    const QTemporaryDir directory(QDir::current().filePath("lan-instance-service-test-XXXXXX"));
    QVERIFY2(directory.isValid(), "Could not create the test's temporary build-directory data.");

    INISettingsObject settings(directory.filePath("settings.ini"));
    InstanceList instances(&settings, directory.filePath("instances"));
    Lan::InstanceService service(&instances, directory.path());
    QString error;
    QVERIFY2(service.start(&error), qPrintable(error));

    QUdpSocket peer;
    QVERIFY(peer.bind(QHostAddress::AnyIPv4, 0));
    const auto peerPort = peer.localPort();
    QVERIFY(peerPort != 0);

    const Lan::Announcement announcement{
        .serviceId = serviceId(), .instanceId = publicInstanceId(), .instanceName = QStringLiteral("Family pack"), .available = true
    };
    QCOMPARE(peer.writeDatagram(Lan::makeAnnouncementDatagram(announcement), QHostAddress::Broadcast, Lan::DiscoveryPort) > 0, true);
    QTRY_COMPARE_WITH_TIMEOUT(service.remoteInstances().size(), 1, 5000);
    const auto discovered = service.remoteInstances().front();
    QCOMPARE(discovered.serviceId, serviceId());
    QCOMPARE(discovered.instanceId, publicInstanceId());
    QCOMPARE(discovered.instanceName, QStringLiteral("Family pack"));
    QCOMPARE(discovered.port, peerPort);
    QVERIFY(discovered.available);

    const auto firstRequestId = service.requestImport(serviceId(), publicInstanceId(), &error);
    QVERIFY2(!firstRequestId.isEmpty(), qPrintable(error));
    const auto firstRequestDatagram = receiveDatagram(peer);
    QVERIFY(!firstRequestDatagram.isNull());
    const auto firstRequest = Lan::parseDatagram(firstRequestDatagram.data(), firstRequestDatagram.senderAddress());
    QVERIFY(firstRequest.has_value());
    QCOMPARE(std::get<Lan::ImportRequest>(requireMessage(firstRequest)).requestId, firstRequestId);

    service.cancelImport(firstRequestId);
    const auto cancelDatagram = receiveDatagram(peer);
    QVERIFY(!cancelDatagram.isNull());
    const auto cancel = Lan::parseDatagram(cancelDatagram.data(), cancelDatagram.senderAddress());
    QVERIFY(cancel.has_value());
    QCOMPARE(std::get<Lan::CancelRequest>(requireMessage(cancel)).requestId, firstRequestId);

    const auto secondRequestId = service.requestImport(serviceId(), publicInstanceId(), &error);
    QVERIFY2(!secondRequestId.isEmpty(), qPrintable(error));
    const auto secondRequestDatagram = receiveDatagram(peer);
    QVERIFY(!secondRequestDatagram.isNull());
    const auto secondRequest = Lan::parseDatagram(secondRequestDatagram.data(), secondRequestDatagram.senderAddress());
    QVERIFY(secondRequest.has_value());
    QCOMPARE(std::get<Lan::ImportRequest>(requireMessage(secondRequest)).requestId, secondRequestId);

    QSignalSpy transferReady(&service, &Lan::InstanceService::transferReady);
    const auto addresses = Lan::privateLanAddresses();
    QVERIFY(!addresses.isEmpty());
    const QUrl offeredUrl(QStringLiteral("http://%1:32768/instance/%2").arg(addresses.front().toString(), capability()));
    const auto readyDatagram = Lan::makeTransferReadyDatagram(secondRequestId, offeredUrl);
    QVERIFY(!readyDatagram.isEmpty());
    QCOMPARE(peer.writeDatagram(readyDatagram, secondRequestDatagram.senderAddress(), secondRequestDatagram.senderPort()) > 0, true);
    QVERIFY(transferReady.wait(5000));
    QCOMPARE(transferReady.first().at(0).toString(), secondRequestId);
    const auto receivedUrl = transferReady.first().at(1).toUrl();
    QCOMPARE(receivedUrl.host(), secondRequestDatagram.senderAddress().toString());
    QCOMPARE(receivedUrl.port(), 32768);
    QCOMPARE(receivedUrl.path(), QStringLiteral("/instance/") + capability());

    service.stop();
    QVERIFY(!service.isRunning());
}

QTEST_GUILESS_MAIN(LanInstanceServiceTest)

#include "LanInstanceService_test.moc"
