// SPDX-License-Identifier: GPL-3.0-only

#include "lan/LanInstanceService.h"

#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "MMCZip.h"
#include "QObjectPtr.h"
#include "archive/ExportToZipTask.h"
#include "archive/InstanceArchive.h"
#include "lan/LanNetwork.h"
#include "lan/LanOffer.h"
#include "lan/LanProtocol.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QNetworkDatagram>
#include <QQueue>
#include <QTemporaryDir>
#include <QTimer>
#include <QUdpSocket>

#include <algorithm>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr int g_announcementIntervalMs = 1500;
constexpr int g_remoteExpiryMs = 5000;
constexpr int g_requestTimeoutMs = 10 * 60 * 1000;
constexpr int g_transferLifetimeMs = 30 * 60 * 1000;
constexpr qsizetype g_maxQueuedRequests = 8;
constexpr std::size_t g_maxActiveTransfers = 4;

QString remoteKey(const QString& serviceId, const QString& instanceId)
{
    return serviceId + QChar(':') + instanceId;
}

}  // namespace

namespace Lan {

namespace {

struct RemoteRecord {
    RemoteInstance instance;
    qint64 lastSeen = 0;
};

struct RequestContext {
    QString requestId;
    QString instanceId;
    QHostAddress peerAddress;
    quint16 peerPort = 0;
};

struct ActiveTransfer {
    RequestContext request;
    std::unique_ptr<QTemporaryDir> directory;
    std::unique_ptr<Offer> offer;
    QByteArray readyDatagram;
    qint64 expiresAt = 0;
};

struct OutgoingRequest {
    QString requestId;
    QString serviceId;
    QString instanceId;
    QHostAddress peerAddress;
    quint16 peerPort = 0;
    QByteArray datagram;
    qint64 expiresAt = 0;
};

}  // namespace

class InstanceService::Private {
   public:
    Private(InstanceService* owner, InstanceList* instanceList, QString root)
        : q(owner), instances(instanceList), dataRoot(std::move(root)), serviceId(randomId())
    {}

    QString publicInstanceId(const QString& localInstanceId) const
    {
        return QString::fromLatin1(
            QCryptographicHash::hash((serviceId + QChar('\0') + localInstanceId).toUtf8(), QCryptographicHash::Sha256).toHex());
    }

    BaseInstance* localInstance(const QString& publicId) const
    {
        for (int index = 0; index < instances->count(); ++index) {
            auto* instance = instances->at(index);
            if (publicInstanceId(instance->id()) == publicId) {
                return instance;
            }
        }
        return nullptr;
    }

    void send(const QByteArray& datagram, const QHostAddress& address, quint16 port)
    {
        if (!datagram.isEmpty()) {
            controlSocket.writeDatagram(datagram, address, port);
        }
    }

    void sendFailure(const RequestContext& request, const QString& reason)
    {
        send(makeTransferFailureDatagram({ .requestId = request.requestId, .reason = reason }), request.peerAddress, request.peerPort);
    }

    bool hasRequest(const QString& requestId, const QHostAddress& peer) const
    {
        if (preparing && preparing->requestId == requestId && preparing->peerAddress == peer) {
            return true;
        }
        return std::ranges::any_of(
            requests, [&](const RequestContext& request) { return request.requestId == requestId && request.peerAddress == peer; });
    }

    void enqueue(const RequestContext& request)
    {
        for (const auto& transfer : activeTransfers) {
            if (transfer->request.requestId == request.requestId && transfer->request.peerAddress == request.peerAddress) {
                send(transfer->readyDatagram, request.peerAddress, request.peerPort);
                return;
            }
        }
        if (hasRequest(request.requestId, request.peerAddress)) {
            return;
        }
        if (requests.size() >= g_maxQueuedRequests) {
            sendFailure(request, InstanceService::tr("The other launcher has too many pending LAN imports. Try again shortly."));
            return;
        }
        requests.enqueue(request);
        startNextPreparation();
    }

