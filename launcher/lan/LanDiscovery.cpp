// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanDiscovery.h"

#include "lan/LanOffer.h"

#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>

#include <utility>

namespace {

constexpr quint16 g_discoveryPort = 38527;
constexpr auto g_protocol = "org.prismlauncher.instance-share.v1";
constexpr qsizetype g_maxDatagramBytes = 4096;
constexpr qsizetype g_maxInstanceNameBytes = 256;
constexpr qsizetype g_maxUrls = 16;

bool isPrivateIpv4(const QHostAddress& address)
{
    return address.protocol() == QAbstractSocket::IPv4Protocol &&
           (address.isInSubnet(QHostAddress("10.0.0.0"), 8) || address.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
            address.isInSubnet(QHostAddress("192.168.0.0"), 16) || address.isInSubnet(QHostAddress("169.254.0.0"), 16));
}

}  // namespace

namespace Lan {

QByteArray makeDiscoveryDatagram(const QString& instanceName, const QList<QUrl>& urls)
{
    if (instanceName.isEmpty() || instanceName.toUtf8().size() > g_maxInstanceNameBytes || urls.isEmpty() || urls.size() > g_maxUrls) {
        return {};
    }

    QJsonArray urlArray;
    for (const auto& url : urls) {
        QString error;
        if (!Offer::isLocalOfferUrl(url, &error)) {
            return {};
        }
        urlArray.append(url.toString(QUrl::FullyEncoded));
    }

    QJsonObject object;
    object.insert(QStringLiteral("protocol"), QString::fromLatin1(g_protocol));
    object.insert(QStringLiteral("instance"), instanceName);
    object.insert(QStringLiteral("urls"), urlArray);
    const auto datagram = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return datagram.size() <= g_maxDatagramBytes ? datagram : QByteArray();
}

QList<DiscoveredOffer> parseDiscoveryDatagram(const QByteArray& datagram, const QHostAddress& sender)
{
    QList<DiscoveredOffer> offers;
    if (datagram.isEmpty() || datagram.size() > g_maxDatagramBytes || !isPrivateIpv4(sender)) {
        return offers;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(datagram, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return offers;
    }
    const auto object = document.object();
    if (object.value(QStringLiteral("protocol")).toString() != QString::fromLatin1(g_protocol)) {
        return offers;
    }
    const auto instanceName = object.value(QStringLiteral("instance")).toString();
    const auto urls = object.value(QStringLiteral("urls")).toArray();
    if (instanceName.isEmpty() || instanceName.toUtf8().size() > g_maxInstanceNameBytes || urls.isEmpty() || urls.size() > g_maxUrls) {
        return offers;
    }

    for (const auto& value : urls) {
        if (!value.isString()) {
            return {};
        }
        const auto url = QUrl(value.toString());
        QString error;
        if (!Offer::isLocalOfferUrl(url, &error)) {
            return {};
        }
        offers.append({ instanceName, url });
    }
    return offers;
}

class Advertiser::Private {
   public:
    QUdpSocket socket;
    QTimer timer;
    QString instanceName;
    QList<QUrl> urls;
};

Advertiser::Advertiser() : m_private(std::make_unique<Private>())
{
    m_private->timer.setInterval(1000);
    connect(&m_private->timer, &QTimer::timeout, this, &Advertiser::announce);
}

Advertiser::~Advertiser()
{
    stop();
}

bool Advertiser::start(const QString& instanceName, const QList<QUrl>& urls, QString* error)
{
    const auto datagram = makeDiscoveryDatagram(instanceName, urls);
    if (datagram.isEmpty()) {
        *error = tr("The LAN share announcement is invalid.");
        return false;
    }
    stop();
    if (!m_private->socket.bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        *error = tr("Could not open the LAN discovery socket.");
        return false;
    }
    m_private->instanceName = instanceName;
    m_private->urls = urls;
    m_private->timer.start();
    announce();
    return true;
}

void Advertiser::stop()
{
    m_private->timer.stop();
    m_private->socket.close();
    m_private->instanceName.clear();
    m_private->urls.clear();
}

void Advertiser::announce()
{
    const auto datagram = makeDiscoveryDatagram(m_private->instanceName, m_private->urls);
    if (!datagram.isEmpty()) {
        m_private->socket.writeDatagram(datagram, QHostAddress::Broadcast, g_discoveryPort);
    }
}

class Browser::Private {
   public:
    QUdpSocket socket;
    QSet<QString> seenUrls;
};

Browser::Browser() : m_private(std::make_unique<Private>())
{
    connect(&m_private->socket, &QUdpSocket::readyRead, this, &Browser::receiveDatagrams);
}

Browser::~Browser()
{
    stop();
}

bool Browser::start(QString* error)
{
    stop();
    if (!m_private->socket.bind(QHostAddress::AnyIPv4, g_discoveryPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        *error = tr("Could not listen for LAN instance announcements.");
        return false;
    }
    return true;
}

void Browser::stop()
{
    m_private->socket.close();
}

void Browser::clearOffers()
{
    m_private->seenUrls.clear();
}

void Browser::receiveDatagrams()
{
    while (m_private->socket.hasPendingDatagrams()) {
        const auto datagram = m_private->socket.receiveDatagram();
        for (const auto& offer : parseDiscoveryDatagram(datagram.data(), datagram.senderAddress())) {
            const auto key = offer.url.toString(QUrl::FullyEncoded);
            if (m_private->seenUrls.contains(key)) {
                continue;
            }
            m_private->seenUrls.insert(key);
            emit offerDiscovered(offer);
        }
    }
}

}  // namespace Lan
