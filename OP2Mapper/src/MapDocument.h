#pragma once

#include <QString>
#include <QStringList>
#include <memory>

namespace OP2Utility { class Map; class ResourceManager; enum class CellType; }

class TileImageCache;

class MapDocument
{
public:
    MapDocument();
    ~MapDocument();

    // Loads the map and its tilesets. Returns true on success.
    // On success, missingTilesets contains tileset filenames that couldn't be resolved.
    bool load(const QString &mapPath, const QString &op2Folder,
              QString &errorOut, QStringList &missingTilesets);

    bool isLoaded() const { return m_map != nullptr; }
    const OP2Utility::Map *map() const { return m_map.get(); }
    const TileImageCache *cache() const { return m_cache.get(); }

    int widthTiles() const;
    int heightTiles() const;

    // 0-based tile coordinates; safe to call when not loaded (returns FastPassible1).
    OP2Utility::CellType cellTypeAt(int x, int y) const;

private:
    std::unique_ptr<OP2Utility::ResourceManager> m_resources;
    std::unique_ptr<OP2Utility::Map> m_map;
    std::unique_ptr<TileImageCache> m_cache;
};
