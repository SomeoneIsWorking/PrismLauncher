// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace Lan {

struct DiscoveredOffer {
    QString instanceName;
    QUrl url;
};

QByteArray makeDiscoveryDatagram(const QString& instanceName, const QList<QUrl>& urls);
QList<DiscoveredOffer> parseDiscoveryDatagram(const QByteArray& datagram, const QHostAddress& sender);

class Advertiser final : public QObject {
    Q_OBJECT

   public:
    Advertiser();
    ~Advertiser() override;

    Advertiser(const Advertiser&) = delete;
    Advertiser& operator=(const Advertiser&) = delete;

    bool start(const QString& instanceName, const QList<QUrl>& urls, QString* error);
    void stop();

   private slots:
    void announce();

   private:
    class Private;
    std::unique_ptr<Private> m_private;
};

class Browser final : public QObject {
    Q_OBJECT

   public:
    Browser();
    ~Browser() override;

    Browser(const Browser&) = delete;
    Browser& operator=(const Browser&) = delete;

    bool start(QString* error);
    void stop();
    void clearOffers();

   signals:
    void offerDiscovered(const Lan::DiscoveredOffer& offer);

   private slots:
    void receiveDatagrams();

   private:
    class Private;
    std::unique_ptr<Private> m_private;
};

}  // namespace Lan
