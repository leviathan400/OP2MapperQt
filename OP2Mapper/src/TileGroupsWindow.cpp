#include "TileGroupsWindow.h"

#include "Editor.h"
#include "MapDocument.h"
#include "MapRenderer.h"
#include "Settings.h"
#include "TileGroupIO.h"
#include "TileImageCache.h"

#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>

namespace {
const char *kDefaultFolder = "(Default)";
const char *kNoGroups = "No Tile Groups Found";
} // namespace

class TileGroupsPreview : public QWidget
{
public:
    TileGroupsPreview(TileGroupsWindow *owner, QScrollArea *scroll)
        : QWidget(scroll), m_owner(owner), m_scroll(scroll) {}
    void refit()
    {
        const QSize view = m_scroll->viewport()->size();
        resize(m_owner->m_previewImage.isNull() ? view : m_owner->m_previewImage.size().expandedTo(view));
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(169, 169, 169));
        if (!m_owner->m_previewImage.isNull()) {
            p.drawImage(0, 0, m_owner->m_previewImage);
            return;
        }
        QFont f(QStringLiteral("Arial"));
        f.setPixelSize(12);
        p.setFont(f);
        p.setPen(QColor(105, 105, 105));
        QString text;
        if (m_owner->m_noGroupsAvailable) {
            text = QObject::tr("No tile groups found.\n\nDrop .json files in:\n%1")
                       .arg(m_owner->m_root.isEmpty() ? QObject::tr("(Tilegroups folder not configured)")
                                                      : QDir::toNativeSeparators(m_owner->m_root));
        } else if (m_owner->m_groupCombo->count() > 0
                   && m_owner->m_groupCombo->currentText() != QLatin1String(kNoGroups)) {
            text = QObject::tr("Select a tile group from the list above.");
        }
        if (!text.isEmpty())
            p.drawText(QRect(10, 10, width() - 20, height() - 20), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
    }

private:
    TileGroupsWindow *m_owner;
    QScrollArea *m_scroll;
};

TileGroupsWindow::TileGroupsWindow(Editor *editor, QWidget *mainWindow)
    : ToolPalette(QStringLiteral("TileGroups"), QStringLiteral("fTileGroups"), mainWindow)
    , m_editor(editor)
{
    setWindowTitle(tr("Tile Groups"));
    setFixedSize(446, 382);

    auto label = [this](const QString &text, int x, int y, int w = 80, Qt::Alignment a = Qt::AlignLeft) {
        auto *l = new QLabel(text, this);
        l->setGeometry(x, y, w, 16);
        l->setAlignment(a | Qt::AlignVCenter);
        return l;
    };
    label(tr("Folder:"), 12, 9);
    m_folderCombo = new QComboBox(this);
    m_folderCombo->setGeometry(12, 25, 193, 21);
    label(tr("Tile Group:"), 12, 49);
    m_groupCombo = new QComboBox(this);
    m_groupCombo->setGeometry(12, 65, 193, 21);

    m_scroll = new QScrollArea(this);
    m_scroll->setGeometry(12, 92, 259, 243);
    m_scroll->setFrameShape(QFrame::Box);
    m_scroll->setWidgetResizable(false);
    m_preview = new TileGroupsPreview(this, m_scroll);
    m_scroll->setWidget(m_preview);

    m_pathLabel = new QLabel(this);
    m_pathLabel->setGeometry(12, 339, 420, 32);
    m_pathLabel->setStyleSheet(QStringLiteral("color: #696969;"));
    m_pathLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    auto value = [this](int y) {
        auto *l = new QLabel(this);
        l->setGeometry(365, y, 76, 16);
        l->setStyleSheet(QStringLiteral("color: #4040a0;"));
        return l;
    };
    label(tr("Tile Groups:"), 280, 15, 80, Qt::AlignRight);
    m_countValue = value(15);
    label(tr("TileSet:"), 280, 45, 80, Qt::AlignRight);
    m_tilesetValue = value(45);
    label(tr("Name:"), 280, 75, 80, Qt::AlignRight);
    m_nameValue = value(75);
    label(tr("Width:"), 280, 105, 80, Qt::AlignRight);
    m_widthValue = value(105);
    label(tr("Height:"), 280, 135, 80, Qt::AlignRight);
    m_heightValue = value(135);
    label(tr("Notes:"), 280, 165, 80, Qt::AlignRight);
    m_notesValue = value(165);
    m_notesValue->setGeometry(365, 165, 76, 40);
    m_notesValue->setWordWrap(true);
    m_notesValue->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_notesValue->setText(tr("N/A"));

    auto *place = new QPushButton(tr("Place Tile Group"), this);
    place->setGeometry(303, 210, 130, 23);
    auto *hint = label(tr("Ctrl+B to Place"), 303, 236, 130, Qt::AlignHCenter);
    hint->setStyleSheet(QStringLiteral("color: #696969;"));

    connect(place, &QPushButton::clicked, this, [this] { m_editor->beginTileGroupPlacement(true); });
    connect(m_folderCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { onFolderChanged(); });
    connect(m_groupCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { onGroupChanged(); });
    connect(m_editor->document(), &MapDocument::documentReplaced, this, [this] { renderPreview(); });

    refreshFromDisk();
}

void TileGroupsWindow::resetLabels()
{
    m_widthValue->clear();
    m_heightValue->clear();
    m_tilesetValue->clear();
    m_nameValue->clear();
    m_notesValue->clear();
    m_pathLabel->clear();
}

