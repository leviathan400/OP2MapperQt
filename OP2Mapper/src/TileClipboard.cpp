#include "TileClipboard.h"

#include "MapDocument.h"

#include <OP2Utility.h>

#include <algorithm>

void TileClipboard::clear()
{
    m_width = 0;
    m_height = 0;
    m_cells.clear();
    m_sourceTilesetName.clear();
    m_hasCellTypes = true;
}

void TileClipboard::resetForRegion(int w, int h, QString sourceTilesetName)
{
    clear();
    if (w <= 0 || h <= 0)
        return;
    m_width = w;
    m_height = h;
    m_cells.assign(static_cast<std::size_t>(w * h), Cell{});
    m_sourceTilesetName = std::move(sourceTilesetName);
    m_hasCellTypes = false;
}

void TileClipboard::captureFrom(const MapDocument &doc, int x0, int y0, int w, int h)
{
    clear();
    if (!doc.isLoaded() || w <= 0 || h <= 0)
        return;

    const auto *map = doc.map();
    const int mapW = doc.widthTiles();
    const int mapH = doc.heightTiles();

    // Clip to map bounds. Skip the capture entirely if the rectangle is
    // wholly outside.
    const int cx0 = std::max(0, x0);
    const int cy0 = std::max(0, y0);
    const int cx1 = std::min(mapW, x0 + w);
    const int cy1 = std::min(mapH, y0 + h);
    if (cx1 <= cx0 || cy1 <= cy0)
        return;

    // Preserve the original rectangle's dimensions — out-of-map cells are
    // captured as "skip" (tilesetIdx = -1) so paste preserves the shape.
    m_width = w;
    m_height = h;
    m_cells.assign(static_cast<std::size_t>(w * h), Cell{});
    m_hasCellTypes = true;

    for (int y = cy0; y < cy1; ++y) {
        for (int x = cx0; x < cx1; ++x) {
            Cell c;
            c.originalMappingIdx = doc.mappingIndexAt(x, y);
            if (c.originalMappingIdx < map->tileMappings.size()) {
                const auto &m = map->tileMappings[c.originalMappingIdx];
                c.tilesetIdx = static_cast<int>(m.tilesetIndex);
                c.imageIdx = static_cast<int>(m.tileGraphicIndex);
                if (m_sourceTilesetName.isEmpty()
                    && c.tilesetIdx >= 0
                    && static_cast<std::size_t>(c.tilesetIdx) < map->tilesetSources.size()) {
                    const std::string fn = map->tilesetSources[c.tilesetIdx].tilesetFilename;
                    // VB writer captures a 6-char prefix (e.g. "well00").
                    m_sourceTilesetName = QString::fromStdString(fn).left(6);
                }
            }
            c.cellType = static_cast<int>(doc.cellTypeAt(x, y));
            const int lx = x - x0;
            const int ly = y - y0;
            m_cells[ly * w + lx] = c;
        }
    }

    if (m_sourceTilesetName.isEmpty())
        m_sourceTilesetName = QStringLiteral("well00");
}
