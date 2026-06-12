#include "MapDocument.h"

#include "TileClipboard.h"
#include "TileImageCache.h"

#include <OP2Utility.h>

#include <QCoreApplication>
#include <QUndoCommand>
#include <QUndoStack>

#include <exception>

namespace {

// Replicates Map::GetTileIndex (private in OP2Utility). Maps use 32-tile-wide
// column blocks, so storage is (upperX * height + y) * 32 + lowerX.
std::size_t tileIndex(const OP2Utility::Map &map, int x, int y)
{
    const std::size_t lowerX = static_cast<std::size_t>(x) & 0x1F;
    const std::size_t upperX = static_cast<std::size_t>(x) >> 5;
    return (upperX * map.HeightInTiles() + static_cast<std::size_t>(y)) * 32 + lowerX;
}

// ---- Undo commands ----------------------------------------------------------
//
// Each command captures the previous state lazily on the first redo() call.
// That makes construction cheap (just stores the desired new value) and means
// the document doesn't need to be probed twice when MapView pushes a command
// from inside a paint loop.

class PaintTileCommand : public QUndoCommand
{
public:
    PaintTileCommand(MapDocument *doc, int x, int y, int tilesetIdx, int imageIdx)
        : m_doc(doc), m_x(x), m_y(y),
          m_newTileset(tilesetIdx), m_newImage(imageIdx)
    {
        setText(QCoreApplication::translate("MapDocument", "Paint tile"));
    }

    void redo() override
    {
        if (!m_captured) {
            m_oldMappingIdx = m_doc->mappingIndexAt(m_x, m_y);
            m_captured = true;
        }
        m_doc->applyPaintTileGraphic(m_x, m_y, m_newTileset, m_newImage);
    }

    void undo() override
    {
        m_doc->setMappingIndexAt(m_x, m_y, m_oldMappingIdx);
    }

private:
    MapDocument *m_doc;
    int m_x;
    int m_y;
    int m_newTileset;
    int m_newImage;
    unsigned m_oldMappingIdx = 0;
    bool m_captured = false;
};

class PaintCellTypeCommand : public QUndoCommand
{
public:
    PaintCellTypeCommand(MapDocument *doc, int x, int y, OP2Utility::CellType ct)
        : m_doc(doc), m_x(x), m_y(y), m_newType(ct)
    {
        setText(QCoreApplication::translate("MapDocument", "Paint cell type"));
    }

    void redo() override
    {
        if (!m_captured) {
            m_oldType = m_doc->cellTypeAt(m_x, m_y);
            m_captured = true;
        }
        m_doc->setCellTypeAt(m_x, m_y, m_newType);
    }

    void undo() override
    {
        m_doc->setCellTypeAt(m_x, m_y, m_oldType);
    }

private:
    MapDocument *m_doc;
    int m_x;
    int m_y;
    OP2Utility::CellType m_newType;
    OP2Utility::CellType m_oldType{};
    bool m_captured = false;
};

class PlaceTilesCommand : public QUndoCommand
{
public:
    PlaceTilesCommand(MapDocument *doc, int x0, int y0, TileClipboard clip)
        : m_doc(doc), m_x0(x0), m_y0(y0), m_clip(std::move(clip))
    {
        setText(QCoreApplication::translate("MapDocument", "Paste tiles"));
    }

    void redo() override
    {
        const int W = m_clip.width();
        const int H = m_clip.height();
        const int mapW = m_doc->widthTiles();
        const int mapH = m_doc->heightTiles();

        if (!m_captured) {
            m_old.assign(static_cast<std::size_t>(W * H), {});
            for (int dy = 0; dy < H; ++dy) {
                for (int dx = 0; dx < W; ++dx) {
                    const int x = m_x0 + dx;
                    const int y = m_y0 + dy;
                    auto &o = m_old[dy * W + dx];
                    // Mirror redo's bounds exactly (including the protected
                    // bottom row) so undo restores precisely the cells redo
                    // wrote — nothing more, nothing less.
                    if (x < 0 || y < 0 || x >= mapW || y >= mapH - 1) {
                        o.outOfBounds = true;
                        continue;
                    }
                    o.mappingIdx = m_doc->mappingIndexAt(x, y);
                    o.cellType = static_cast<int>(m_doc->cellTypeAt(x, y));
                }
            }
            m_captured = true;
        }

        const bool applyCellTypes = m_clip.hasCellTypes();
        for (int dy = 0; dy < H; ++dy) {
            for (int dx = 0; dx < W; ++dx) {
                const auto &c = m_clip.at(dx, dy);
                if (c.tilesetIdx < 0 || c.imageIdx < 0)
                    continue; // skip-cell
                const int x = m_x0 + dx;
                const int y = m_y0 + dy;
                if (x < 0 || y < 0 || x >= mapW || y >= mapH - 1)
                    continue; // bottom-row protection
                m_doc->applyPaintTileGraphic(x, y, c.tilesetIdx, c.imageIdx);
                if (applyCellTypes) {
                    m_doc->setCellTypeAt(x, y,
                        static_cast<OP2Utility::CellType>(c.cellType));
                }
            }
        }
    }

