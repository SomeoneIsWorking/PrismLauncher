// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QFileInfo>
#include <QString>

#include "updater/prismupdater/GitHubRelease.h"

namespace FlatpakUpdate {

QString assetName(const QString& version, const QString& architecture);
QString bundleProblem(const GitHubReleaseAsset& asset, const QFileInfo& bundle, const QString& repository, const QString& version);
QString installBundle(const QString& bundlePath, const QString& repositoryPath);

}  // namespace FlatpakUpdate
