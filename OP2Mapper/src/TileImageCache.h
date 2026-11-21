#pragma once

#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

namespace OP2Utility { class ResourceManager; struct TilesetSource; }

// Decoded tileset graphics for one map. Each tileset BMP becomes a tall
// ARGB32 strip (32 wide, 32*N tall). Loading follows OP2Mapper3: loose
// files from the resolved tileset folder first, then the ResourceManager
// (.vol archives); palette index 0 becomes grey (192,192,192) when the
// palette's entry 0 is white; sources sharing a filename share one strip.
// Also caches nearest-neighbour scaled copies for drawing at any tile size.
class TileImageCache
{
public:
    static constexpr int TileSize = 32;

    // resources may be null; tilesetDir may be empty (folder not found).
    TileImageCache(OP2Utility::ResourceManager *resources, const QString &tilesetDir);

    // Build the cache for the map's tileset list. Returns the names (with
    // .bmp) of non-empty sources that couldn't be loaded.
    QStringList load(const std::vector<OP2Utility::TilesetSource> &sources);

    // Strip for a tileset index, or nullptr if missing/empty.
    const QImage *tileset(std::size_t tilesetIndex) const;
    int tileCount(std::size_t tilesetIndex) const;

    // Non-empty sources and how many of them loaded (>= 1 tile).
    int sourceCount() const { return m_nonEmptySources; }
    int loadedCount() const { return m_loadedSources; }

    // A copy of one 32x32 tile, or a null image.
    QImage tile(std::size_t tilesetIndex, std::size_t imageIndex) const;

    // The strip scaled so each tile is tilePx x tilePx (nearest neighbour).
    // tilePx == 32 returns the original. Cached; nullptr if missing.
    const QPixmap *scaledStrip(std::size_t tilesetIndex, int tilePx) const;

    // Placeholder for missing tiles: Basic cell-type colour, grey border + X.
    const QPixmap &fallbackTile(int cellType, int tilePx) const;

private:
    OP2Utility::ResourceManager *m_resources;
    QString m_tilesetDir;
    std::vector<std::shared_ptr<QImage>> m_tilesets; // parallel to sources
    int m_nonEmptySources = 0;
    int m_loadedSources = 0;
    mutable QHash<quint64, QPixmap> m_scaled;   // (strip ptr id, px)
    mutable QHash<quint32, QPixmap> m_fallback; // (cellType, px)
};
