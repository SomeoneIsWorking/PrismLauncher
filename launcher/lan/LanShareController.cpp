// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanShareController.h"

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "archive/ExportToZipTask.h"
#include "archive/InstanceArchive.h"

#include <QFileInfo>
#include <QObject>
#include <QTemporaryDir>

namespace Lan {

ShareController::ShareController(BaseInstance* instance) : m_instance(instance) {}

ShareController::~ShareController()
{
    stop();
}

shared_qobject_ptr<MMCZip::ExportToZipTask> ShareController::createArchiveTask(QString* error)
{
    if (m_instance == nullptr) {
        *error = QObject::tr("No instance is selected.");
        return shared_qobject_ptr<MMCZip::ExportToZipTask>();
    }
    if (m_instance->isRunning()) {
        *error = QObject::tr("Stop the selected instance before sharing it.");
        return shared_qobject_ptr<MMCZip::ExportToZipTask>();
    }

    stop();
    const auto offersRoot = FS::PathCombine(APPLICATION->dataRoot(), "cache", "lan-share");
    if (!FS::ensureFolderPathExists(offersRoot)) {
        *error = QObject::tr("Could not create Prism's local share cache.");
        return shared_qobject_ptr<MMCZip::ExportToZipTask>();
    }
    m_temporaryDirectory = std::make_unique<QTemporaryDir>(FS::PathCombine(offersRoot, "offer-XXXXXX"));
    if (!m_temporaryDirectory->isValid()) {
        *error = QObject::tr("Could not create a temporary directory for the shared archive.");
        return shared_qobject_ptr<MMCZip::ExportToZipTask>();
    }

    MMCZip::saveInstanceIcon(m_instance);
    QFileInfoList files;
    if (!MMCZip::collectFileListRecursively(m_instance->instanceRoot(), nullptr, &files, {})) {
        *error = QObject::tr("Could not collect the selected instance for sharing.");
        m_temporaryDirectory.reset();
        return shared_qobject_ptr<MMCZip::ExportToZipTask>();
    }

    m_archivePath = m_temporaryDirectory->filePath("shared-instance.zip");
    return makeShared<MMCZip::ExportToZipTask>(m_archivePath, m_instance->instanceRoot(), files, "", true);
}

bool ShareController::start(QString* error)
{
    if (m_archivePath.isEmpty() || !QFileInfo::exists(m_archivePath)) {
        *error = QObject::tr("Create the shared archive before starting an offer.");
        return false;
    }

    return m_offer.start(m_archivePath, m_instance->name(), error);
}

void ShareController::stop()
{
    m_offer.stop();
}

bool ShareController::isSharing() const
{
    return m_offer.isSharing();
}

QList<QUrl> ShareController::offerUrls() const
{
    return m_offer.urls();
}

}  // namespace Lan
