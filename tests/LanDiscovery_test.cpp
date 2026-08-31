// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanDiscovery.h"

#include <QHostAddress>
#include <QTest>

namespace {

QUrl validOfferUrl()
{
    return QUrl("http://192.168.1.4:32768/instance/0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
}

}  // namespace

class LanDiscoveryTest : public QObject {
    Q_OBJECT

   private slots:
    void roundTripsOffers();
    void rejectsUntrustedAnnouncements();
};

void LanDiscoveryTest::roundTripsOffers()
{
    const auto datagram = Lan::makeDiscoveryDatagram(QStringLiteral("Family pack"), { validOfferUrl() });
    QVERIFY(!datagram.isEmpty());

    const auto offers = Lan::parseDiscoveryDatagram(datagram, QHostAddress("192.168.1.9"));
    QCOMPARE(offers.size(), 1);
    QCOMPARE(offers.front().instanceName, QStringLiteral("Family pack"));
    QCOMPARE(offers.front().url, validOfferUrl());
}

void LanDiscoveryTest::rejectsUntrustedAnnouncements()
{
    QVERIFY(Lan::makeDiscoveryDatagram(QStringLiteral("Family pack"), { QUrl("http://8.8.8.8:32768/instance/token") }).isEmpty());
    const auto valid = Lan::makeDiscoveryDatagram(QStringLiteral("Family pack"), { validOfferUrl() });
    QVERIFY(!Lan::parseDiscoveryDatagram(valid, QHostAddress("8.8.8.8")).size());
    QVERIFY(!Lan::parseDiscoveryDatagram(QByteArray("not-json"), QHostAddress("192.168.1.9")).size());
}

QTEST_GUILESS_MAIN(LanDiscoveryTest)

#include "LanDiscovery_test.moc"
