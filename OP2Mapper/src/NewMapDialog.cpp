// NewMapDialog: File > New Map.

#include "NewMapDialog.h"

#include "BlankMaps.h"

#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>

NewMapDialog::NewMapDialog(CreateFn create, QWidget *parent)
    : QDialog(parent), m_create(std::move(create))
{
    setWindowTitle(tr("New Map"));
    setWindowIcon(QIcon(QStringLiteral(":/images/lists.ico")));
    setFixedSize(724, 521);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto label = [this](const QString &t, int y) {
        auto *l = new QLabel(t, this);
        l->setGeometry(20, y, 100, 20);
    };
    label(tr("Map Name:"), 20);
    m_mapName = new QLineEdit(tr("New Map"), this);
    m_mapName->setGeometry(120, 20, 420, 22);
    label(tr("File Name:"), 50);
    m_fileName = new QLineEdit(QStringLiteral("New_Map.map"), this);
    m_fileName->setGeometry(120, 50, 420, 22);
    label(tr("Tileset:"), 80);
    m_tileset = new QLineEdit(QStringLiteral("well00"), this);
    m_tileset->setGeometry(120, 80, 420, 22);
    label(tr("Notes:"), 110);
    m_notes = new QPlainTextEdit(this);
    m_notes->setGeometry(120, 110, 420, 70);

    auto *dims = new QGroupBox(tr("Map Dimensions"), this);
    dims->setGeometry(20, 190, 520, 56);
    auto *sizeLabel = new QLabel(tr("Map Size:"), dims);
    sizeLabel->setGeometry(10, 24, 90, 20);
    m_size = new QComboBox(dims);
    m_size->setGeometry(100, 22, 200, 22);
    for (const auto &s : BlankMaps::availableSizes())
        m_size->addItem(QStringLiteral("%1 x %2").arg(s.width).arg(s.height), QPoint(s.width, s.height));
    m_size->setCurrentIndex(4);

    auto *biome = new QGroupBox(tr("Starting Map Biome"), this);
    biome->setGeometry(12, 256, 700, 200);
    const struct { const char *file; const char *text; QColor fallback; } biomes[] = {
        { ":/images/blank_4x4.bmp", "Blank", QColor(0, 0, 255) },
        { ":/images/TerrainType_0.bmp", "Terrain 0", QColor(165, 42, 42) },
        { ":/images/TerrainType_1.bmp", "Terrain 1", QColor(255, 165, 0) },
        { ":/images/TerrainType_2.bmp", "Terrain 2", QColor(169, 169, 169) },
    };
    for (int i = 0; i < 4; ++i) {
        auto *panel = new QLabel(biome);
        panel->setGeometry(30 + i * 168, 28, 128, 128);
        panel->setScaledContents(true);
        QPixmap pm(QString::fromLatin1(biomes[i].file));
        if (pm.isNull()) {
            pm = QPixmap(128, 128);
            pm.fill(biomes[i].fallback);
            QPainter p(&pm);
            p.setPen(Qt::white);
            p.drawText(pm.rect(), Qt::AlignCenter, QString::fromLatin1(biomes[i].file).section('/', -1));
        }
        panel->setPixmap(pm);
        panel->installEventFilter(this);
        panel->setCursor(Qt::PointingHandCursor);
        m_biomePanels << panel;
        auto *text = new QLabel(tr(biomes[i].text), biome);
        text->setGeometry(30 + i * 168, 160, 128, 20);
        text->setAlignment(Qt::AlignCenter);
        text->installEventFilter(this);
        text->setCursor(Qt::PointingHandCursor);
        m_biomeLabels << text;
    }
    selectBiome(0);

    auto *ok = new QPushButton(tr("OK"), this);
    ok->setGeometry(500, 475, 100, 32);
    ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    cancel->setGeometry(610, 475, 100, 32);
    connect(ok, &QPushButton::clicked, this, &NewMapDialog::onOk);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_mapName, &QLineEdit::textChanged, this, &NewMapDialog::regenerateFileName);
    connect(m_size, qOverload<int>(&QComboBox::currentIndexChanged), this, &NewMapDialog::regenerateFileName);
    regenerateFileName();
    m_mapName->selectAll();
    m_mapName->setFocus();
}

// Template problems are reported as soon as the dialog opens, not only on OK.
void NewMapDialog::reportTemplateProblems()
{
    QString err;
    if (!BlankMaps::ensureExtracted(err)) {
        QMessageBox::warning(this, tr("New Map"),
            tr("Could not prepare the blank map templates needed for New Map.\n\nFolder: %1\nError: %2")
                .arg(QDir::toNativeSeparators(BlankMaps::folder()), err));
    }
}

// OP2Mapper3's rule: name with spaces turned into underscores, invalid
// characters removed, then "_WxH.map". Runs whenever the name or size changes.
void NewMapDialog::regenerateFileName()
{
    QString name = m_mapName->text().trimmed();
    name.replace(QLatin1Char(' '), QLatin1Char('_'));
    QString clean;
    for (QChar c : name) {
        if (c.unicode() >= 0x20 && !QStringLiteral("\"<>|:*?\\/").contains(c))
            clean += c;
    }
    const QPoint s = m_size->currentData().toPoint();
    clean += QStringLiteral("_%1x%2").arg(s.x()).arg(s.y());
    if (!clean.endsWith(QLatin1String(".map"), Qt::CaseInsensitive))
        clean += QStringLiteral(".map");
    m_fileName->setText(clean);
}

// The selected biome panel gets a sunken 3 px frame.
void NewMapDialog::selectBiome(int index)
{
    m_biome = index;
    for (int i = 0; i < m_biomePanels.size(); ++i) {
        m_biomePanels[i]->setFrameStyle(i == index ? (QFrame::Panel | QFrame::Sunken) : (QFrame::Box | QFrame::Plain));
        m_biomePanels[i]->setLineWidth(i == index ? 3 : 1);
    }
}

// Clicking a biome picture or its caption selects it.
bool NewMapDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        for (int i = 0; i < m_biomePanels.size(); ++i) {
            if (watched == m_biomePanels[i] || watched == m_biomeLabels[i]) {
                selectBiome(i);
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

// Validates the fields ("Missing Information"), then hands the result to the
// create callback; the dialog stays open if that fails.
void NewMapDialog::onOk()
{
    const QString title = tr("Missing Information");
    if (m_mapName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, title, tr("Please enter a map name."));
        m_mapName->setFocus();
        return;
    }
    QString file = m_fileName->text().trimmed();
    if (file.isEmpty()) {
        QMessageBox::warning(this, title, tr("Please enter a file name."));
        m_fileName->setFocus();
        return;
    }
    if (!file.endsWith(QLatin1String(".map"), Qt::CaseInsensitive))
        file += QStringLiteral(".map");
    if (m_tileset->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, title, tr("Please enter a tileset name."));
        m_tileset->setFocus();
        return;
    }
    if (m_size->currentIndex() < 0) {
        QMessageBox::warning(this, title, tr("Please select a map size."));
        m_size->setFocus();
        return;
    }
    Result r;
    r.mapName = m_mapName->text().trimmed();
    r.fileName = file;
    r.tileset = m_tileset->text().trimmed();
    r.notes = m_notes->toPlainText();
    const QPoint s = m_size->currentData().toPoint();
    r.width = s.x();
    r.height = s.y();
    r.biome = static_cast<Biome>(m_biome);
    if (m_create && m_create(r))
        accept();
}
