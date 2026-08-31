// SPDX-License-Identifier: GPL-3.0-only

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "lan/LanOffer.h"

namespace {

struct HttpResult {
    int status = 0;
    QByteArray body;
    QNetworkReply::NetworkError error = QNetworkReply::UnknownNetworkError;
};

HttpResult get(const QUrl& url)
{
    QNetworkAccessManager manager;
    QEventLoop eventLoop;
    QTimer timeout;
    timeout.setSingleShot(true);
    auto* reply = manager.get(QNetworkRequest(url));
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timeout.start(5000);
    eventLoop.exec();

    HttpResult result;
    if (!timeout.isActive()) {
        reply->abort();
    }
    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    result.error = reply->error();
    reply->deleteLater();
    return result;
}

}  // namespace

class LanOfferTest : public QObject {
    Q_OBJECT

   private slots:
    void streamsOnlyTheCapabilityUrl();
    void rejectsUnsafeImportUrls();
};

void LanOfferTest::streamsOnlyTheCapabilityUrl()
{
    QTemporaryDir directory(QDir::current().filePath("lan-offer-test-XXXXXX"));
    QVERIFY2(directory.isValid(), "Could not create the test's temporary build-directory data.");
    const QByteArray payload(128 * 1024, 'x');
    const auto archivePath = directory.filePath("instance.zip");
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::WriteOnly));
    QCOMPARE(archive.write(payload), payload.size());
    archive.close();

    Lan::Offer offer;
    QString error;
    QVERIFY2(offer.start(archivePath, &error), qPrintable(error));
    const auto urls = offer.urls();
    QVERIFY(!urls.empty());

    const auto authorized = get(urls.front());
    QCOMPARE(authorized.error, QNetworkReply::NoError);
    QCOMPARE(authorized.status, 200);
    QCOMPARE(authorized.body, payload);

    QUrl unauthorized(urls.front());
    unauthorized.setPath("/instance/0000000000000000000000000000000000000000000000000000000000000000");
    const auto denied = get(unauthorized);
    QCOMPARE(denied.status, 404);
    QCOMPARE(denied.body, QByteArray("offer not found\n"));

    offer.stop();
    QVERIFY(!offer.isSharing());

    const auto expired = get(urls.front());
    QVERIFY(expired.error != QNetworkReply::NoError);
}

void LanOfferTest::rejectsUnsafeImportUrls()
{
    QString error;
    const QUrl accepted("http://192.168.1.4:32768/instance/0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    QVERIFY(Lan::Offer::isLocalOfferUrl(accepted, &error));
    QVERIFY(!Lan::Offer::isLocalOfferUrl(
        QUrl("http://8.8.8.8:32768/instance/0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), &error));
    QVERIFY(!Lan::Offer::isLocalOfferUrl(
        QUrl("http://127.0.0.1:32768/instance/0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), &error));
    QVERIFY(!Lan::Offer::isLocalOfferUrl(
        QUrl("https://192.168.1.4:32768/instance/0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), &error));
    QVERIFY(!Lan::Offer::isLocalOfferUrl(QUrl("http://192.168.1.4:32768/instance/not-a-capability"), &error));
}

QTEST_GUILESS_MAIN(LanOfferTest)

#include "LanOffer_test.moc"
