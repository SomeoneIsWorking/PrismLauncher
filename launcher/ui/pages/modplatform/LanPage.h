// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"

class QLabel;
class QListWidget;
class QListWidgetItem;
class NewInstanceDialog;
class QPushButton;

namespace Lan {
class InstanceService;
}

class LanPage final : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit LanPage(NewInstanceDialog* dialog, QWidget* parent = nullptr);
    ~LanPage() override;

    QString displayName() const override { return tr("Import from LAN"); }
    QIcon icon() const override;
    QString id() const override { return "lan"; }
    void openedImpl() override;
    void closedImpl() override;

   private slots:
    void reloadInstances();
    void prepareSelected();
    void refreshInstances();
    void transferReady(const QString& requestId, const QUrl& url);
    void transferFailed(const QString& requestId, const QString& reason);
    void updateActions();

   private:
    void cancelPendingRequest();

    NewInstanceDialog* m_dialog;
    Lan::InstanceService* m_service;
    QLabel* m_statusLabel;
    QListWidget* m_instances;
    QPushButton* m_refreshButton;
    QPushButton* m_prepareButton;
    QString m_selectedName;
    QString m_requestId;
};
