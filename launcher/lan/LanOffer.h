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
class Advertiser;

// Owns one direct, capability-protected HTTP offer for one archive. Sharing
// is explicitly local-network scoped and announces itself only while active.
class Offer final {
   public:
    Offer();
    ~Offer();

    Offer(const Offer&) = delete;
    Offer& operator=(const Offer&) = delete;

    bool start(const QString& archivePath, const QString& instanceName, QString* error);
    void stop();

    bool isSharing() const;
    QList<QUrl> urls() const;

    static bool isLocalOfferUrl(const QUrl& url, QString* error);

   private:
    QString m_archivePath;
    QString m_capability;
    std::unique_ptr<lucent::http::Server> m_server;
    std::unique_ptr<Advertiser> m_discovery;
};

}  // namespace Lan
