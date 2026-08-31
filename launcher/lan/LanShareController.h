// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "QObjectPtr.h"
#include "lan/LanOffer.h"

#include <QList>
#include <QString>
#include <QUrl>

#include <memory>

class BaseInstance;
class QTemporaryDir;

namespace MMCZip {
class ExportToZipTask;
}

namespace Lan {

// Owns one explicit, capability-protected local-network offer. It creates the
// archive with Prism's existing ZIP writer and leaves archive import to
// InstanceImportTask on the receiving launcher.
class ShareController final {
   public:
    explicit ShareController(BaseInstance* instance);
    ~ShareController();

    ShareController(const ShareController&) = delete;
    ShareController& operator=(const ShareController&) = delete;

    shared_qobject_ptr<MMCZip::ExportToZipTask> createArchiveTask(QString* error);
    bool start(QString* error);
    void stop();

    bool isSharing() const;
    QList<QUrl> offerUrls() const;

    static bool isLocalOfferUrl(const QUrl& url, QString* error) { return Offer::isLocalOfferUrl(url, error); }

   private:
    BaseInstance* m_instance;
    std::unique_ptr<QTemporaryDir> m_temporaryDirectory;
    QString m_archivePath;
    Offer m_offer;
};

}  // namespace Lan
