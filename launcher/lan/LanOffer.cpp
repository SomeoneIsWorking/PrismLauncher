// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanOffer.h"

#include "lan/LanNetwork.h"
#include "lucent/http.h"

#include <QFileInfo>
#include <QList>
#include <QObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QString>
#include <QUrl>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace {

constexpr auto g_offerPathPrefix = "/instance/";
constexpr auto g_capabilityHexLength = 64;

QString randomCapability()
{
    const auto randomHex = [](quint64 value) { return QString::number(value, 16).rightJustified(16, QChar('0')); };
    return randomHex(QRandomGenerator::system()->generate64()) + randomHex(QRandomGenerator::system()->generate64()) +
           randomHex(QRandomGenerator::system()->generate64()) + randomHex(QRandomGenerator::system()->generate64());
}

bool constantTimeEquals(std::string_view left, std::string_view right)
{
    if (left.size() != right.size()) {
        return false;
    }
    std::uint8_t difference = 0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        difference += static_cast<std::uint8_t>(left.at(index) != right.at(index));
    }
    return difference == 0;
}

}  // namespace

namespace Lan {

Offer::Offer() = default;

Offer::~Offer()
{
    stop();
}

bool Offer::start(const QString& archivePath, QString* error)
{
    const QFileInfo archive(archivePath);
    if (!archive.isFile() || !archive.isReadable()) {
        *error = QObject::tr("The shared archive does not exist.");
        return false;
    }
    if (privateLanAddresses().isEmpty()) {
        *error = QObject::tr("No private IPv4 local-network address is available for sharing.");
        return false;
    }

    stop();
    m_archivePath = archivePath;
    m_capability = randomCapability();
    const std::string expectedPath = (QString::fromLatin1(g_offerPathPrefix) + m_capability).toStdString();
    const std::string filePath = m_archivePath.toStdString();
    lucent::http::ServerOptions options;
    options.listen_scope = lucent::http::ListenScope::LocalNetwork;
    options.max_body_bytes = 0;
    options.max_connections = 2;
    m_server = std::make_unique<lucent::http::Server>(options, [expectedPath, filePath](const lucent::http::Request& request) {
        if (request.method != "GET" || !constantTimeEquals(request.path(), expectedPath)) {
            return lucent::http::Response::text(404, "Not Found", "offer not found\n");
        }
        return lucent::http::Response::file(200, "OK", "application/zip", filePath);
    });
    if (!m_server->start()) {
        m_server.reset();
        m_capability.clear();
        m_archivePath.clear();
        *error = QObject::tr("Could not start the local-network share. Check whether a firewall is blocking Prism Launcher.");
        return false;
    }
    return true;
}

void Offer::stop()
{
    if (m_server) {
        m_server->stop();
        m_server.reset();
    }
    m_capability.clear();
    m_archivePath.clear();
}

bool Offer::isSharing() const
{
    return m_server != nullptr && m_server->running();
}

QList<QUrl> Offer::urls() const
{
    QList<QUrl> urls;
    if (!isSharing()) {
        return urls;
    }
    for (const auto& address : privateLanAddresses()) {
        QUrl url;
        url.setScheme("http");
        url.setHost(address.toString());
        url.setPort(m_server->port());
        url.setPath(QString::fromLatin1(g_offerPathPrefix) + m_capability);
        urls.append(url);
    }
    return urls;
}

bool Offer::isLocalOfferUrl(const QUrl& url, QString* error)
{
    const QHostAddress address(url.host());
    const QString token = url.path().mid(QString::fromLatin1(g_offerPathPrefix).size());
    const bool valid = url.isValid() && url.scheme() == "http" && url.userInfo().isEmpty() && url.query().isEmpty() &&
                       url.fragment().isEmpty() && url.port() > 0 && isPrivateIpv4(address) &&
                       url.path().startsWith(QString::fromLatin1(g_offerPathPrefix)) &&
                       QRegularExpression(QStringLiteral("^[0-9a-f]{%1}$").arg(g_capabilityHexLength)).match(token).hasMatch();
    if (!valid) {
        *error = QObject::tr("The LAN share link must be an http URL to a private IPv4 address with a valid offer capability.");
    }
    return valid;
}

}  // namespace Lan
