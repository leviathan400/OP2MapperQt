#include "TilesetWindow.h"

#include "MapDocument.h"
#include "TileImageCache.h"
#include "TilesetGrid.h"

#include <OP2Utility.h>

#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

namespace {
constexpr const char *kGeometryKey = "TilesetWindow/geometry";
}

TilesetWindow::TilesetWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool)
{
    setWindowTitle(tr("Tile Set"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    m_combo = new QComboBox(this);
    m_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    layout->addWidget(m_combo);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(false);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_grid = new TilesetGrid;
    m_scroll->setWidget(m_grid);

    m_emptyLabel = new QLabel(tr("Open a map to load tilesets."), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: gray;"));

    layout->addWidget(m_scroll, 1);
    layout->addWidget(m_emptyLabel, 1);
    m_scroll->setVisible(false);

    // Width that exactly fits the 6-column grid + scrollbar + frame.
    const int gridWidth = TilesetGrid::TilesPerRow
                          * (TilesetGrid::TileSize + TilesetGrid::TilePadding);
    setMinimumWidth(gridWidth + 40);
    resize(gridWidth + 40, 460);

    connect(m_combo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TilesetWindow::onTilesetIndexChanged);
    connect(m_grid, &TilesetGrid::tileClicked,
            this, &TilesetWindow::onGridTileClicked);
    connect(m_grid, &TilesetGrid::clearSelectionRequested,
            this, &TilesetWindow::onGridClearRequested);

    restoreGeometry();
}

void TilesetWindow::setDocument(MapDocument *doc)
{
    m_doc = doc;
    populateCombo();
}

void TilesetWindow::populateCombo()
{
    const QSignalBlocker block(m_combo);
    m_combo->clear();
    m_sourceIndices.clear();
    m_currentTilesetIdx = -1;

    if (!m_doc || !m_doc->map()) {
        m_grid->setTilesetImage(nullptr);
        m_scroll->setVisible(false);
        m_emptyLabel->setVisible(true);
        return;
    }

    const auto &sources = m_doc->map()->tilesetSources;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        if (sources[i].IsEmpty())
            continue;
        m_combo->addItem(QString::fromStdString(sources[i].tilesetFilename));
        m_sourceIndices.push_back(static_cast<int>(i));
    }

    const bool any = m_combo->count() > 0;
    m_scroll->setVisible(any);
    m_emptyLabel->setVisible(!any);
    if (!any) {
        m_grid->setTilesetImage(nullptr);
        return;
    }

    m_combo->setCurrentIndex(0);
    loadCurrentTileset();
}

void TilesetWindow::onTilesetIndexChanged(int comboRow)
{
    Q_UNUSED(comboRow);
    loadCurrentTileset();
}

void TilesetWindow::loadCurrentTileset()
{
    const int row = m_combo->currentIndex();
    if (row < 0 || row >= static_cast<int>(m_sourceIndices.size()) || !m_doc || !m_doc->cache()) {
        m_grid->setTilesetImage(nullptr);
        m_currentTilesetIdx = -1;
        return;
    }

    m_currentTilesetIdx = m_sourceIndices[row];
    const QImage *img = m_doc->cache()->tileset(static_cast<std::size_t>(m_currentTilesetIdx));
    m_grid->setTilesetImage(img);
}

void TilesetWindow::onGridTileClicked(int imageIdx)
{
    if (m_currentTilesetIdx < 0)
        return;
    emit tileSelected(m_currentTilesetIdx, imageIdx);
}

void TilesetWindow::onGridClearRequested()
{
    emit tileSelected(-1, -1);
}

void TilesetWindow::closeEvent(QCloseEvent *event)
{
    saveGeometry();
    QWidget::closeEvent(event);
}

void TilesetWindow::saveGeometry()
{
    QSettings s;
    s.setValue(QLatin1String(kGeometryKey), QWidget::saveGeometry());
}

void TilesetWindow::restoreGeometry()
{
    QSettings s;
    const QByteArray g = s.value(QLatin1String(kGeometryKey)).toByteArray();
    if (!g.isEmpty())
        QWidget::restoreGeometry(g);
}
