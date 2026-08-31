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

// Owns one direct, capability-protected HTTP offer for one archive. Sharing
// is explicitly local-network scoped and has no discovery channel.
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