    void startNextPreparation()
    {
        if (archiveTask || requests.isEmpty()) {
            return;
        }
        while (!requests.isEmpty()) {
            preparing = requests.dequeue();
            auto* instance = localInstance(preparing->instanceId);
            if (instance == nullptr) {
                sendFailure(*preparing, InstanceService::tr("That instance is no longer available."));
                preparing.reset();
                continue;
            }
            if (instance->isRunning()) {
                sendFailure(*preparing, InstanceService::tr("That instance is currently running and cannot be copied safely."));
                preparing.reset();
                continue;
            }
            if (activeTransfers.size() >= g_maxActiveTransfers) {
                sendFailure(*preparing, InstanceService::tr("The other launcher has too many active LAN transfers. Try again later."));
                preparing.reset();
                continue;
            }

            const auto transfersRoot = FS::PathCombine(dataRoot, "cache", "lan-import");
            if (!FS::ensureFolderPathExists(transfersRoot)) {
                preparationFailed(InstanceService::tr("The other launcher could not create its LAN transfer cache."));
                continue;
            }
            preparingDirectory = std::make_unique<QTemporaryDir>(FS::PathCombine(transfersRoot, "transfer-XXXXXX"));
            if (!preparingDirectory->isValid()) {
                preparationFailed(InstanceService::tr("The other launcher could not create a temporary transfer directory."));
                continue;
            }

            MMCZip::saveInstanceIcon(instance);
            QFileInfoList files;
            if (!MMCZip::collectFileListRecursively(instance->instanceRoot(), nullptr, &files, {})) {
                preparationFailed(InstanceService::tr("The other launcher could not read that instance."));
                continue;
            }

            preparingArchivePath = preparingDirectory->filePath(QStringLiteral("instance.zip"));
            archiveTask = makeShared<MMCZip::ExportToZipTask>(preparingArchivePath, instance->instanceRoot(), files, QString(), true);
            preparingRunningConnection = QObject::connect(instance, &BaseInstance::runningStatusChanged, q, [this](bool running) {
                if (running && archiveTask) {
                    preparationAbortReason = InstanceService::tr("That instance started running and can no longer be copied safely.");
                    static_cast<Task*>(archiveTask.get())->abort();
                }
            });
            QObject::connect(archiveTask.get(), &Task::succeeded, q, [this] { preparationSucceeded(); });
            QObject::connect(archiveTask.get(), &Task::failed, q, [this](const QString& reason) {
                preparationFailed(InstanceService::tr("The other launcher could not prepare the instance: %1").arg(reason));
                startNextPreparation();
            });
            QObject::connect(archiveTask.get(), &Task::aborted, q, [this] {
                if (preparing && !preparationAbortReason.isEmpty()) {
                    sendFailure(*preparing, preparationAbortReason);
                }
                clearPreparation();
                startNextPreparation();
            });
            archiveTask->start();
            return;
        }
    }

    void preparationSucceeded()
    {
        if (!preparing) {
            preparationFailed(InstanceService::tr("The requested LAN import is no longer pending."));
            startNextPreparation();
            return;
        }
        const auto* instance = localInstance(preparing->instanceId);
        if (instance == nullptr || instance->isRunning()) {
            preparationFailed(InstanceService::tr("That instance is no longer available to copy safely."));
            startNextPreparation();
            return;
        }
        auto transfer = std::make_unique<ActiveTransfer>();
        transfer->request = preparing.value();
        transfer->directory = std::move(preparingDirectory);
        transfer->offer = std::make_unique<Offer>();
        QString error;
        if (!transfer->offer->start(preparingArchivePath, &error)) {
            preparationFailed(InstanceService::tr("The other launcher could not start the transfer: %1").arg(error));
            startNextPreparation();
            return;
        }
        const auto urls = transfer->offer->urls();
        if (urls.isEmpty()) {
            preparationFailed(InstanceService::tr("The other launcher has no reachable local-network address."));
            startNextPreparation();
            return;
        }
        transfer->readyDatagram = makeTransferReadyDatagram(transfer->request.requestId, urls.front());
        if (transfer->readyDatagram.isEmpty()) {
            preparationFailed(InstanceService::tr("The other launcher could not create a safe LAN transfer response."));
            startNextPreparation();
            return;
        }
        transfer->expiresAt = QDateTime::currentMSecsSinceEpoch() + g_transferLifetimeMs;
        send(transfer->readyDatagram, transfer->request.peerAddress, transfer->request.peerPort);
        activeTransfers.push_back(std::move(transfer));
        clearPreparation();
        startNextPreparation();
    }

