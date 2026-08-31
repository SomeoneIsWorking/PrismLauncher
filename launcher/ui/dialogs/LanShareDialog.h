// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>

#include <memory>

class BaseInstance;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace Lan {
class ShareController;
}

class LanShareDialog final : public QDialog {
    Q_OBJECT

   public:
    explicit LanShareDialog(BaseInstance* instance, QWidget* parent = nullptr);
    ~LanShareDialog() override;

   private slots:
    void startSharing();
    void copyPrimaryLink();
    void stopSharing();

   private:
    void showOfferLinks();

    std::unique_ptr<Lan::ShareController> m_controller;
    QLabel* m_statusLabel;
    QPlainTextEdit* m_links;
    QPushButton* m_startButton;
    QPushButton* m_copyButton;
    QPushButton* m_stopButton;
};
