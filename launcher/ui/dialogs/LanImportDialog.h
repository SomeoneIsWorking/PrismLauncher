// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "lan/LanDiscovery.h"

#include <QDialog>
#include <QUrl>

#include <memory>

class QLabel;
class QListWidget;
class QListWidgetItem;

class LanImportDialog final : public QDialog {
    Q_OBJECT

   public:
    explicit LanImportDialog(QWidget* parent = nullptr);
    ~LanImportDialog() override;

    QUrl selectedUrl() const { return m_selectedUrl; }

   private slots:
    void addOffer(const Lan::DiscoveredOffer& offer);
    void importSelected();
    void refreshOffers();

   private:
    std::unique_ptr<Lan::Browser> m_browser;
    QLabel* m_statusLabel;
    QListWidget* m_offers;
    QUrl m_selectedUrl;
};
