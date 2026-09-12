// SPDX-License-Identifier: GPL-3.0-only

#include "ui/pages/modplatform/LanPage.h"

#include "Application.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "lan/LanInstanceService.h"
#include "minecraft/MinecraftInstance.h"
#include "tasks/Task.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/dialogs/ProgressDialog.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int g_ServiceIdRole = Qt::UserRole;
constexpr int g_InstanceIdRole = Qt::UserRole + 1;
constexpr int g_AvailableRole = Qt::UserRole + 2;
constexpr int g_InstanceNameRole = Qt::UserRole + 3;

}  // namespace

LanPage::LanPage(NewInstanceDialog* dialog, QWidget* parent)
    : QWidget(parent)
    , m_dialog(dialog)
    , m_service(APPLICATION->lanInstanceService())
    , m_statusLabel(new QLabel(this))
    , m_instances(new QListWidget(this))
    , m_localInstances(new QComboBox(this))
    , m_refreshButton(new QPushButton(tr("Refresh"), this))
    , m_prepareButton(new QPushButton(tr("Prepare selected"), this))
    , m_updateButton(new QPushButton(tr("Update local instance"), this))
{
    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(
        tr("Prism automatically finds instances from other Prism Launchers on your local network. Select an available instance to "
           "import a new copy or update an existing local instance."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_statusLabel->setText(tr("Looking for LAN instances..."));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_instances->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_instances);

    auto* updateLabel = new QLabel(tr("Update an existing local instance from the selected LAN instance:"), this);
    updateLabel->setWordWrap(true);
    layout->addWidget(updateLabel);
    layout->addWidget(m_localInstances);

    auto* controls = new QHBoxLayout();
    controls->addStretch();
    controls->addWidget(m_refreshButton);
    controls->addWidget(m_prepareButton);
    controls->addWidget(m_updateButton);
    layout->addLayout(controls);

    connect(m_service, &Lan::InstanceService::remoteInstancesChanged, this, &LanPage::reloadInstances);
    connect(m_service, &Lan::InstanceService::transferReady, this, &LanPage::transferReady);
    connect(m_service, &Lan::InstanceService::transferFailed, this, &LanPage::transferFailed);
    connect(m_instances, &QListWidget::itemSelectionChanged, this, &LanPage::updateActions);
    connect(m_localInstances, qOverload<int>(&QComboBox::currentIndexChanged), this, &LanPage::updateActions);
    connect(m_instances, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { prepareSelected(); });
    connect(m_refreshButton, &QPushButton::clicked, this, &LanPage::refreshInstances);
    connect(m_prepareButton, &QPushButton::clicked, this, &LanPage::prepareSelected);
    connect(m_updateButton, &QPushButton::clicked, this, &LanPage::updateSelected);

    updateActions();
}

LanPage::~LanPage()
{
    cancelPendingRequest();
}

QIcon LanPage::icon() const
{
    QIcon icon;
    const auto folder = QIcon::fromTheme("viewfolder");
    for (const auto size : { 16, 24, 32, 48, 64 }) {
        auto pixmap = folder.pixmap(size, size);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        const qreal badgeSize = size * 0.55;
        const QRectF badge(size - badgeSize, size - badgeSize, badgeSize, badgeSize);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Highlight));
        painter.drawEllipse(badge);

        const auto center = badge.center();
        const qreal radius = badgeSize * 0.11;
        const QPointF top(center.x(), badge.top() + (badgeSize * 0.27));
        const QPointF left(badge.left() + (badgeSize * 0.27), badge.bottom() - (badgeSize * 0.27));
        const QPointF right(badge.right() - (badgeSize * 0.27), badge.bottom() - (badgeSize * 0.27));
        const QPen networkPen(Qt::white, qMax<qreal>(1.0, size / 16.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(networkPen);
        painter.drawLine(top, left);
        painter.drawLine(top, right);
        painter.drawLine(left, right);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.drawEllipse(top, radius, radius);
        painter.drawEllipse(left, radius, radius);
        painter.drawEllipse(right, radius, radius);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

void LanPage::openedImpl()
{
    m_dialog->setSuggestedPack();
    reloadLocalInstances();
    if (!m_service->isRunning()) {
        m_statusLabel->setText(tr("LAN instance discovery is unavailable. Check Prism Launcher's log for the network error."));
        m_refreshButton->setEnabled(false);
        updateActions();
        return;
    }
    refreshInstances();
}

void LanPage::closedImpl()
{
    cancelPendingRequest();
}

void LanPage::reloadInstances()
{
    if (!m_requestId.isEmpty() || !isOpened) {
        return;
    }

    m_instances->clear();
    const auto instances = m_service->remoteInstances();
    for (const auto& instance : instances) {
        const auto label = instance.available ? tr("%1 (%2)").arg(instance.instanceName, instance.address.toString())
                                              : tr("%1 (%2, currently running)").arg(instance.instanceName, instance.address.toString());
        auto* item = new QListWidgetItem(label, m_instances);
        item->setData(g_ServiceIdRole, instance.serviceId);
        item->setData(g_InstanceIdRole, instance.instanceId);
        item->setData(g_AvailableRole, instance.available);
        item->setData(g_InstanceNameRole, instance.instanceName);
        if (!instance.available) {
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
        }
    }
    m_statusLabel->setText(instances.isEmpty() ? tr("No LAN instances found yet.") : tr("LAN instances found: %1").arg(instances.size()));
    updateActions();
}

void LanPage::prepareSelected()
{
    requestSelected({});
}

void LanPage::updateSelected()
{
    QString targetId = m_localInstances->currentData().toString();
    auto* target = APPLICATION->instances()->getInstanceById(targetId);
    if (dynamic_cast<MinecraftInstance*>(target) == nullptr || target->isRunning()) {
        m_statusLabel->setText(tr("Select a local instance that is not running."));
        reloadLocalInstances();
        return;
    }
    auto response = QMessageBox::question(
        this, tr("Update instance from LAN"),
        tr("Replace the mods and pack configuration in '%1' with the selected LAN instance? Local worlds, screenshots, resource packs, "
           "server list, player options, and launcher settings will be kept.")
            .arg(target->name()),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (response == QMessageBox::Yes) {
        target->saveNow();
        requestSelected(targetId);
    }
}

void LanPage::requestSelected(const QString& updateTarget)
{
    const auto* item = m_instances->currentItem();
    if (item == nullptr || !item->data(g_AvailableRole).toBool() || !m_requestId.isEmpty()) {
        return;
    }

    QString error;
    m_requestId = m_service->requestImport(item->data(g_ServiceIdRole).toString(), item->data(g_InstanceIdRole).toString(), &error);
    if (m_requestId.isEmpty()) {
        m_statusLabel->setText(error);
        return;
    }
    m_selectedName = item->data(g_InstanceNameRole).toString();
    m_updateTarget = updateTarget;
    m_statusLabel->setText(tr("Preparing the instance on the other launcher..."));
    m_instances->setEnabled(false);
    updateActions();
}

void LanPage::refreshInstances()
{
    m_statusLabel->setText(tr("Looking for LAN instances..."));
    m_service->refreshDiscovery();
    reloadInstances();
}

void LanPage::transferReady(const QString& requestId, const QUrl& url)
{
    if (requestId != m_requestId) {
        return;
    }
    m_requestId.clear();
    if (m_updateTarget.isEmpty()) {
        m_instances->setEnabled(true);
        updateActions();
        m_dialog->importFromLan(url, m_selectedName);
        return;
    }

    QString targetId = m_updateTarget;
    m_updateTarget.clear();
    m_instances->setEnabled(true);
    auto* target = APPLICATION->instances()->getInstanceById(targetId);
    if (dynamic_cast<MinecraftInstance*>(target) == nullptr || target->isRunning()) {
        m_statusLabel->setText(tr("The selected local Minecraft instance is missing, incompatible, or currently running."));
        updateActions();
        return;
    }

    auto* importer = new InstanceImportTask(url, this);
    importer->setLanUpdateTarget(targetId);
    std::unique_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(importer));
    QString failure;
    ProgressDialog progress(this);
    connect(task.get(), &Task::failed, &progress, [&failure](const QString& reason) { failure = reason; });
    progress.setSkipButton(true, tr("Abort"));
    const int result = progress.execWithTask(task.get());
    m_statusLabel->setText(result == QDialog::Accepted ? tr("Local instance updated from LAN.")
                                                       : (failure.isEmpty() ? tr("LAN update was cancelled. The local instance was kept.")
                                                                            : tr("LAN update failed: %1").arg(failure)));
    reloadLocalInstances();
    updateActions();
}

void LanPage::transferFailed(const QString& requestId, const QString& reason)
{
    if (requestId != m_requestId) {
        return;
    }
    m_requestId.clear();
    m_selectedName.clear();
    m_updateTarget.clear();
    m_statusLabel->setText(reason);
    m_instances->setEnabled(true);
    updateActions();
}

void LanPage::updateActions()
{
    const auto* selected = m_instances->currentItem();
    const bool busy = !m_requestId.isEmpty();
    m_prepareButton->setEnabled(!busy && selected != nullptr && selected->data(g_AvailableRole).toBool());
    auto* target = APPLICATION->instances()->getInstanceById(m_localInstances->currentData().toString());
    m_updateButton->setEnabled(!busy && selected != nullptr && selected->data(g_AvailableRole).toBool() &&
                               dynamic_cast<MinecraftInstance*>(target) != nullptr && !target->isRunning());
    m_localInstances->setEnabled(!busy);
    m_refreshButton->setEnabled(!busy && m_service->isRunning());
}

void LanPage::cancelPendingRequest()
{
    if (m_requestId.isEmpty()) {
        return;
    }
    m_service->cancelImport(m_requestId);
    m_requestId.clear();
    m_selectedName.clear();
    m_updateTarget.clear();
    m_instances->setEnabled(true);
    updateActions();
}

void LanPage::reloadLocalInstances()
{
    QString selectedId = m_localInstances->currentData().toString();
    m_localInstances->clear();
    auto* instances = APPLICATION->instances();
    for (int index = 0; index < instances->count(); ++index) {
        auto* instance = instances->at(index);
        if (!instance->isRunning() && dynamic_cast<MinecraftInstance*>(instance) != nullptr) {
            m_localInstances->addItem(instance->name(), instance->id());
        }
    }
    int previous = m_localInstances->findData(selectedId);
    if (previous >= 0) {
        m_localInstances->setCurrentIndex(previous);
    }
    updateActions();
}