void TileGroupsWindow::refreshFromDisk(const QString &selectFolder)
{
    loadFolders();
    if (!selectFolder.isEmpty()) {
        for (int i = 0; i < m_folderCombo->count(); ++i) {
            if (m_folderCombo->itemText(i).compare(selectFolder, Qt::CaseInsensitive) == 0) {
                if (m_folderCombo->currentIndex() == i)
                    onFolderChanged();
                else
                    m_folderCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void TileGroupsWindow::loadFolders()
{
    m_root = Settings::tileGroupsPath();
    resetLabels();
    {
        const QSignalBlocker b1(m_folderCombo);
        const QSignalBlocker b2(m_groupCombo);
        m_folderCombo->clear();
        m_groupCombo->clear();
    }
    m_previewImage = QImage();
    const QDir root(m_root);
    if (m_root.trimmed().isEmpty() || !root.exists()) {
        m_noGroupsAvailable = true;
        m_preview->refit();
        return;
    }
    QStringList entries;
    if (!root.entryList({ QStringLiteral("*.json") }, QDir::Files).isEmpty())
        entries << tr(kDefaultFolder);
    entries << root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    m_noGroupsAvailable = entries.isEmpty();
    {
        const QSignalBlocker b(m_folderCombo);
        m_folderCombo->addItems(entries);
    }
    if (!entries.isEmpty()) {
        m_folderCombo->setCurrentIndex(0);
        onFolderChanged();
    }
    m_preview->refit();
}

void TileGroupsWindow::onFolderChanged()
{
    resetLabels();
    {
        const QSignalBlocker b(m_groupCombo);
        m_groupCombo->clear();
    }
    if (m_folderCombo->currentIndex() < 0)
        return;
    const QString folder = m_folderCombo->currentText();
    const QString path = folder == tr(kDefaultFolder) ? m_root : QDir(m_root).filePath(folder);
    const QDir d(path);
    QStringList names;
    if (d.exists()) {
        for (const QString &fn : d.entryList({ QStringLiteral("*.json") }, QDir::Files, QDir::Name))
            names << QFileInfo(fn).completeBaseName();
        m_countValue->setText(QString::number(names.size()));
    }
    {
        const QSignalBlocker b(m_groupCombo);
        if (names.isEmpty())
            m_groupCombo->addItem(tr(kNoGroups));
        else
            m_groupCombo->addItems(names);
    }
    m_groupCombo->setCurrentIndex(0);
    onGroupChanged();
}

void TileGroupsWindow::onGroupChanged()
{
    const QString file = m_groupCombo->currentText();
    if (m_groupCombo->currentIndex() < 0 || file == tr(kNoGroups)) {
        m_previewImage = QImage();
        m_pathLabel->clear();
        m_preview->refit();
        return;
    }
    const QString folder = m_folderCombo->currentText();
    const QString dir = folder == tr(kDefaultFolder) ? m_root : QDir(m_root).filePath(folder);
    const QString path = QDir(dir).filePath(file + QStringLiteral(".json"));
    m_pathLabel->setText(QDir::toNativeSeparators(path));
    m_pathLabel->setToolTip(QDir::toNativeSeparators(path));

    TileGroupData data;
    TileGroupIO::Header header;
    QString err;
    if (!TileGroupIO::load(path, data, header, err)) {
        m_editor->clearTileGroup();
        m_previewImage = QImage();
        m_widthValue->clear();
        m_heightValue->clear();
        m_tilesetValue->clear();
        m_nameValue->clear();
        m_notesValue->setText(tr("N/A"));
        m_preview->refit();
        QMessageBox::warning(this, tr("Invalid Tile Group"),
            tr("The tile group file could not be read:\n%1\n\nThe file may be malformed or hand-edited incorrectly.")
                .arg(QDir::toNativeSeparators(path)));
        return;
    }
    data.fileName = file;
    m_editor->setTileGroup(data);
    const QString na = tr("N/A");
    m_widthValue->setText(QString::number(data.width));
    m_heightValue->setText(QString::number(data.height));
    m_tilesetValue->setText(header.tileset.isEmpty() ? na : header.tileset);
    m_nameValue->setText(header.name.isEmpty() ? na : header.name);
    m_notesValue->setText(header.notes.isEmpty() ? na : header.notes);
    renderPreview();
}

void TileGroupsWindow::renderPreview()
{
    m_previewImage = QImage();
    const TileGroupData &g = m_editor->tileGroup();
    const MapDocument *doc = m_editor->document();
    if (g.isLoaded() && doc && doc->isLoaded()) {
        constexpr int T = TileImageCache::TileSize;
        QImage img(g.width * T, g.height * T, QImage::Format_ARGB32);
        if (!img.isNull()) {
            img.fill(QColor(211, 211, 211));
            QPainter p(&img);
            for (int y = 0; y < g.height; ++y) {
                for (int x = 0; x < g.width; ++x) {
                    const QImage tile = MapRenderer::mappingImage(*doc, g.mappings[static_cast<std::size_t>(y * g.width + x)]);
                    if (!tile.isNull())
                        p.drawImage(x * T, y * T, tile);
                }
            }
            p.end();
            m_previewImage = img;
        }
    }
    m_preview->refit();
}

void TileGroupsWindow::showEvent(QShowEvent *event)
{
    ToolPalette::showEvent(event);
    m_preview->refit();
}
