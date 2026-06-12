#pragma once

#include <QWidget>

class MapDocument;
class TilesetGrid;
class QComboBox;
class QScrollArea;
class QLabel;

// "Tile Set" tool palette (VB fTileSet equivalent): a combo of the map's
// tileset sources over a scrollable TilesetGrid. Picking a tile arms the
// map view's tile-paint mode via tileSelected. Floating Qt::Tool window;
// geometry persists in QSettings across runs.
class TilesetWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TilesetWindow(QWidget *parent = nullptr);

    void setDocument(MapDocument *doc);

signals:
    // Emits (-1, -1) when the user clears the selection (right-click in the grid).
    void tileSelected(int tilesetIdx, int imageIdx);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTilesetIndexChanged(int comboRow);
    void onGridTileClicked(int imageIdx);
    void onGridClearRequested();

private:
    void populateCombo();
    void loadCurrentTileset();
    void saveGeometry();
    void restoreGeometry();

    MapDocument *m_doc = nullptr;
    QComboBox *m_combo = nullptr;
    QScrollArea *m_scroll = nullptr;
    TilesetGrid *m_grid = nullptr;
    QLabel *m_emptyLabel = nullptr;

    // For each combo row, the index into map->tilesetSources it points at
    // (non-empty sources only).
    std::vector<int> m_sourceIndices;
    int m_currentTilesetIdx = -1;
};