    void preparationFailed(const QString& reason)
    {
        if (preparing) {
            sendFailure(*preparing, reason);
        }
        clearPreparation();
    }

    void clearPreparation()
    {
        QObject::disconnect(preparingRunningConnection);
        preparingRunningConnection = {};
        archiveTask.reset();
        preparing.reset();
        preparingDirectory.reset();
        preparingArchivePath.clear();
        preparationAbortReason.clear();
    }

    void cancel(const CancelRequest& cancelRequest, const QHostAddress& peer)
    {
        for (auto iterator = activeTransfers.begin(); iterator != activeTransfers.end();) {
            const auto& request = (*iterator)->request;
            if (request.requestId == cancelRequest.requestId && request.peerAddress == peer) {
                iterator = activeTransfers.erase(iterator);
            } else {
                ++iterator;
            }
        }
        for (qsizetype index = requests.size(); index-- > 0;) {
            const auto& request = requests.at(index);
            if (request.requestId == cancelRequest.requestId && request.peerAddress == peer) {
                requests.removeAt(index);
            }
        }
        if (preparing && preparing->requestId == cancelRequest.requestId && preparing->peerAddress == peer && archiveTask) {
            static_cast<Task*>(archiveTask.get())->abort();
        }
    }

    InstanceService* q;
    InstanceList* instances;
    QString dataRoot;
    QString serviceId;
    QUdpSocket discoverySocket;
    QUdpSocket controlSocket;
    QTimer announcementTimer;
    QTimer maintenanceTimer;
    QTimer outgoingRetryTimer;
    QHash<QString, RemoteRecord> remotes;
    QQueue<RequestContext> requests;
    std::optional<RequestContext> preparing;
    std::unique_ptr<QTemporaryDir> preparingDirectory;
    QString preparingArchivePath;
    QString preparationAbortReason;
    QMetaObject::Connection preparingRunningConnection;
    shared_qobject_ptr<MMCZip::ExportToZipTask> archiveTask;
    std::vector<std::unique_ptr<ActiveTransfer>> activeTransfers;
    std::optional<OutgoingRequest> outgoing;
};

InstanceService::InstanceService(InstanceList* instances, QString dataRoot, QObject* parent)
    : QObject(parent), m_private(std::make_unique<Private>(this, instances, std::move(dataRoot)))
{
    m_private->announcementTimer.setInterval(g_announcementIntervalMs);
    m_private->maintenanceTimer.setInterval(1000);
    m_private->outgoingRetryTimer.setInterval(1000);
    connect(&m_private->discoverySocket, &QUdpSocket::readyRead, this, &InstanceService::receiveDatagrams);
    connect(&m_private->controlSocket, &QUdpSocket::readyRead, this, &InstanceService::receiveDatagrams);
    connect(&m_private->announcementTimer, &QTimer::timeout, this, &InstanceService::announceInstances);
    connect(&m_private->maintenanceTimer, &QTimer::timeout, this, &InstanceService::maintainState);
    connect(&m_private->outgoingRetryTimer, &QTimer::timeout, this, &InstanceService::retryOutgoingRequest);
    connect(instances, &InstanceList::instancesChanged, this, &InstanceService::announceInstances);
}

InstanceService::~InstanceService()
{
    stop();
}

bool InstanceService::start(QString* error)
{
    stop();
    if (privateLanAddresses().isEmpty()) {
        *error = tr("No private IPv4 local-network address is available for LAN instance discovery.");
        return false;
    }
    if (!m_private->discoverySocket.bind(QHostAddress::AnyIPv4, DiscoveryPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        *error = tr("Could not open the LAN instance discovery socket.");
        return false;
    }
    if (!m_private->controlSocket.bind(QHostAddress::AnyIPv4, 0)) {
        m_private->discoverySocket.close();
        *error = tr("Could not open the LAN instance transfer-control socket.");
        return false;
    }
    m_private->announcementTimer.start();
    m_private->maintenanceTimer.start();
    m_private->outgoingRetryTimer.start();
    announceInstances();
    m_private->send(makeQueryDatagram(), QHostAddress::Broadcast, DiscoveryPort);
    return true;
}

void InstanceService::stop()
{
    m_private->announcementTimer.stop();
    m_private->maintenanceTimer.stop();
    m_private->outgoingRetryTimer.stop();
    if (m_private->archiveTask) {
        static_cast<Task*>(m_private->archiveTask.get())->abort();
    }
    m_private->discoverySocket.close();
    m_private->controlSocket.close();
    m_private->requests.clear();
    m_private->clearPreparation();
    m_private->activeTransfers.clear();
    m_private->outgoing.reset();
    const bool hadRemotes = !m_private->remotes.isEmpty();
    m_private->remotes.clear();
    if (hadRemotes) {
        emit remoteInstancesChanged();
    }
}

bool InstanceService::isRunning() const
{
    return m_private->discoverySocket.state() == QAbstractSocket::BoundState &&
           m_private->controlSocket.state() == QAbstractSocket::BoundState;
}

QList<RemoteInstance> InstanceService::remoteInstances() const
{
    QList<RemoteInstance> result;
    result.reserve(m_private->remotes.size());
    for (const auto& record : std::as_const(m_private->remotes)) {
        result.append(record.instance);
    }
    std::ranges::sort(result, [](const RemoteInstance& left, const RemoteInstance& right) {
        const auto nameOrder = QString::localeAwareCompare(left.instanceName, right.instanceName);
        if (nameOrder != 0) {
            return nameOrder < 0;
        }
        return left.address.toString() < right.address.toString();
    });
    return result;
}

void InstanceService::refreshDiscovery()
{
    if (!isRunning()) {
        return;
    }
    const bool hadRemotes = !m_private->remotes.isEmpty();
    m_private->remotes.clear();
    if (hadRemotes) {
        emit remoteInstancesChanged();
    }
    m_private->send(makeQueryDatagram(), QHostAddress::Broadcast, DiscoveryPort);
}

QString InstanceService::requestImport(const QString& serviceId, const QString& instanceId, QString* error)
{
    if (!isRunning()) {
        *error = tr("LAN instance discovery is unavailable.");
        return {};
    }
    if (m_private->outgoing) {
        *error = tr("Another LAN import request is already in progress.");
        return {};
    }
    const auto found = m_private->remotes.constFind(remoteKey(serviceId, instanceId));
    if (found == m_private->remotes.cend()) {
        *error = tr("That LAN instance is no longer available.");
        return {};
    }
    if (!found->instance.available) {
        *error = tr("That instance is currently running on the other launcher and cannot be copied safely.");
        return {};
    }
    OutgoingRequest request;
    request.requestId = randomId();
    request.serviceId = serviceId;
    request.instanceId = instanceId;
    request.peerAddress = found->instance.address;
    request.peerPort = found->instance.port;
    request.datagram = makeImportRequestDatagram({ .serviceId = serviceId, .instanceId = instanceId, .requestId = request.requestId });
    request.expiresAt = QDateTime::currentMSecsSinceEpoch() + g_requestTimeoutMs;
    m_private->outgoing = request;
    retryOutgoingRequest();
    return request.requestId;
}

void InstanceService::cancelImport(const QString& requestId)
{
    if (!m_private->outgoing || m_private->outgoing->requestId != requestId) {
        return;
    }
    const auto cancel = makeCancelRequestDatagram({ .serviceId = m_private->outgoing->serviceId, .requestId = requestId });
    m_private->send(cancel, m_private->outgoing->peerAddress, m_private->outgoing->peerPort);
    m_private->outgoing.reset();
}

void InstanceService::announceInstances()
{
    if (!isRunning()) {
        return;
    }
    for (int index = 0; index < m_private->instances->count(); ++index) {
        const auto* instance = m_private->instances->at(index);
        const Announcement announcement{ .serviceId = m_private->serviceId,
                                         .instanceId = m_private->publicInstanceId(instance->id()),
                                         .instanceName = instance->name(),
                                         .available = !instance->isRunning() };
        m_private->send(makeAnnouncementDatagram(announcement), QHostAddress::Broadcast, DiscoveryPort);
    }
}

void InstanceService::receiveDatagrams()
{
    const auto receiveFrom = [this](QUdpSocket& socket) {
        while (socket.hasPendingDatagrams()) {
            const auto datagram = socket.receiveDatagram();
            const auto senderPort = datagram.senderPort();
            if (senderPort <= 0 || senderPort > 65535) {
                continue;
            }
            const auto peerPort = static_cast<quint16>(senderPort);
            const auto message = parseDatagram(datagram.data(), datagram.senderAddress());
            if (!message) {
                continue;
            }
            if (std::holds_alternative<Query>(*message)) {
                announceInstances();
            } else if (const auto* announcement = std::get_if<Announcement>(&*message)) {
                if (announcement->serviceId == m_private->serviceId) {
                    continue;
                }
                const auto key = remoteKey(announcement->serviceId, announcement->instanceId);
                const RemoteInstance instance{ .serviceId = announcement->serviceId,
                                               .instanceId = announcement->instanceId,
                                               .instanceName = announcement->instanceName,
                                               .address = datagram.senderAddress(),
                                               .port = peerPort,
                                               .available = announcement->available };
                const auto current = m_private->remotes.constFind(key);
                const bool changed = current == m_private->remotes.cend() || current->instance.instanceName != instance.instanceName ||
                                     current->instance.address != instance.address || current->instance.port != instance.port ||
                                     current->instance.available != instance.available;
                m_private->remotes.insert(key, RemoteRecord{ .instance = instance, .lastSeen = QDateTime::currentMSecsSinceEpoch() });
                if (changed) {
                    emit remoteInstancesChanged();
                }
            } else if (const auto* request = std::get_if<ImportRequest>(&*message)) {
                if (request->serviceId == m_private->serviceId) {
                    m_private->enqueue({ .requestId = request->requestId,
                                         .instanceId = request->instanceId,
                                         .peerAddress = datagram.senderAddress(),
                                         .peerPort = peerPort });
                }
            } else if (const auto* cancel = std::get_if<CancelRequest>(&*message)) {
                if (cancel->serviceId == m_private->serviceId) {
                    m_private->cancel(*cancel, datagram.senderAddress());
                }
            } else if (const auto* ready = std::get_if<TransferReady>(&*message)) {
                if (m_private->outgoing && ready->requestId == m_private->outgoing->requestId &&
                    datagram.senderAddress() == m_private->outgoing->peerAddress) {
                    const auto requestId = ready->requestId;
                    const auto url = ready->url;
                    m_private->outgoing.reset();
                    emit transferReady(requestId, url);
                }
            } else if (const auto* failure = std::get_if<TransferFailure>(&*message)) {
                if (m_private->outgoing && failure->requestId == m_private->outgoing->requestId &&
                    datagram.senderAddress() == m_private->outgoing->peerAddress) {
                    const auto requestId = failure->requestId;
                    const auto reason = failure->reason;
                    m_private->outgoing.reset();
                    emit transferFailed(requestId, reason);
                }
            }
        }
    };
    receiveFrom(m_private->discoverySocket);
    receiveFrom(m_private->controlSocket);
}

void InstanceService::maintainState()
{
    const auto now = QDateTime::currentMSecsSinceEpoch();
    bool remotesChanged = false;
    for (auto iterator = m_private->remotes.begin(); iterator != m_private->remotes.end();) {
        if (now - iterator->lastSeen > g_remoteExpiryMs) {
            iterator = m_private->remotes.erase(iterator);
            remotesChanged = true;
        } else {
            ++iterator;
        }
    }
    if (remotesChanged) {
        emit remoteInstancesChanged();
    }
    for (auto iterator = m_private->activeTransfers.begin(); iterator != m_private->activeTransfers.end();) {
        if ((*iterator)->expiresAt <= now) {
            iterator = m_private->activeTransfers.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void InstanceService::retryOutgoingRequest()
{
    if (!m_private->outgoing) {
        return;
    }
    if (QDateTime::currentMSecsSinceEpoch() >= m_private->outgoing->expiresAt) {
        const auto requestId = m_private->outgoing->requestId;
        m_private->outgoing.reset();
        emit transferFailed(requestId, tr("The other launcher did not finish preparing the instance in time."));
        return;
    }
    m_private->send(m_private->outgoing->datagram, m_private->outgoing->peerAddress, m_private->outgoing->peerPort);
}

}  // namespace Lan
