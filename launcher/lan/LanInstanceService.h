// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class InstanceList;

namespace Lan {

struct RemoteInstance {
    QString serviceId;
    QString instanceId;
    QString instanceName;
    QHostAddress address;
    quint16 port = 0;
    bool available = false;
};

// Application-owned LAN catalogue and on-demand transfer coordinator. It
// advertises instance metadata continuously, but creates and serves an archive
// only in response to a recipient request.
class InstanceService final : public QObject {
    Q_OBJECT

   public:
    InstanceService(InstanceList* instances, QString dataRoot, QObject* parent = nullptr);
    ~InstanceService() override;

    InstanceService(const InstanceService&) = delete;
    InstanceService& operator=(const InstanceService&) = delete;

    bool start(QString* error);
    void stop();
    bool isRunning() const;
    QList<RemoteInstance> remoteInstances() const;
    void refreshDiscovery();
    QString requestImport(const QString& serviceId, const QString& instanceId, QString* error);
    void cancelImport(const QString& requestId);

   signals:
    void remoteInstancesChanged();
    void transferReady(const QString& requestId, const QUrl& url);
    void transferFailed(const QString& requestId, const QString& reason);

   private slots:
    void announceInstances();
    void receiveDatagrams();
    void maintainState();
    void retryOutgoingRequest();

   private:
    class Private;
    std::unique_ptr<Private> m_private;
};

}  // namespace Lan
