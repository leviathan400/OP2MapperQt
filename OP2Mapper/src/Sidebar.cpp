// Sidebar: the right-hand information panel of the main window.

#include "Sidebar.h"

#include "Editor.h"

#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>

void ElideLabel::setFullText(const QString &text)
{
    m_full = text;
    setText(text);
    update();
}

void ElideLabel::setColor(const QColor &c)
{
    QPalette pal = palette();
    pal.setColor(QPalette::WindowText, c);
    setPalette(pal);
}

// Elides to the label width; the tooltip shows the full text only when it was cut.
void ElideLabel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QFontMetrics fm(font());
    const QString shown = fm.elidedText(m_full, Qt::ElideRight, width());
    setToolTip(shown != m_full ? m_full : QString());
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, shown);
}

Sidebar::Sidebar(QWidget *parent)
    : QScrollArea(parent)
{
    setFixedWidth(264);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    auto *outer = new QVBoxLayout(content);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->setSpacing(14);

    QVBoxLayout *l = nullptr;
    outer->addWidget(section(tr("Map"), l));
    m_mapName = row(l, QString());
    m_mapSize = row(l, QString());
    m_mapImage = row(l, QString());
    m_tileCount = row(l, QString());

    m_tilesetsBox = section(tr("Tilesets"), l);
    outer->addWidget(m_tilesetsBox);
    m_tilesetList = new QListWidget;
    m_tilesetList->setSelectionMode(QAbstractItemView::NoSelection);
    m_tilesetList->setFocusPolicy(Qt::NoFocus);
    m_tilesetList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    l->addWidget(m_tilesetList);

    outer->addWidget(section(tr("Tile Under Cursor"), l));
    m_coords = row(l, QString());
    m_tileset = row(l, QString());
    m_imageIndex = row(l, QString());
    m_mappingIndex = row(l, QString());
    m_cellType = row(l, QString());
    auto *prev = new QHBoxLayout;
    prev->setContentsMargins(2, 4, 0, 0);
    prev->setSpacing(10);
    m_smallPreview = new QLabel;
    m_smallPreview->setFixedSize(34, 34);
    m_smallPreview->setStyleSheet(QStringLiteral("background: black; border: 1px solid #808080;"));
    m_smallPreview->setAlignment(Qt::AlignCenter);
    m_bigPreview = new QLabel;
    m_bigPreview->setFixedSize(98, 98);
    m_bigPreview->setStyleSheet(QStringLiteral("background: black; border: 1px solid #808080;"));
    m_bigPreview->setAlignment(Qt::AlignCenter);
    prev->addWidget(m_smallPreview, 0, Qt::AlignTop);
    prev->addWidget(m_bigPreview, 0, Qt::AlignTop);
    prev->addStretch();
    l->addLayout(prev);

    outer->addWidget(section(tr("View"), l));
    auto *zoomRow = new QHBoxLayout;
    m_zoom = new QLabel;
    m_resetZoom = new QPushButton(tr("Reset Zoom"));
    m_resetZoom->setFixedSize(84, 24);
    zoomRow->addWidget(m_zoom, 1);
    zoomRow->addWidget(m_resetZoom);
    l->addLayout(zoomRow);

    outer->addWidget(section(tr("Editing"), l));
    m_editMode = new QPushButton(tr("Enable Edit Mode"));
    m_editMode->setFixedHeight(28);
    m_clearTool = new QPushButton(tr("Clear Tool Selection"));
    m_clearTool->setFixedHeight(28);
    l->addWidget(m_editMode);
    l->addSpacing(6);
    l->addWidget(m_clearTool);
    auto *toolRow = new QHBoxLayout;
    toolRow->setContentsMargins(0, 6, 0, 0);
    m_toolIcon = new QLabel;
    m_toolIcon->setFixedSize(34, 34);
    m_toolIcon->setAlignment(Qt::AlignCenter);
    m_toolIcon->setStyleSheet(QStringLiteral("background: black; border: 1px solid #808080;"));
    auto *labels = new QVBoxLayout;
    labels->setSpacing(1);
    m_toolLabel = new ElideLabel;
    m_placementLabel = new ElideLabel;
    labels->addWidget(m_toolLabel);
    labels->addWidget(m_placementLabel);
    toolRow->addWidget(m_toolIcon, 0, Qt::AlignTop);
    toolRow->addLayout(labels, 1);
    l->addLayout(toolRow);

    outer->addStretch();
    setWidget(content);

    connect(m_resetZoom, &QPushButton::clicked, this, &Sidebar::resetZoomClicked);
    connect(m_editMode, &QPushButton::clicked, this, &Sidebar::editModeClicked);
    connect(m_clearTool, &QPushButton::clicked, this, &Sidebar::clearToolClicked);

    clearMap();
}

QGroupBox *Sidebar::section(const QString &title, QVBoxLayout *&inner)
{
    auto *box = new QGroupBox(title);
    inner = new QVBoxLayout(box);
    inner->setContentsMargins(8, 8, 8, 8);
    inner->setSpacing(0);
    return box;
}

ElideLabel *Sidebar::row(QVBoxLayout *layout, const QString &text)
{
    auto *l = new ElideLabel;
    l->setFixedHeight(17);
    l->setFullText(text);
    layout->addWidget(l);
    return l;
}

void Sidebar::clearMap()
{
    const QString na = tr("N/A");
    m_mapName->setFullText(tr("Map Name: %1").arg(na));
    m_mapSize->setFullText(tr("Map Size: %1").arg(na));
    m_mapImage->setFullText(tr("Map Image: %1").arg(na));
    m_tileCount->setFullText(tr("Tile Count: %1").arg(na));
    setTilesets({}, 0);
    m_tilesetsBox->setTitle(tr("Tilesets"));
    setTileInfo(false, {}, {}, 0, 0, {}, {}, {});
    setZoomText(tr("Zoom Level: N/A"), false);
    setEditModeEnabled(false, false);
}

