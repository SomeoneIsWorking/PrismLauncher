// SPDX-License-Identifier: GPL-3.0-only

#include "ui/dialogs/LanShareDialog.h"

#include "BaseInstance.h"
#include "archive/ExportToZipTask.h"
#include "lan/LanShareController.h"
#include "ui/dialogs/ProgressDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

LanShareDialog::LanShareDialog(BaseInstance* instance, QWidget* parent)
    : QDialog(parent)
    , m_controller(std::make_unique<Lan::ShareController>(instance))
    , m_statusLabel(new QLabel(this))
    , m_links(new QPlainTextEdit(this))
    , m_startButton(new QPushButton(tr("Start sharing"), this))
    , m_copyButton(new QPushButton(tr("Copy link"), this))
    , m_stopButton(new QPushButton(tr("Stop sharing"), this))
{
    setWindowTitle(tr("Share instance on local network"));
    setMinimumWidth(560);

    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(
        tr("Prism will create a temporary ZIP for the selected instance and offer it only while this dialog stays open. "
           "Other Prism Launchers can find it automatically from Add Instance > Import from LAN. You can also copy a link as a fallback."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_statusLabel->setText(tr("Sharing is stopped."));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_links->setReadOnly(true);
    m_links->setPlaceholderText(tr("Start sharing to announce this instance on the LAN."));
    layout->addWidget(m_links);

    auto* controls = new QDialogButtonBox(this);
    controls->addButton(m_startButton, QDialogButtonBox::ActionRole);
    controls->addButton(m_copyButton, QDialogButtonBox::ActionRole);
    controls->addButton(m_stopButton, QDialogButtonBox::ActionRole);
    controls->addButton(QDialogButtonBox::Close);
    layout->addWidget(controls);

    m_copyButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    connect(m_startButton, &QPushButton::clicked, this, &LanShareDialog::startSharing);
    connect(m_copyButton, &QPushButton::clicked, this, &LanShareDialog::copyPrimaryLink);
    connect(m_stopButton, &QPushButton::clicked, this, &LanShareDialog::stopSharing);
    connect(controls, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

LanShareDialog::~LanShareDialog()
{
    stopSharing();
}

void LanShareDialog::startSharing()
{
    QString error;
    const auto task = m_controller->createArchiveTask(&error);
    if (!task) {
        QMessageBox::critical(this, tr("Cannot share instance"), error);
        return;
    }

    ProgressDialog progress(this);
    progress.setSkipButton(true, tr("Abort"));
    if (progress.execWithTask(task.get()) != QDialog::Accepted) {
        return;
    }
    if (!m_controller->start(&error)) {
        QMessageBox::critical(this, tr("Cannot start LAN share"), error);
        return;
    }
    showOfferLinks();
}

void LanShareDialog::copyPrimaryLink()
{
    const auto urls = m_controller->offerUrls();
    if (!urls.empty()) {
        QApplication::clipboard()->setText(urls.front().toString());
    }
}

void LanShareDialog::stopSharing()
{
    m_controller->stop();
    m_statusLabel->setText(tr("Sharing is stopped."));
    m_links->clear();
    m_startButton->setEnabled(true);
    m_copyButton->setEnabled(false);
    m_stopButton->setEnabled(false);
}

void LanShareDialog::showOfferLinks()
{
    QStringList links;
    for (const auto& url : m_controller->offerUrls()) {
        links.append(url.toString());
    }
    m_links->setPlainText(links.join('\n'));
    m_statusLabel->setText(tr("Sharing is active. It stops when you close this dialog or choose Stop sharing."));
    m_startButton->setEnabled(false);
    m_copyButton->setEnabled(!links.empty());
    m_stopButton->setEnabled(true);
}
