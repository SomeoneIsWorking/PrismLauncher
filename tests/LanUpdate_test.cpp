// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanUpdate.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool writeFile(const QString& path, const QByteArray& content)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

}  // namespace

class LanUpdateTest : public QObject {
    Q_OBJECT

   private slots:
    void replacesPackFilesAndKeepsPlayerData();
    void refusesIncompleteUpdateWithoutChangingLocalInstance();
};

void LanUpdateTest::replacesPackFilesAndKeepsPlayerData()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QString instances = root.filePath(QStringLiteral("instances"));
    QString existing = QDir(instances).filePath(QStringLiteral("My Pack"));
    QString staged = QDir(instances).filePath(QStringLiteral(".tmp/staged"));

    QVERIFY(writeFile(QDir(existing).filePath(QStringLiteral("instance.cfg")), "local settings"));
    QVERIFY(writeFile(QDir(existing).filePath(QStringLiteral("minecraft/mods/old.jar")), "old mod"));
    QVERIFY(writeFile(QDir(existing).filePath(QStringLiteral("minecraft/saves/world/level.dat")), "local world"));
    QVERIFY(writeFile(QDir(existing).filePath(QStringLiteral("minecraft/options.txt")), "local options"));
    QVERIFY(writeFile(QDir(staged).filePath(QStringLiteral("instance.cfg")), "sender settings"));
    QVERIFY(writeFile(QDir(staged).filePath(QStringLiteral("minecraft/mods/new.jar")), "new mod"));
    QVERIFY(writeFile(QDir(staged).filePath(QStringLiteral("minecraft/saves/sender/level.dat")), "sender world"));
    QVERIFY(writeFile(QDir(staged).filePath(QStringLiteral("minecraft/options.txt")), "sender options"));

    QString error;
    QVERIFY2(Lan::prepareInstanceUpdate(existing, staged, &error), qPrintable(error));
    QVERIFY2(Lan::commitInstanceUpdate(existing, staged, &error), qPrintable(error));
    QCOMPARE(readFile(QDir(existing).filePath(QStringLiteral("instance.cfg"))), QByteArray("local settings"));
    QCOMPARE(readFile(QDir(existing).filePath(QStringLiteral("minecraft/mods/new.jar"))), QByteArray("new mod"));
    QVERIFY(!QFileInfo::exists(QDir(existing).filePath(QStringLiteral("minecraft/mods/old.jar"))));
    QCOMPARE(readFile(QDir(existing).filePath(QStringLiteral("minecraft/saves/world/level.dat"))), QByteArray("local world"));
    QVERIFY(!QFileInfo::exists(QDir(existing).filePath(QStringLiteral("minecraft/saves/sender/level.dat"))));
    QCOMPARE(readFile(QDir(existing).filePath(QStringLiteral("minecraft/options.txt"))), QByteArray("local options"));
}

void LanUpdateTest::refusesIncompleteUpdateWithoutChangingLocalInstance()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QString instances = root.filePath(QStringLiteral("instances"));
    QString existing = QDir(instances).filePath(QStringLiteral("Local"));
    QString staged = QDir(instances).filePath(QStringLiteral(".tmp/staged"));
    QVERIFY(writeFile(QDir(existing).filePath(QStringLiteral("instance.cfg")), "local settings"));
    QVERIFY(QDir().mkpath(staged));

    QString error;
    QVERIFY(!Lan::prepareInstanceUpdate(existing, staged, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(readFile(QDir(existing).filePath(QStringLiteral("instance.cfg"))), QByteArray("local settings"));
    QVERIFY(!Lan::commitInstanceUpdate(existing, staged, &error));
    QCOMPARE(readFile(QDir(existing).filePath(QStringLiteral("instance.cfg"))), QByteArray("local settings"));
}

QTEST_GUILESS_MAIN(LanUpdateTest)

#include "LanUpdate_test.moc"
