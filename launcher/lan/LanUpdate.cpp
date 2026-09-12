// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanUpdate.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <system_error>

namespace {

namespace Fs = std::filesystem;

Fs::path nativePath(const QString& path)
{
#ifdef Q_OS_WIN
    return { path.toStdWString() };
#else
    return { path.toStdString() };
#endif
}

QString filesystemError(const QString& action, const std::error_code& error)
{
    return QStringLiteral("%1: %2").arg(action, QString::fromStdString(error.message()));
}

QString gameDirectory(const QString& root)
{
    QString dotted = QDir(root).filePath(QStringLiteral(".minecraft"));
    QString ordinary = QDir(root).filePath(QStringLiteral("minecraft"));
    return QFileInfo(dotted).exists() && !QFileInfo(ordinary).exists() ? dotted : ordinary;
}

bool replaceWithLocal(const QString& local, const QString& staged, QString* error)
{
    std::error_code failure;
    Fs::remove_all(nativePath(staged), failure);
    if (failure) {
        *error = filesystemError(QStringLiteral("Could not remove sender data before preserving local data"), failure);
        return false;
    }

    QFileInfo source(local);
    if (!source.exists() && !source.isSymLink()) {
        return true;
    }
    if (source.isSymLink()) {
        *error = QStringLiteral("Local player data contains a symbolic link: %1").arg(local);
        return false;
    }

    Fs::create_directories(nativePath(QFileInfo(staged).absolutePath()), failure);
    if (!failure) {
        Fs::copy(nativePath(local), nativePath(staged), Fs::copy_options::recursive | Fs::copy_options::copy_symlinks, failure);
    }
    if (failure) {
        *error = filesystemError(QStringLiteral("Could not preserve local player data"), failure);
        return false;
    }
    return true;
}

}  // namespace

namespace Lan {

bool prepareInstanceUpdate(const QString& existingRoot, const QString& stagedRoot, QString* error)
{
    if (error == nullptr || !QFileInfo(existingRoot).isDir() || QFileInfo(existingRoot).isSymLink() ||
        !QFileInfo(QDir(existingRoot).filePath(QStringLiteral("instance.cfg"))).isFile() || !QFileInfo(stagedRoot).isDir() ||
        QFileInfo(stagedRoot).isSymLink() || !QFileInfo(QDir(stagedRoot).filePath(QStringLiteral("instance.cfg"))).isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("The local instance or downloaded instance is incomplete.");
        }
        return false;
    }

    if (!replaceWithLocal(QDir(existingRoot).filePath(QStringLiteral("instance.cfg")),
                          QDir(stagedRoot).filePath(QStringLiteral("instance.cfg")), error)) {
        return false;
    }

    QString localGame = gameDirectory(existingRoot);
    QString stagedGame = gameDirectory(stagedRoot);
    if (QFileInfo(localGame).isSymLink() || QFileInfo(stagedGame).isSymLink()) {
        *error = QStringLiteral("An instance game directory is a symbolic link and cannot be updated safely.");
        return false;
    }
    return std::ranges::all_of(
        std::initializer_list<const char*>{ "saves", "screenshots", "resourcepacks", "texturepacks", "shaderpacks", "options.txt",
                                            "optionsof.txt", "servers.dat", "servers.dat_old", "usercache.json", "usernamecache.json" },
        [&](const char* name) {
            return replaceWithLocal(QDir(localGame).filePath(QString::fromLatin1(name)),
                                    QDir(stagedGame).filePath(QString::fromLatin1(name)), error);
        });
}

bool commitInstanceUpdate(const QString& existingRoot, const QString& stagedRoot, QString* error)
{
    if (error == nullptr || !QFileInfo(existingRoot).isDir() || QFileInfo(existingRoot).isSymLink() || !QFileInfo(stagedRoot).isDir() ||
        QFileInfo(stagedRoot).isSymLink() || !QFileInfo(QDir(stagedRoot).filePath(QStringLiteral("instance.cfg"))).isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("The local instance or staged update has disappeared.");
        }
        return false;
    }

    QString instancesRoot = QFileInfo(existingRoot).absolutePath();
    if (QFileInfo(stagedRoot).absolutePath() != QDir(instancesRoot).filePath(QStringLiteral(".tmp"))) {
        *error = QStringLiteral("The staged update is outside the instance staging directory.");
        return false;
    }

    QString backup = QDir(instancesRoot).filePath(QStringLiteral(".tmp/lan-backup-%1").arg(QUuid::createUuid().toString(QUuid::Id128)));
    std::error_code failure;
    Fs::rename(nativePath(existingRoot), nativePath(backup), failure);
    if (failure) {
        *error = filesystemError(QStringLiteral("Could not back up the local instance"), failure);
        return false;
    }

    Fs::rename(nativePath(stagedRoot), nativePath(existingRoot), failure);
    if (failure) {
        std::error_code restoreFailure;
        Fs::rename(nativePath(backup), nativePath(existingRoot), restoreFailure);
        *error = filesystemError(QStringLiteral("Could not install the staged update"), failure);
        if (restoreFailure) {
            *error +=
                QStringLiteral("; restore the previous instance from %1: %2").arg(backup, QString::fromStdString(restoreFailure.message()));
        }
        return false;
    }

    Fs::remove_all(nativePath(backup), failure);
    if (failure) {
        qWarning() << "LAN update succeeded, but its temporary backup could not be removed:" << backup
                   << QString::fromStdString(failure.message());
    }
    return true;
}

}  // namespace Lan
