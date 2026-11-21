#include "TilesetWindow.h"

#include "Editor.h"
#include "MapDocument.h"
#include "TileImageCache.h"
#include "TilesetGrid.h"

#include <QComboBox>
#include <QScrollArea>
#include <QSignalBlocker>

TilesetWindow::TilesetWindow(Editor *editor, QWidget *mainWindow)
    : ToolPalette(QStringLiteral("Tileset"), QStringLiteral("fTileSet"), mainWindow)
    , m_editor(editor)
{
    setWindowTitle(tr("Tile Set"));
    setFixedSize(248, 445);

    m_combo = new QComboBox(this);
    m_combo->setGeometry(12, 12, 224, 21);

    m_scroll = new QScrollArea(this);
    m_scroll->setGeometry(12, 39, 224, 395);
    m_scroll->setFrameShape(QFrame::Box);
    m_scroll->setLineWidth(1);
    m_scroll->setWidgetResizable(false);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: #A9A9A9; }"));
    m_grid = new TilesetGrid(m_scroll);
    m_scroll->setWidget(m_grid);

    connect(m_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { loadCurrentTileset(); });
    connect(m_grid, &TilesetGrid::tileClicked, this, &TilesetWindow::onTileClicked);
    connect(m_editor, &Editor::tilesetSelectionCleared, this, &TilesetWindow::clearSelection);
}

void TilesetWindow::reloadFromDocument()
{
    const QSignalBlocker block(m_combo);
    m_combo->clear();
    m_sourceIndices.clear();
    m_currentTilesetIdx = -1;
    const MapDocument *doc = m_editor->document();
    if (doc && doc->map()) {
        const auto &sources = doc->map()->tilesetSources;
        for (std::size_t i = 0; i < sources.size(); ++i) {
            if (sources[i].IsEmpty())
                continue;
            m_combo->addItem(QString::fromStdString(sources[i].tilesetFilename));
            m_sourceIndices.push_back(static_cast<int>(i));
        }
    }
    if (m_combo->count() > 0)
        m_combo->setCurrentIndex(0);
    loadCurrentTileset();
}

void TilesetWindow::loadCurrentTileset()
{
    const int row = m_combo->currentIndex();
    const MapDocument *doc = m_editor->document();
    if (row < 0 || row >= static_cast<int>(m_sourceIndices.size()) || !doc || !doc->cache()) {
        m_currentTilesetIdx = -1;
        m_grid->setTilesetImage(nullptr);
        return;
    }
    m_currentTilesetIdx = m_sourceIndices[static_cast<std::size_t>(row)];
    m_grid->setTilesetImage(doc->cache()->tileset(static_cast<std::size_t>(m_currentTilesetIdx)));
}

void TilesetWindow::clearSelection()
{
    m_grid->setSelectedTile(-1);
}

void TilesetWindow::onTileClicked(int index)
{
    if (m_currentTilesetIdx < 0)
        return;
    if (m_editor->pickTilesetTile(m_currentTilesetIdx, index))
        m_grid->setSelectedTile(index);
}
