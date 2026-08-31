// SPDX-License-Identifier: GPL-3.0-only

#include "ui/dialogs/LanImportDialog.h"

#include "lan/LanDiscovery.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

LanImportDialog::LanImportDialog(QWidget* parent)
    : QDialog(parent), m_browser(std::make_unique<Lan::Browser>()), m_statusLabel(new QLabel(this)), m_offers(new QListWidget(this))
{
    setWindowTitle(tr("Import instance from LAN"));
    setMinimumWidth(560);

    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(tr("Prism automatically discovers instances currently shared by another Prism Launcher on your local "
                                      "network. Select one to import it."),
                                   this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_statusLabel->setText(tr("Looking for shared instances..."));
    layout->addWidget(m_statusLabel);

    m_offers->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_offers);

    auto* controls = new QDialogButtonBox(this);
    auto* refresh = controls->addButton(tr("Refresh"), QDialogButtonBox::ActionRole);
    auto* import = controls->addButton(tr("Import selected"), QDialogButtonBox::AcceptRole);
    controls->addButton(QDialogButtonBox::Close);
    import->setEnabled(false);
    layout->addWidget(controls);

    connect(m_browser.get(), &Lan::Browser::offerDiscovered, this, &LanImportDialog::addOffer);
    connect(m_offers, &QListWidget::itemSelectionChanged, import,
            [this, import] { import->setEnabled(m_offers->currentItem() != nullptr); });
    connect(m_offers, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { importSelected(); });
    connect(refresh, &QPushButton::clicked, this, &LanImportDialog::refreshOffers);
    connect(import, &QPushButton::clicked, this, &LanImportDialog::importSelected);
    connect(controls, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QString error;
    if (!m_browser->start(&error)) {
        m_statusLabel->setText(error);
        refresh->setEnabled(false);
    }
}

LanImportDialog::~LanImportDialog()
{
    m_browser->stop();
}

void LanImportDialog::addOffer(const Lan::DiscoveredOffer& offer)
{
    auto* item = new QListWidgetItem(tr("%1 (%2)").arg(offer.instanceName, offer.url.host()), m_offers);
    item->setData(Qt::UserRole, offer.url);
    m_statusLabel->setText(tr("Shared instances found: %1").arg(m_offers->count()));
}

void LanImportDialog::importSelected()
{
    const auto* item = m_offers->currentItem();
    if (item == nullptr) {
        return;
    }
    m_selectedUrl = item->data(Qt::UserRole).toUrl();
    accept();
}

void LanImportDialog::refreshOffers()
{
    m_offers->clear();
    m_selectedUrl.clear();
    m_browser->clearOffers();
    m_statusLabel->setText(tr("Looking for shared instances..."));
}
