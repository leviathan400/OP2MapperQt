#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QString>

class MapDocument;
class QPainter;

// Rendering shared by the map view, mini map, exports and previews. Ported
// from OP2Mapper3 MapRenderer.vb. Everything is drawn at an integer tile
// size in device pixels (tilePx), so tiles never leave seams.
namespace MapRenderer {

enum Mode { GameMap = 0, CellTypeOverlay, CellTypeOverlayAll, CellTypeBasic, CellTypeAll };
QString modeName(int mode); // enum name, e.g. "CellTypeOverlayAll"
inline bool isRestrictedMode(int mode) { return mode == CellTypeBasic || mode == CellTypeAll; }

struct Overlays {
    bool grid = false;
    bool largeGrid = false;
    bool coords = false;
    int largeGridSize = 32;
    int coordSpacing = 32;
};

// Draws the render-mode layer for the tiles in `tiles` (inclusive tile
// rect, clipped to the map). Tile (x, y) lands at origin + (x, y) * tilePx.
void drawTiles(QPainter &p, const MapDocument &doc, const QRect &tiles, int tilePx,
               const QPoint &origin, int mode);

// Small grid, large grid and coordinate labels for the visible tiles.
// `outputRect` is the painted area in device pixels (the small grid spans it).
void drawOverlays(QPainter &p, const MapDocument &doc, const QRect &tiles, int tilePx,
                  const QPoint &origin, const QRect &outputRect, const Overlays &o);

// Whole map at tilePx per tile (for exports, mini map, overview).
QImage renderFullMap(const MapDocument &doc, int tilePx, int mode, const Overlays &o);

// One map tile's 32x32 graphic (ignores render mode), or the fallback tile.
QImage rawTileImage(const MapDocument &doc, int x, int y);
// A mapping's 32x32 graphic or a null image.
QImage mappingImage(const MapDocument &doc, int mapping);

} // namespace MapRenderer
