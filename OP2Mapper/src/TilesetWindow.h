#pragma once

#include "ToolPalette.h"

#include <vector>

class Editor;
class TilesetGrid;
class QComboBox;
class QScrollArea;

// "Tile Set" palette (OP2Mapper3 fTileSet, tool 1).
class TilesetWindow : public ToolPalette
{
    Q_OBJECT
public:
    TilesetWindow(Editor *editor, QWidget *mainWindow);

    void reloadFromDocument(); // combo contents from the map's tilesets
    void clearSelection();

private:
    void loadCurrentTileset();
    void onTileClicked(int index);

    Editor *m_editor;
    QComboBox *m_combo = nullptr;
    QScrollArea *m_scroll = nullptr;
    TilesetGrid *m_grid = nullptr;
    std::vector<int> m_sourceIndices;
    int m_currentTilesetIdx = -1;
};
