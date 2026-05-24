#include "MapDocument.h"

#include "TileImageCache.h"

#include <OP2Utility.h>

#include <exception>

MapDocument::MapDocument() = default;
MapDocument::~MapDocument() = default;

bool MapDocument::load(const QString &mapPath, const QString &op2Folder,
                       QString &errorOut, QStringList &missingTilesets)
{
    try {
        m_resources = std::make_unique<OP2Utility::ResourceManager>(op2Folder.toStdString());
        m_map = std::make_unique<OP2Utility::Map>(
            OP2Utility::Map::ReadMap(mapPath.toStdString()));
        m_cache = std::make_unique<TileImageCache>(*m_resources);
        missingTilesets = m_cache->load(m_map->tilesetSources);
        return true;
    } catch (const std::exception &e) {
        errorOut = QString::fromUtf8(e.what());
        m_map.reset();
        m_cache.reset();
        m_resources.reset();
        return false;
    }
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
