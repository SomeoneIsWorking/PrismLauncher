// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QString>
#include <QUrl>

#include <optional>
#include <variant>

namespace Lan {

inline constexpr quint16 DiscoveryPort = 38527;

struct Query {};

struct Announcement {
    QString serviceId;
    QString instanceId;
    QString instanceName;
    bool available = false;
};

struct ImportRequest {
    QString serviceId;
    QString instanceId;
    QString requestId;
};

struct CancelRequest {
    QString serviceId;
    QString requestId;
};

struct TransferReady {
    QString requestId;
    QUrl url;
};

struct TransferFailure {
    QString requestId;
    QString reason;
};

using Message = std::variant<Query, Announcement, ImportRequest, CancelRequest, TransferReady, TransferFailure>;

QString randomId();
QByteArray makeQueryDatagram();
QByteArray makeAnnouncementDatagram(const Announcement& announcement);
QByteArray makeImportRequestDatagram(const ImportRequest& request);
QByteArray makeCancelRequestDatagram(const CancelRequest& request);
QByteArray makeTransferReadyDatagram(const QString& requestId, const QUrl& url);
QByteArray makeTransferFailureDatagram(const TransferFailure& failure);
std::optional<Message> parseDatagram(const QByteArray& datagram, const QHostAddress& sender);

}  // namespace Lan
