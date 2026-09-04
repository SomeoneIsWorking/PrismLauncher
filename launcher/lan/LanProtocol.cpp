// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanProtocol.h"

#include "lan/LanNetwork.h"
#include "lan/LanOffer.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <cmath>

namespace {

constexpr auto g_protocol = "org.prismlauncher.instance-catalog.v2";
constexpr qsizetype g_maxDatagramBytes = 4096;
constexpr qsizetype g_maxInstanceNameBytes = 256;
constexpr qsizetype g_maxFailureBytes = 512;
constexpr int g_idHexLength = 32;
constexpr int g_instanceIdHexLength = 64;

bool isHexId(const QString& value, int length)
{
    return QRegularExpression(QStringLiteral("^[0-9a-f]{%1}$").arg(length)).match(value).hasMatch();
}

QJsonObject messageObject(const QString& type)
{
    QJsonObject object;
    object.insert(QStringLiteral("protocol"), QString::fromLatin1(g_protocol));
    object.insert(QStringLiteral("type"), type);
    return object;
}

QByteArray encode(const QJsonObject& object)
{
    const auto datagram = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return datagram.size() <= g_maxDatagramBytes ? datagram : QByteArray();
}

}  // namespace

namespace Lan {

QString randomId()
{
    const auto randomHex = [](quint64 value) { return QString::number(value, 16).rightJustified(16, QChar('0')); };
    return randomHex(QRandomGenerator::system()->generate64()) + randomHex(QRandomGenerator::system()->generate64());
}

QByteArray makeQueryDatagram()
{
    return encode(messageObject(QStringLiteral("query")));
}

QByteArray makeAnnouncementDatagram(const Announcement& announcement)
{
    if (!isHexId(announcement.serviceId, g_idHexLength) || !isHexId(announcement.instanceId, g_instanceIdHexLength) ||
        announcement.instanceName.isEmpty() || announcement.instanceName.toUtf8().size() > g_maxInstanceNameBytes) {
        return {};
    }
    auto object = messageObject(QStringLiteral("instance"));
    object.insert(QStringLiteral("service"), announcement.serviceId);
    object.insert(QStringLiteral("instance"), announcement.instanceId);
    object.insert(QStringLiteral("name"), announcement.instanceName);
    object.insert(QStringLiteral("available"), announcement.available);
    return encode(object);
}

QByteArray makeImportRequestDatagram(const ImportRequest& request)
{
    if (!isHexId(request.serviceId, g_idHexLength) || !isHexId(request.instanceId, g_instanceIdHexLength) ||
        !isHexId(request.requestId, g_idHexLength)) {
        return {};
    }
    auto object = messageObject(QStringLiteral("request"));
    object.insert(QStringLiteral("service"), request.serviceId);
    object.insert(QStringLiteral("instance"), request.instanceId);
    object.insert(QStringLiteral("request"), request.requestId);
    return encode(object);
}

QByteArray makeCancelRequestDatagram(const CancelRequest& request)
{
    if (!isHexId(request.serviceId, g_idHexLength) || !isHexId(request.requestId, g_idHexLength)) {
        return {};
    }
    auto object = messageObject(QStringLiteral("cancel"));
    object.insert(QStringLiteral("service"), request.serviceId);
    object.insert(QStringLiteral("request"), request.requestId);
    return encode(object);
}

QByteArray makeTransferReadyDatagram(const QString& requestId, const QUrl& url)
{
    QString error;
    if (!isHexId(requestId, g_idHexLength) || !Offer::isLocalOfferUrl(url, &error)) {
        return {};
    }
    auto object = messageObject(QStringLiteral("ready"));
    object.insert(QStringLiteral("request"), requestId);
    object.insert(QStringLiteral("port"), url.port());
    object.insert(QStringLiteral("capability"), url.path().section('/', -1));
    return encode(object);
}

QByteArray makeTransferFailureDatagram(const TransferFailure& failure)
{
    if (!isHexId(failure.requestId, g_idHexLength) || failure.reason.isEmpty() || failure.reason.toUtf8().size() > g_maxFailureBytes) {
        return {};
    }
    auto object = messageObject(QStringLiteral("failure"));
    object.insert(QStringLiteral("request"), failure.requestId);
    object.insert(QStringLiteral("reason"), failure.reason);
    return encode(object);
}

std::optional<Message> parseDatagram(const QByteArray& datagram, const QHostAddress& sender)
{
    if (datagram.isEmpty() || datagram.size() > g_maxDatagramBytes || !isPrivateIpv4(sender)) {
        return std::nullopt;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(datagram, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const auto object = document.object();
    if (object.value(QStringLiteral("protocol")).toString() != QString::fromLatin1(g_protocol)) {
        return std::nullopt;
    }
    const auto type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("query")) {
        return Message(Query{});
    }
    if (type == QStringLiteral("instance")) {
        if (!object.value(QStringLiteral("service")).isString() || !object.value(QStringLiteral("instance")).isString() ||
            !object.value(QStringLiteral("name")).isString() || !object.value(QStringLiteral("available")).isBool()) {
            return std::nullopt;
        }
        Announcement announcement{ .serviceId = object.value(QStringLiteral("service")).toString(),
                                   .instanceId = object.value(QStringLiteral("instance")).toString(),
                                   .instanceName = object.value(QStringLiteral("name")).toString(),
                                   .available = object.value(QStringLiteral("available")).toBool() };
        if (makeAnnouncementDatagram(announcement).isEmpty()) {
            return std::nullopt;
        }
        return Message(std::move(announcement));
    }
    if (type == QStringLiteral("request")) {
        if (!object.value(QStringLiteral("service")).isString() || !object.value(QStringLiteral("instance")).isString() ||
            !object.value(QStringLiteral("request")).isString()) {
            return std::nullopt;
        }
        ImportRequest request{ .serviceId = object.value(QStringLiteral("service")).toString(),
                               .instanceId = object.value(QStringLiteral("instance")).toString(),
                               .requestId = object.value(QStringLiteral("request")).toString() };
        if (makeImportRequestDatagram(request).isEmpty()) {
            return std::nullopt;
        }
        return Message(std::move(request));
    }
    if (type == QStringLiteral("cancel")) {
        if (!object.value(QStringLiteral("service")).isString() || !object.value(QStringLiteral("request")).isString()) {
            return std::nullopt;
        }
        CancelRequest request{ .serviceId = object.value(QStringLiteral("service")).toString(),
                               .requestId = object.value(QStringLiteral("request")).toString() };
        if (makeCancelRequestDatagram(request).isEmpty()) {
            return std::nullopt;
        }
        return Message(std::move(request));
    }
    if (type == QStringLiteral("ready")) {
        if (!object.value(QStringLiteral("request")).isString() || !object.value(QStringLiteral("capability")).isString() ||
            !object.value(QStringLiteral("port")).isDouble()) {
            return std::nullopt;
        }
        const auto requestId = object.value(QStringLiteral("request")).toString();
        const auto capability = object.value(QStringLiteral("capability")).toString();
        const auto portValue = object.value(QStringLiteral("port")).toDouble();
        if (portValue < 1 || portValue > 65535 || std::trunc(portValue) != portValue) {
            return std::nullopt;
        }
        const auto port = static_cast<int>(portValue);
        QUrl url;
        url.setScheme(QStringLiteral("http"));
        url.setHost(sender.toString());
        url.setPort(port);
        url.setPath(QStringLiteral("/instance/") + capability);
        QString error;
        if (!isHexId(requestId, g_idHexLength) || !Offer::isLocalOfferUrl(url, &error)) {
            return std::nullopt;
        }
        return Message(TransferReady{ .requestId = requestId, .url = url });
    }
    if (type == QStringLiteral("failure")) {
        if (!object.value(QStringLiteral("request")).isString() || !object.value(QStringLiteral("reason")).isString()) {
            return std::nullopt;
        }
        TransferFailure failure{ .requestId = object.value(QStringLiteral("request")).toString(),
                                 .reason = object.value(QStringLiteral("reason")).toString() };
        if (makeTransferFailureDatagram(failure).isEmpty()) {
            return std::nullopt;
        }
        return Message(std::move(failure));
    }
    return std::nullopt;
}

}  // namespace Lan
