// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanProtocol.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <optional>
#include <variant>

namespace {

QString serviceId()
{
    return QStringLiteral("0123456789abcdef0123456789abcdef");
}
QString instanceId()
{
    return QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
}
QString requestId()
{
    return QStringLiteral("fedcba9876543210fedcba9876543210");
}
QString capability()
{
    return QStringLiteral("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
}
QHostAddress privateSender()
{
    return QHostAddress(QStringLiteral("192.168.1.9"));
}

QJsonObject decodeObject(const QByteArray& datagram)
{
    return QJsonDocument::fromJson(datagram).object();
}

const Lan::Message& requireMessage(const std::optional<Lan::Message>& message)
{
    if (!message) {
        qFatal("Expected a valid LAN protocol message");
    }
    return message.value();
}

}  // namespace

class LanProtocolTest : public QObject {
    Q_OBJECT

   private slots:
    void roundTripsEveryMessage();
    void rejectsUntrustedOrMalformedDatagrams();
    void rejectsInvalidFields();
};

void LanProtocolTest::roundTripsEveryMessage()
{
    const auto query = Lan::parseDatagram(Lan::makeQueryDatagram(), privateSender());
    QVERIFY(query.has_value());
    QVERIFY(std::holds_alternative<Lan::Query>(requireMessage(query)));

    const Lan::Announcement expectedAnnouncement{
        .serviceId = serviceId(), .instanceId = instanceId(), .instanceName = QStringLiteral("Family pack"), .available = true
    };
    const auto announcement = Lan::parseDatagram(Lan::makeAnnouncementDatagram(expectedAnnouncement), privateSender());
    QVERIFY(announcement.has_value());
    const auto actualAnnouncement = std::get<Lan::Announcement>(requireMessage(announcement));
    QCOMPARE(actualAnnouncement.serviceId, expectedAnnouncement.serviceId);
    QCOMPARE(actualAnnouncement.instanceId, expectedAnnouncement.instanceId);
    QCOMPARE(actualAnnouncement.instanceName, expectedAnnouncement.instanceName);
    QCOMPARE(actualAnnouncement.available, expectedAnnouncement.available);

    const Lan::ImportRequest expectedRequest{ .serviceId = serviceId(), .instanceId = instanceId(), .requestId = requestId() };
    const auto request = Lan::parseDatagram(Lan::makeImportRequestDatagram(expectedRequest), privateSender());
    QVERIFY(request.has_value());
    const auto actualRequest = std::get<Lan::ImportRequest>(requireMessage(request));
    QCOMPARE(actualRequest.serviceId, expectedRequest.serviceId);
    QCOMPARE(actualRequest.instanceId, expectedRequest.instanceId);
    QCOMPARE(actualRequest.requestId, expectedRequest.requestId);

    const Lan::CancelRequest expectedCancel{ .serviceId = serviceId(), .requestId = requestId() };
    const auto cancel = Lan::parseDatagram(Lan::makeCancelRequestDatagram(expectedCancel), privateSender());
    QVERIFY(cancel.has_value());
    const auto actualCancel = std::get<Lan::CancelRequest>(requireMessage(cancel));
    QCOMPARE(actualCancel.serviceId, expectedCancel.serviceId);
    QCOMPARE(actualCancel.requestId, expectedCancel.requestId);

    const QUrl offeredUrl(QStringLiteral("http://192.168.1.4:32768/instance/") + capability());
    const auto ready = Lan::parseDatagram(Lan::makeTransferReadyDatagram(requestId(), offeredUrl), privateSender());
    QVERIFY(ready.has_value());
    const auto actualReady = std::get<Lan::TransferReady>(requireMessage(ready));
    QCOMPARE(actualReady.requestId, requestId());
    QCOMPARE(actualReady.url, QUrl(QStringLiteral("http://192.168.1.9:32768/instance/") + capability()));

    const Lan::TransferFailure expectedFailure{ .requestId = requestId(), .reason = QStringLiteral("Instance is running") };
    const auto failure = Lan::parseDatagram(Lan::makeTransferFailureDatagram(expectedFailure), privateSender());
    QVERIFY(failure.has_value());
    const auto actualFailure = std::get<Lan::TransferFailure>(requireMessage(failure));
    QCOMPARE(actualFailure.requestId, expectedFailure.requestId);
    QCOMPARE(actualFailure.reason, expectedFailure.reason);
}

void LanProtocolTest::rejectsUntrustedOrMalformedDatagrams()
{
    QVERIFY(!Lan::parseDatagram(Lan::makeQueryDatagram(), QHostAddress(QStringLiteral("8.8.8.8"))).has_value());
    QVERIFY(!Lan::parseDatagram(Lan::makeQueryDatagram(), QHostAddress::LocalHost).has_value());
    QVERIFY(!Lan::parseDatagram(QByteArrayLiteral("not-json"), privateSender()).has_value());
    QVERIFY(!Lan::parseDatagram(QByteArray(4097, 'x'), privateSender()).has_value());

    auto wrongProtocol = decodeObject(Lan::makeQueryDatagram());
    wrongProtocol.insert(QStringLiteral("protocol"), QStringLiteral("other-protocol"));
    QVERIFY(!Lan::parseDatagram(QJsonDocument(wrongProtocol).toJson(QJsonDocument::Compact), privateSender()).has_value());
}

void LanProtocolTest::rejectsInvalidFields()
{
    QVERIFY(Lan::makeAnnouncementDatagram({ .serviceId = QStringLiteral("bad"),
                                            .instanceId = instanceId(),
                                            .instanceName = QStringLiteral("Family pack"),
                                            .available = true })
                .isEmpty());
    QVERIFY(Lan::makeAnnouncementDatagram({ .serviceId = serviceId(), .instanceId = instanceId(), .instanceName = {}, .available = true })
                .isEmpty());
    QVERIFY(Lan::makeImportRequestDatagram({ .serviceId = serviceId(), .instanceId = instanceId(), .requestId = QStringLiteral("bad") })
                .isEmpty());
    QVERIFY(Lan::makeCancelRequestDatagram({ .serviceId = serviceId(), .requestId = QStringLiteral("bad") }).isEmpty());
    QVERIFY(
        Lan::makeTransferReadyDatagram(requestId(), QUrl(QStringLiteral("https://192.168.1.4:32768/instance/") + capability())).isEmpty());
    QVERIFY(Lan::makeTransferFailureDatagram({ .requestId = requestId(), .reason = {} }).isEmpty());

    auto wrongAvailableType = decodeObject(Lan::makeAnnouncementDatagram(
        { .serviceId = serviceId(), .instanceId = instanceId(), .instanceName = QStringLiteral("Pack"), .available = true }));
    wrongAvailableType.insert(QStringLiteral("available"), QStringLiteral("true"));
    QVERIFY(!Lan::parseDatagram(QJsonDocument(wrongAvailableType).toJson(QJsonDocument::Compact), privateSender()).has_value());

    auto wrongPortType = decodeObject(
        Lan::makeTransferReadyDatagram(requestId(), QUrl(QStringLiteral("http://192.168.1.4:32768/instance/") + capability())));
    wrongPortType.insert(QStringLiteral("port"), QStringLiteral("32768"));
    QVERIFY(!Lan::parseDatagram(QJsonDocument(wrongPortType).toJson(QJsonDocument::Compact), privateSender()).has_value());

    auto fractionalPort = wrongPortType;
    fractionalPort.insert(QStringLiteral("port"), 32768.5);
    QVERIFY(!Lan::parseDatagram(QJsonDocument(fractionalPort).toJson(QJsonDocument::Compact), privateSender()).has_value());

    auto outOfRangePort = wrongPortType;
    outOfRangePort.insert(QStringLiteral("port"), 65536);
    QVERIFY(!Lan::parseDatagram(QJsonDocument(outOfRangePort).toJson(QJsonDocument::Compact), privateSender()).has_value());
}

QTEST_GUILESS_MAIN(LanProtocolTest)

#include "LanProtocol_test.moc"
