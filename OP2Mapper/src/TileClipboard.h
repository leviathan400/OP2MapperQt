#pragma once

#include <QString>

#include <vector>

namespace OP2Utility { enum class CellType; }

class MapDocument;

// In-memory clipboard for rectangular tile regions. Stores tiles in a
// resolved form — (tilesetIdx, imageIdx) pairs rather than raw mapping
// indices — so a paste into a different map can re-resolve mappings
// against that map's own tileMappings table.
class TileClipboard
{
public:
    struct Cell {
        int tilesetIdx = -1;       // -1 means "skip" on paste
        int imageIdx = -1;
        int cellType = 0;          // OP2Utility::CellType raw value
        unsigned originalMappingIdx = 0; // raw tileMappingIndex from source map
                                         // (for community-compat tile-group JSON export)
    };

    bool isEmpty() const { return m_width <= 0 || m_height <= 0; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    const Cell &at(int x, int y) const { return m_cells[y * m_width + x]; }

    // Source-map tileset filename (6-char prefix like "well00") of the first
    // valid cell — written into tile-group JSON headers.
    QString sourceTilesetName() const { return m_sourceTilesetName; }

    // Whether paste should also overwrite cell-type values. Copy-from-map sets
    // this true; tile-group loads set it false (groups don't carry cell types).
    bool hasCellTypes() const { return m_hasCellTypes; }
    void setHasCellTypes(bool on) { m_hasCellTypes = on; }

    void clear();

    // Resize / reset to a blank w*h region. Use when populating from sources
    // other than captureFrom (e.g. tile-group loader). hasCellTypes defaults
    // to false in that case.
    void resetForRegion(int w, int h, QString sourceTilesetName);
    Cell &mutableCell(int x, int y) { return m_cells[y * m_width + x]; }

    // Captures the (x0, y0, w, h) region from the document. Coordinates outside
    // the map are clipped. After capture, isEmpty() == false iff anything was
    // captured.
    void captureFrom(const MapDocument &doc, int x0, int y0, int w, int h);

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<Cell> m_cells;
    QString m_sourceTilesetName;
    bool m_hasCellTypes = true;
};
