// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QList>
#include <QString>
#include <QUrl>

#include <memory>

namespace lucent::http {
class Server;
}

namespace Lan {

// Owns one short-lived, capability-protected HTTP endpoint for one archive.
// Discovery and archive lifetime belong to InstanceService.
class Offer final {
   public:
    Offer();
    ~Offer();

    Offer(const Offer&) = delete;
    Offer& operator=(const Offer&) = delete;

    bool start(const QString& archivePath, QString* error);
    void stop();

    bool isSharing() const;
    QList<QUrl> urls() const;

    static bool isLocalOfferUrl(const QUrl& url, QString* error);

   private:
    QString m_archivePath;
    QString m_capability;
    std::unique_ptr<lucent::http::Server> m_server;
};

}  // namespace Lan
