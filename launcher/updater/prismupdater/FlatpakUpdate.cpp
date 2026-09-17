// SPDX-License-Identifier: GPL-3.0-only

#include "FlatpakUpdate.h"

#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

#include <utility>

namespace FlatpakUpdate {

QString assetName(const QString& version, const QString& architecture)
{
    QString arch;
    if (architecture == "x86_64" || architecture == "amd64") {
        arch = "x86_64";
    } else if (architecture == "aarch64" || architecture == "arm64") {
        arch = "aarch64";
    } else {
        return {};
    }
    return QStringLiteral("PrismLauncher-%1-%2.flatpak").arg(version, arch);
}

QString bundleProblem(const GitHubReleaseAsset& asset, const QFileInfo& bundle, const QString& repository, const QString& version)
{
    if (!bundle.isFile() || bundle.size() != asset.size || asset.size <= 0 || asset.size > 500 * 1024 * 1024) {
        return QStringLiteral("Flatpak bundle size does not match the release asset.");
    }
    const auto expectedUrl = QStringLiteral("%1/releases/download/%2/%3").arg(repository, version, asset.name);
    if (asset.browser_download_url != expectedUrl || !repository.startsWith("https://github.com/")) {
        return QStringLiteral("Flatpak bundle URL is outside the configured release repository.");
    }
    static const QRegularExpression s_digestPattern(QStringLiteral("^sha256:[0-9a-f]{64}$"));
    if (!s_digestPattern.match(asset.digest).hasMatch()) {
        return QStringLiteral("Flatpak release is missing a SHA-256 digest.");
    }
    QFile input(bundle.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly)) {
        return QStringLiteral("Flatpak bundle could not be opened: %1").arg(input.errorString());
    }
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    if (!hasher.addData(&input) || QString::fromLatin1(hasher.result().toHex()) != asset.digest.mid(7)) {
        return QStringLiteral("Flatpak bundle SHA-256 does not match the release asset.");
    }
    return {};
}

namespace {

struct HostResult {
    bool succeeded;
    QString output;

    HostResult(bool succeeded, QString output) : succeeded(succeeded), output(std::move(output)) {}
};

HostResult runHostFlatpak(const QStringList& arguments)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    QStringList hostArguments = { "--host", "flatpak" };
    hostArguments.append(arguments);
    process.start(QStringLiteral("flatpak-spawn"), hostArguments);
    if (!process.waitForStarted(5000)) {
        return { false, process.errorString() };
    }
    if (!process.waitForFinished(120000)) {
        process.kill();
        process.waitForFinished();
        return { false, QStringLiteral("Host Flatpak command timed out") };
    }
    return { process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0, QString::fromLocal8Bit(process.readAll()).trimmed() };
}

}  // namespace

QString installBundle(const QString& bundlePath, const QString& repositoryPath)
{
    constexpr auto appId = "org.prismlauncher.PrismLauncher";
    constexpr auto remote = "prism-fork-local";
    const auto expectedUrl = QUrl::fromLocalFile(repositoryPath).toString();
    const auto remotes = runHostFlatpak({ "remotes", "--user", "--columns=name,url" });
    if (!remotes.succeeded || !remotes.output.split('\n').contains(QStringLiteral("%1\t%2").arg(remote, expectedUrl))) {
        return QStringLiteral("The fork Flatpak repository is missing or points elsewhere: %1").arg(expectedUrl);
    }
    const auto origin = runHostFlatpak({ "info", "--user", "--show-origin", appId });
    if (!origin.succeeded || origin.output != remote) {
        return QStringLiteral(
            "This Flatpak is not installed from the fork repository. Reinstall it from the fork repository before updating.");
    }
    const auto imported = runHostFlatpak({ "build-import-bundle", repositoryPath, bundlePath });
    if (!imported.succeeded) {
        return QStringLiteral("Could not import the verified bundle: %1").arg(imported.output);
    }
    const auto finalized = runHostFlatpak({ "build-update-repo", repositoryPath });
    if (!finalized.succeeded) {
        return QStringLiteral("Could not publish the fork Flatpak repository metadata: %1").arg(finalized.output);
    }
    const auto available = runHostFlatpak({ "remote-info", "--user", "--show-commit", remote, appId });
    if (!available.succeeded || available.output.isEmpty()) {
        return QStringLiteral("Could not read the imported Flatpak commit: %1").arg(available.output);
    }
    const auto update = runHostFlatpak({ "update", "--user", "--noninteractive", appId });
    if (!update.succeeded) {
        return QStringLiteral("Host Flatpak update failed: %1").arg(update.output);
    }
    const auto installed = runHostFlatpak({ "info", "--user", "--show-commit", appId });
    if (!installed.succeeded || installed.output != available.output) {
        return QStringLiteral("Flatpak did not activate the imported release. Check whether this application is masked.");
    }
    return {};
}

}  // namespace FlatpakUpdate