    void undo() override
    {
        const int W = m_clip.width();
        const int H = m_clip.height();
        const bool applyCellTypes = m_clip.hasCellTypes();
        for (int dy = 0; dy < H; ++dy) {
            for (int dx = 0; dx < W; ++dx) {
                const auto &o = m_old[dy * W + dx];
                if (o.outOfBounds)
                    continue;
                const int x = m_x0 + dx;
                const int y = m_y0 + dy;
                m_doc->setMappingIndexAt(x, y, o.mappingIdx);
                if (applyCellTypes) {
                    m_doc->setCellTypeAt(x, y,
                        static_cast<OP2Utility::CellType>(o.cellType));
                }
            }
        }
    }

private:
    struct OldCell {
        unsigned mappingIdx = 0;
        int cellType = 0;
        bool outOfBounds = false;
    };

    MapDocument *m_doc;
    int m_x0;
    int m_y0;
    TileClipboard m_clip;
    std::vector<OldCell> m_old;
    bool m_captured = false;
};

} // namespace

MapDocument::MapDocument(QObject *parent)
    : QObject(parent)
    , m_undoStack(new QUndoStack(this))
{
    connect(m_undoStack, &QUndoStack::cleanChanged,
            this, [this](bool clean) { emit dirtyChanged(!clean); });
}

MapDocument::~MapDocument() = default;

bool MapDocument::load(const QString &mapPath, const QString &op2Folder,
                       QString &errorOut, QStringList &missingTilesets)
{
    try {
        auto resources = std::make_unique<OP2Utility::ResourceManager>(op2Folder.toStdString());
        auto map = std::make_unique<OP2Utility::Map>(
            OP2Utility::Map::ReadMap(mapPath.toStdString()));
        auto cache = std::make_unique<TileImageCache>(*resources);
        missingTilesets = cache->load(map->tilesetSources);

        m_resources = std::move(resources);
        m_map = std::move(map);
        m_cache = std::move(cache);
        m_currentPath = mapPath;
        m_undoStack->clear();
        m_undoStack->setClean();
        return true;
    } catch (const std::exception &e) {
        errorOut = QString::fromUtf8(e.what());
        m_map.reset();
        m_cache.reset();
        m_resources.reset();
        return false;
    }
}

void MapDocument::reset()
{
    m_undoStack->clear();
    m_undoStack->setClean();
    m_map.reset();
    m_cache.reset();
    m_resources.reset();
    m_currentPath.clear();
}

int MapDocument::widthTiles() const
{
    return m_map ? static_cast<int>(m_map->WidthInTiles()) : 0;
}

int MapDocument::heightTiles() const
{
    return m_map ? static_cast<int>(m_map->HeightInTiles()) : 0;
}

OP2Utility::CellType MapDocument::cellTypeAt(int x, int y) const
{
    if (!m_map || x < 0 || y < 0 || x >= widthTiles() || y >= heightTiles())
        return OP2Utility::CellType::FastPassible1;
    return m_map->GetCellType(static_cast<std::size_t>(x), static_cast<std::size_t>(y));
}

unsigned MapDocument::mappingIndexAt(int x, int y) const
{
    if (!m_map || x < 0 || y < 0 || x >= widthTiles() || y >= heightTiles())
        return 0;
    const std::size_t idx = tileIndex(*m_map, x, y);
    if (idx >= m_map->tiles.size())
        return 0;
    return m_map->tiles[idx].tileMappingIndex;
}

void MapDocument::setMappingIndexAt(int x, int y, unsigned mappingIndex)
{
    if (!m_map || x < 0 || y < 0 || x >= widthTiles() || y >= heightTiles())
        return;
    const std::size_t idx = tileIndex(*m_map, x, y);
    if (idx >= m_map->tiles.size())
        return;
    if (m_map->tiles[idx].tileMappingIndex == mappingIndex)
        return;
    m_map->tiles[idx].tileMappingIndex = mappingIndex;
    emit tileChanged(x, y);
}