void Sidebar::setMapInfo(const QString &fileName, int w, int h, int tileCount)
{
    m_mapName->setFullText(tr("Map Name: %1").arg(fileName));
    m_mapSize->setFullText(tr("Map Size: %1 x %2").arg(w).arg(h));
    m_mapImage->setFullText(tr("Map Image: %1 x %2 pixels").arg(w * 32).arg(h * 32));
    m_tileCount->setFullText(tr("Tile Count: %1").arg(tileCount));
}

// The list grows to fit 1..20 rows (then scrolls), plus room for a horizontal scrollbar if needed.
void Sidebar::setTilesets(const QStringList &rows, int missing)
{
    m_tilesetList->clear();
    m_tilesetList->addItems(rows);
    if (rows.isEmpty())
        m_tilesetsBox->setTitle(tr("Tilesets"));
    else if (missing > 0)
        m_tilesetsBox->setTitle(tr("Tilesets (%1, %2 not found)").arg(rows.size()).arg(missing));
    else
        m_tilesetsBox->setTitle(tr("Tilesets (%1)").arg(rows.size()));
    const int itemH = std::max(16, m_tilesetList->fontMetrics().height() + 2);
    int h = std::clamp(static_cast<int>(rows.size()), 1, 20) * itemH + 4;
    int widest = 0;
    for (const QString &r : rows)
        widest = std::max(widest, m_tilesetList->fontMetrics().horizontalAdvance(r) + 8);
    if (widest > m_tilesetList->viewport()->width() && widest > 220)
        h += m_tilesetList->horizontalScrollBar()->sizeHint().height();
    m_tilesetList->setFixedHeight(h);
}

// Selected tile details plus a 32 px and a 3x preview. A missing graphic
// shows OP2Mapper3's grey placeholder with a red X and the tileset / index.
void Sidebar::setTileInfo(bool valid, const QString &coords, const QString &tileset, int imageIndex,
                          int mappingIndex, const QString &cellType, const QImage &tile,
                          const QString &missingTileset)
{
    const QString na = tr("N/A");
    m_coords->setFullText(tr("Coordinates: %1").arg(valid ? coords : na));
    m_tileset->setFullText(tr("Tileset: %1").arg(valid ? (tileset.isEmpty() ? tr("Unknown") : tileset) : na));
    m_imageIndex->setFullText(tr("Image Index: %1").arg(valid ? QString::number(imageIndex) : na));
    m_mappingIndex->setFullText(tr("Mapping Index: %1").arg(valid ? QString::number(mappingIndex) : na));
    m_cellType->setFullText(tr("Cell Type: %1").arg(valid ? cellType : na));
    if (!valid) {
        m_smallPreview->clear();
        m_bigPreview->clear();
        return;
    }
    QImage small = tile;
    QImage big;
    if (small.isNull()) {
        small = QImage(32, 32, QImage::Format_ARGB32);
        small.fill(QColor(211, 211, 211));
        QPainter p(&small);
        p.setPen(QPen(Qt::red, 1));
        p.drawRect(0, 0, 31, 31);
        p.drawLine(0, 0, 31, 31);
        p.drawLine(0, 31, 31, 0);
        big = QImage(96, 96, QImage::Format_ARGB32);
        big.fill(QColor(211, 211, 211));
        QPainter q(&big);
        q.setPen(QPen(Qt::red, 1));
        q.drawRect(0, 0, 95, 95);
        q.drawLine(0, 0, 95, 95);
        q.drawLine(0, 95, 95, 0);
        QFont f(QStringLiteral("Arial"));
        f.setPixelSize(11);
        q.setFont(f);
        q.setPen(Qt::black);
        q.drawText(5, 5 + q.fontMetrics().ascent(), tr("Missing: %1").arg(missingTileset));
        q.drawText(5, 20 + q.fontMetrics().ascent(), tr("Index: %1").arg(imageIndex));
    } else {
        big = small.scaled(96, 96, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    m_smallPreview->setPixmap(QPixmap::fromImage(small));
    m_bigPreview->setPixmap(QPixmap::fromImage(big));
}

void Sidebar::setZoomText(const QString &text, bool locked)
{
    m_zoom->setText(text);
    m_zoom->setStyleSheet(locked ? QStringLiteral("color: gray;") : QString());
}

// Disabled without a map; light blue "Disable Edit Mode" while editing.
void Sidebar::setEditModeEnabled(bool enabled, bool on)
{
    m_editMode->setEnabled(enabled);
    m_editMode->setText(on ? tr("Disable Edit Mode") : tr("Enable Edit Mode"));
    m_editMode->setStyleSheet(on ? QStringLiteral("QPushButton { background: lightblue; }") : QString());
    m_toolIcon->setVisible(on);
}

void Sidebar::setToolLabels(const ToolLabels &l, bool showIcon)
{
    m_toolLabel->setFullText(l.tool);
    m_toolLabel->setColor(l.toolColor);
    m_placementLabel->setFullText(l.placement);
    m_placementLabel->setColor(l.placementColor);
    m_toolIcon->setVisible(showIcon);
    if (!l.swatch.isNull()) {
        m_toolIcon->setPixmap(l.swatch);
    } else {
        QPixmap pm(32, 32);
        pm.fill(l.swatchBack);
        m_toolIcon->setPixmap(pm);
    }
}
