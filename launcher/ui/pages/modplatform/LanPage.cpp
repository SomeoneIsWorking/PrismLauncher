// SPDX-License-Identifier: GPL-3.0-only

#include "ui/pages/modplatform/LanPage.h"

#include "Application.h"
#include "lan/LanInstanceService.h"
#include "ui/dialogs/NewInstanceDialog.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
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
    , m_refreshButton(new QPushButton(tr("Refresh"), this))
    , m_prepareButton(new QPushButton(tr("Prepare selected"), this))
{
    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(
        tr("Prism automatically finds instances from other Prism Launchers on your local network. Select an available instance to "
           "prepare a temporary copy for import."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_statusLabel->setText(tr("Looking for LAN instances..."));
    layout->addWidget(m_statusLabel);

    m_instances->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_instances);

    auto* controls = new QHBoxLayout();
    controls->addStretch();
    controls->addWidget(m_refreshButton);
    controls->addWidget(m_prepareButton);
    layout->addLayout(controls);

    connect(m_service, &Lan::InstanceService::remoteInstancesChanged, this, &LanPage::reloadInstances);
    connect(m_service, &Lan::InstanceService::transferReady, this, &LanPage::transferReady);
    connect(m_service, &Lan::InstanceService::transferFailed, this, &LanPage::transferFailed);
    connect(m_instances, &QListWidget::itemSelectionChanged, this, &LanPage::updateActions);
    connect(m_instances, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { prepareSelected(); });
    connect(m_refreshButton, &QPushButton::clicked, this, &LanPage::refreshInstances);
    connect(m_prepareButton, &QPushButton::clicked, this, &LanPage::prepareSelected);

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
    m_dialog->importFromLan(url, m_selectedName);
}

void LanPage::transferFailed(const QString& requestId, const QString& reason)
{
    if (requestId != m_requestId) {
        return;
    }
    m_requestId.clear();
    m_selectedName.clear();
    m_statusLabel->setText(reason);
    m_instances->setEnabled(true);
    updateActions();
}

void LanPage::updateActions()
{
    const auto* selected = m_instances->currentItem();
    const bool busy = !m_requestId.isEmpty();
    m_prepareButton->setEnabled(!busy && selected != nullptr && selected->data(g_AvailableRole).toBool());
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
    m_instances->setEnabled(true);
    updateActions();
}