void MapDocument::setCellTypeAt(int x, int y, OP2Utility::CellType ct)
{
    if (!m_map || x < 0 || y < 0 || x >= widthTiles() || y >= heightTiles())
        return;
    const std::size_t sx = static_cast<std::size_t>(x);
    const std::size_t sy = static_cast<std::size_t>(y);
    if (m_map->GetCellType(sx, sy) == ct)
        return;
    m_map->SetCellType(ct, sx, sy);
    emit tileChanged(x, y);
}

bool MapDocument::applyPaintTileGraphic(int x, int y, int tilesetIdx, int imageIdx)
{
    if (!m_map || x < 0 || y < 0 || x >= widthTiles() || y >= heightTiles())
        return false;
    if (tilesetIdx < 0 || imageIdx < 0)
        return false;

    int found = -1;
    for (std::size_t i = 0; i < m_map->tileMappings.size(); ++i) {
        const auto &m = m_map->tileMappings[i];
        if (m.tilesetIndex == tilesetIdx && m.tileGraphicIndex == imageIdx) {
            found = static_cast<int>(i);
            break;
        }
    }

    // tileMappingIndex is an 11-bit bitfield → 2048 max entries.
    constexpr std::size_t kMaxMappings = 2048;
    if (found < 0) {
        if (m_map->tileMappings.size() >= kMaxMappings)
            return false;
        OP2Utility::TileMapping nm{};
        nm.tilesetIndex = static_cast<uint16_t>(tilesetIdx);
        nm.tileGraphicIndex = static_cast<uint16_t>(imageIdx);
        nm.animationCount = 1;
        nm.animationDelay = 0;
        m_map->tileMappings.push_back(nm);
        found = static_cast<int>(m_map->tileMappings.size()) - 1;
    }

    const std::size_t idx = tileIndex(*m_map, x, y);
    if (idx >= m_map->tiles.size())
        return false;
    if (static_cast<int>(m_map->tiles[idx].tileMappingIndex) == found)
        return false;
    m_map->tiles[idx].tileMappingIndex = static_cast<unsigned>(found);
    emit tileChanged(x, y);
    return true;
}

bool MapDocument::wouldPaintTileChange(int x, int y, int tilesetIdx, int imageIdx) const
{
    // Bounds + bottom-row protection in one check: y == heightTiles()-1 is
    // OP2's special impassable border row, which the editor never modifies.
    if (!m_map || x < 0 || y < 0 || x >= widthTiles() || y >= heightTiles() - 1)
        return false;
    if (tilesetIdx < 0 || imageIdx < 0)
        return false;
    // If a mapping for (tilesetIdx, imageIdx) already exists and the tile
    // already points at it, painting is a no-op. Critical for drag-paints
    // that sweep over already-painted tiles.
    for (std::size_t i = 0; i < m_map->tileMappings.size(); ++i) {
        const auto &m = m_map->tileMappings[i];
        if (m.tilesetIndex == tilesetIdx && m.tileGraphicIndex == imageIdx)
            return mappingIndexAt(x, y) != i;
    }
    // No existing mapping — painting will insert one, unless the 11-bit
    // tileMappingIndex bitfield is already saturated (2048 entries).
    return m_map->tileMappings.size() < 2048;
}

bool MapDocument::wouldPaintCellTypeChange(int x, int y, OP2Utility::CellType ct) const
{
    // Same bounds + bottom-row-protection combo as wouldPaintTileChange.
    if (!m_map || x < 0 || y < 0 || x >= widthTiles() || y >= heightTiles() - 1)
        return false;
    return cellTypeAt(x, y) != ct;
}

void MapDocument::pushPaintTile(int x, int y, int tilesetIdx, int imageIdx)
{
    if (!wouldPaintTileChange(x, y, tilesetIdx, imageIdx))
        return;
    m_undoStack->push(new PaintTileCommand(this, x, y, tilesetIdx, imageIdx));
}

void MapDocument::pushPaintCellType(int x, int y, OP2Utility::CellType ct)
{
    if (!wouldPaintCellTypeChange(x, y, ct))
        return;
    m_undoStack->push(new PaintCellTypeCommand(this, x, y, ct));
}

void MapDocument::pushPlaceTiles(int x0, int y0, const TileClipboard &clip)
{
    if (!m_map || clip.isEmpty())
        return;
    m_undoStack->push(new PlaceTilesCommand(this, x0, y0, clip));
}

bool MapDocument::isDirty() const
{
    return !m_undoStack->isClean();
}

bool MapDocument::save(const QString &path, QString &errorOut)
{
    if (!m_map) {
        errorOut = QStringLiteral("No map loaded");
        return false;
    }
    try {
        m_map->Write(path.toStdString());
        m_currentPath = path;
        m_undoStack->setClean();
        return true;
    } catch (const std::exception &e) {
        errorOut = QString::fromUtf8(e.what());
        return false;
    }
}
