#include "NewMapDialog.h"

#include "BlankMaps.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

NewMapDialog::NewMapDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Map"));

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    m_sizeCombo = new QComboBox;
    for (const auto &s : BlankMaps::availableSizes()) {
        m_sizeCombo->addItem(tr("%1 × %2").arg(s.width).arg(s.height),
                             QPoint(s.width, s.height));
    }
    form->addRow(tr("Size:"), m_sizeCombo);
    layout->addLayout(form);

    auto *info = new QLabel(tr(
        "Creates a new map from the bundled blank template.\n"
        "Biome presets (Greenworld terrain types) will be added in a future build."));
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(info);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, this, &NewMapDialog::onAccept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void NewMapDialog::onAccept()
{
    const QPoint p = m_sizeCombo->currentData().toPoint();
    m_width = p.x();
    m_height = p.y();
    accept();
}
