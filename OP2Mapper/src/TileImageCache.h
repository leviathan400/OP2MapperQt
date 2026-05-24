#pragma once

#include <QImage>
#include <QString>
#include <memory>
#include <vector>

namespace OP2Utility { class ResourceManager; struct TilesetSource; }

// Loads each tileset BMP once into a tall QImage (32 wide, 32*N tall, ARGB32).
// Paints any tile by blitting the appropriate 32-pixel-tall slice.
class TileImageCache
{
public:
    static constexpr int TileSize = 32;

    explicit TileImageCache(OP2Utility::ResourceManager &resources);

    // Build cache for the given tileset list (called on map load).
    // Returns list of tileset filenames that failed to load (for logging).
    QStringList load(const std::vector<OP2Utility::TilesetSource> &sources);

    // Returns nullptr if tileset is missing or imageIndex out of range.
    const QImage *tileset(std::size_t tilesetIndex) const;

    // Convenience: produce a single 32x32 QImage for one tile (copy).
    QImage tile(std::size_t tilesetIndex, std::size_t imageIndex) const;

private:
    OP2Utility::ResourceManager &m_resources;
    // index parallel to map.tilesetSources; null entry == missing
    std::vector<std::unique_ptr<QImage>> m_tilesets;
};
