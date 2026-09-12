// SPDX-License-Identifier: GPL-3.0-only

#include "updater/prismupdater/FlatpakUpdate.h"

#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class FlatpakUpdateTest : public QObject {
    Q_OBJECT

   private slots:
    void selectsExactArchitectureAsset();
    void verifiesBundleAndRejectsAlteredInputs();
};

void FlatpakUpdateTest::selectsExactArchitectureAsset()
{
    QCOMPARE(FlatpakUpdate::assetName("12.0.8", "x86_64"), QStringLiteral("PrismLauncher-12.0.8-x86_64.flatpak"));
    QCOMPARE(FlatpakUpdate::assetName("12.0.8", "arm64"), QStringLiteral("PrismLauncher-12.0.8-aarch64.flatpak"));
    QVERIFY(FlatpakUpdate::assetName("12.0.8", "unknown").isEmpty());
}

void FlatpakUpdateTest::verifiesBundleAndRejectsAlteredInputs()
{
    const QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("PrismLauncher-12.0.8-x86_64.flatpak");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray content("verified bundle");
    QCOMPARE(file.write(content), content.size());
    file.close();

    GitHubReleaseAsset asset;
    asset.name = QFileInfo(path).fileName();
    asset.size = static_cast<int>(content.size());
    asset.browser_download_url = "https://github.com/SomeoneIsWorking/PrismLauncher/releases/download/12.0.8/" + asset.name;
    asset.digest = "sha256:" + QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());

    const auto repository = QStringLiteral("https://github.com/SomeoneIsWorking/PrismLauncher");
    const auto version = QStringLiteral("12.0.8");
    QVERIFY(FlatpakUpdate::bundleProblem(asset, QFileInfo(path), repository, version).isEmpty());

    asset.digest = "sha256:" + QString(64, '0');
    QVERIFY(FlatpakUpdate::bundleProblem(asset, QFileInfo(path), repository, version).contains("SHA-256"));
    asset.digest.clear();
    QVERIFY(FlatpakUpdate::bundleProblem(asset, QFileInfo(path), repository, version).contains("missing"));
    asset.browser_download_url = "https://github.com/other/PrismLauncher/releases/download/12.0.8/" + asset.name;
    QVERIFY(FlatpakUpdate::bundleProblem(asset, QFileInfo(path), repository, version).contains("outside"));
    asset.size++;
    QVERIFY(FlatpakUpdate::bundleProblem(asset, QFileInfo(path), repository, version).contains("size"));
}

QTEST_GUILESS_MAIN(FlatpakUpdateTest)

#include "FlatpakUpdate_test.moc"
