// MapRenderer: OP2Mapper3's five render modes, grids and coordinate labels,
// drawn at an integer tile size. Used by the map view, mini map, exports and
// the overview image.

#include "MapRenderer.h"

#include "CellTypeInfo.h"
#include "MapDocument.h"
#include "TileImageCache.h"

#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QPainter>
#include <QPixmap>

#include <algorithm>
#include <cmath>

namespace {

constexpr int T = TileImageCache::TileSize;

// GDI+ point sizes at 96 dpi -> pixels.
int ptToPx(double pt) { return std::max(1, static_cast<int>(std::lround(pt * 4.0 / 3.0))); }

// Mode-2 labels: text in the label colour with a black outline at the 8
// neighbour offsets, rendered once per (cell type, font size).
const QPixmap &labelPixmap(int cellType, int fontPx)
{
    static QHash<quint32, QPixmap> cache;
    const quint32 key = (static_cast<quint32>(cellType) << 16) | static_cast<quint32>(fontPx);
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();

    QFont f(QStringLiteral("Arial"));
    f.setPixelSize(fontPx);
    f.setBold(true);
    const QString text = CellTypeInfo::label(cellType);
    const QFontMetrics fm(f);
    const int w = fm.horizontalAdvance(text) + 3;
    const int h = fm.height() + 2;
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setFont(f);
        const int base = 1 + fm.ascent();
        p.setPen(Qt::black);
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx || dy)
                    p.drawText(1 + dx, base + dy, text);
        p.setPen(CellTypeInfo::labelColor(cellType));
        p.drawText(1, base, text);
    }
    return cache.insert(key, pm).value();
}

// Mode-4 labels: white 6-char enum name, cached per (cell type, font size).
const QPixmap &enumLabelPixmap(int cellType, int fontPx)
{
    static QHash<quint32, QPixmap> cache;
    const quint32 key = (static_cast<quint32>(cellType) << 16) | static_cast<quint32>(fontPx);
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();
    QFont f(QStringLiteral("Arial"));
    f.setPixelSize(fontPx);
    const QString text = CellTypeInfo::enumName(cellType).left(6);
    const QFontMetrics fm(f);
    QPixmap pm(fm.horizontalAdvance(text) + 2, fm.height());
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(0, fm.ascent(), text);
    p.end();
    return cache.insert(key, pm).value();
}

// Inclusive tile rect clipped to the map.
QRect clipToMap(const MapDocument &doc, const QRect &tiles)
{
    return tiles.intersected(QRect(0, 0, doc.widthTiles(), doc.heightTiles()));
}

} // namespace

namespace MapRenderer {

QString modeName(int mode)
{
    switch (mode) {
    case CellTypeOverlay: return QStringLiteral("CellTypeOverlay");
    case CellTypeOverlayAll: return QStringLiteral("CellTypeOverlayAll");
    case CellTypeBasic: return QStringLiteral("CellTypeBasic");
    case CellTypeAll: return QStringLiteral("CellTypeAll");
    default: return QStringLiteral("GameMap");
    }
}

void drawTiles(QPainter &p, const MapDocument &doc, const QRect &tilesIn, int tilePx,
               const QPoint &origin, int mode)
{
    if (!doc.isLoaded() || !doc.cache() || tilePx <= 0)
        return;
    const QRect tiles = clipToMap(doc, tilesIn);
    if (tiles.isEmpty())
        return;
    const TileImageCache &cache = *doc.cache();
    const double scale = tilePx / static_cast<double>(T);

    const bool drawImages = mode == GameMap || mode == CellTypeOverlay || mode == CellTypeOverlayAll;
    const bool labels2 = mode == CellTypeOverlayAll && tilePx >= 16;
    const bool overlay = mode == CellTypeOverlay || (mode == CellTypeOverlayAll && tilePx < 16);

    for (int y = tiles.top(); y <= tiles.bottom(); ++y) {
        for (int x = tiles.left(); x <= tiles.right(); ++x) {
            const QRect dst(origin.x() + x * tilePx, origin.y() + y * tilePx, tilePx, tilePx);
            const int ct = doc.cellTypeAt(x, y);
            if (drawImages) {
                const int m = doc.mappingIndexAt(x, y);
                const int ts = doc.tilesetIndexOfMapping(m);
                const int img = doc.imageIndexOfMapping(m);
                const QPixmap *strip = ts >= 0 ? cache.scaledStrip(static_cast<std::size_t>(ts), tilePx) : nullptr;
                if (strip && img >= 0 && (img + 1) * tilePx <= strip->height())
                    p.drawPixmap(dst.topLeft(), *strip, QRect(0, img * tilePx, tilePx, tilePx));
                else
                    p.drawPixmap(dst.topLeft(), cache.fallbackTile(ct, tilePx));
                if (overlay) {
                    QColor c = CellTypeInfo::overlayColor(ct);
                    c.setAlpha(CellTypeInfo::OverlayAlpha);
                    p.fillRect(dst, c);
                }
            } else {
                p.fillRect(dst, mode == CellTypeBasic ? CellTypeInfo::basicColor(ct)
                                                      : CellTypeInfo::detailedColor(ct));
            }
        }
    }

    if (mode == CellTypeBasic || mode == CellTypeAll) {
        // 1 px DarkGray outline per tile (GDI DrawRectangle covers s+1 px).
        p.setPen(QPen(QColor(169, 169, 169), 0));
        for (int y = tiles.top(); y <= tiles.bottom(); ++y)
            for (int x = tiles.left(); x <= tiles.right(); ++x)
                p.drawRect(origin.x() + x * tilePx, origin.y() + y * tilePx, tilePx, tilePx);
    }

    if (labels2) {
        const int fontPx = ptToPx(std::max(7.0 * scale, 6.0));
        for (int y = tiles.top(); y <= tiles.bottom(); ++y) {
            for (int x = tiles.left(); x <= tiles.right(); ++x) {
                const QPixmap &lp = labelPixmap(doc.cellTypeAt(x, y), fontPx);
                p.drawPixmap(origin.x() + x * tilePx + (tilePx - lp.width()) / 2,
                             origin.y() + y * tilePx + (tilePx - lp.height()) / 2, lp);
            }
        }
    }

    if (mode == CellTypeAll && scale >= 0.5) {
        const int fontPx = ptToPx(7.0 * scale);
        const int dy = static_cast<int>(12 * scale);
        for (int y = tiles.top(); y <= tiles.bottom(); ++y) {
            for (int x = tiles.left(); x <= tiles.right(); ++x) {
                p.drawPixmap(origin.x() + x * tilePx + 2, origin.y() + y * tilePx + dy,
                             enumLabelPixmap(doc.cellTypeAt(x, y), fontPx));
            }
        }
    }
}

void drawOverlays(QPainter &p, const MapDocument &doc, const QRect &tilesIn, int tilePx,
                  const QPoint &origin, const QRect &outputRect, const Overlays &o)
{
    if (!doc.isLoaded() || tilePx <= 0)
        return;
    const QRect tiles = clipToMap(doc, tilesIn);
    if (tiles.isEmpty())
        return;
    const int W = doc.widthTiles();
    const int H = doc.heightTiles();
    const double scale = tilePx / static_cast<double>(T);
    const QRect mapRect(origin.x(), origin.y(), W * tilePx, H * tilePx);

    if (o.grid) {
        p.setPen(QPen(Qt::black, 0));
        for (int x = tiles.left(); x <= tiles.right() + 1; ++x) {
            const int sx = origin.x() + x * tilePx;
            p.drawLine(sx, outputRect.top(), sx, outputRect.bottom());
        }
        for (int y = tiles.top(); y <= tiles.bottom() + 1; ++y) {
            const int sy = origin.y() + y * tilePx;
            p.drawLine(outputRect.left(), sy, outputRect.right(), sy);
        }
    }

    if (o.largeGrid && o.largeGridSize > 0) {
        const int step = o.largeGridSize;
        p.setPen(QPen(QColor(60, 60, 180, 180), 2));
        const int top = std::max(mapRect.top(), outputRect.top());
        const int bottom = std::min(mapRect.bottom(), outputRect.bottom());
        const int left = std::max(mapRect.left(), outputRect.left());
        const int right = std::min(mapRect.right(), outputRect.right());
        for (int x = (tiles.left() / step) * step; x <= tiles.right(); x += step) {
            const int sx = origin.x() + x * tilePx;
            if (sx >= mapRect.left() && sx <= mapRect.right())
                p.drawLine(sx, top, sx, bottom);
        }
        for (int y = (tiles.top() / step) * step; y <= tiles.bottom(); y += step) {
            const int sy = origin.y() + y * tilePx;
            if (sy >= mapRect.top() && sy <= mapRect.bottom())
                p.drawLine(left, sy, right, sy);
        }
    }

    if (o.coords && o.coordSpacing > 0) {
        const int sp = o.coordSpacing;
        QFont f(QStringLiteral("Arial"));
        f.setPixelSize(ptToPx(std::max(8.0 * scale, 7.0)));
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        for (int y = (tiles.top() / sp) * sp; y <= tiles.bottom(); y += sp) {
            for (int x = (tiles.left() / sp) * sp; x <= tiles.right(); x += sp) {
                if (x < 0 || y < 0 || x >= W || y >= H)
                    continue;
                const QString text = QStringLiteral("%1, %2").arg(x + 1).arg(y + 1);
                const int bx = origin.x() + x * tilePx;
                const int by = origin.y() + y * tilePx + fm.ascent();
                p.setPen(QColor(0, 0, 0, 200));
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        p.drawText(bx + dx, by + dy, text);
                p.setPen(QColor(255, 255, 255, 200));
                p.drawText(bx, by, text);
            }
        }
    }
}

// The whole map as one image; returns a null image if it can't be allocated.
QImage renderFullMap(const MapDocument &doc, int tilePx, int mode, const Overlays &o)
{
    if (!doc.isLoaded() || tilePx <= 0)
        return {};
    const QSize size(doc.widthTiles() * tilePx, doc.heightTiles() * tilePx);
    QImage img(size, QImage::Format_RGB32);
    if (img.isNull())
        return {};
    img.fill(Qt::black);
    QPainter p(&img);
    const QRect all(0, 0, doc.widthTiles(), doc.heightTiles());
    drawTiles(p, doc, all, tilePx, QPoint(0, 0), mode);
    drawOverlays(p, doc, all, tilePx, QPoint(0, 0), QRect(QPoint(0, 0), size), o);
    p.end();
    return img;
}

QImage mappingImage(const MapDocument &doc, int mapping)
{
    if (!doc.cache())
        return {};
    const int ts = doc.tilesetIndexOfMapping(mapping);
    const int img = doc.imageIndexOfMapping(mapping);
    if (ts < 0 || img < 0)
        return {};
    return doc.cache()->tile(static_cast<std::size_t>(ts), static_cast<std::size_t>(img));
}

// Falls back to the placeholder tile when the graphic is missing.
QImage rawTileImage(const MapDocument &doc, int x, int y)
{
    QImage img = mappingImage(doc, doc.mappingIndexAt(x, y));
    if (!img.isNull() || !doc.cache())
        return img;
    return doc.cache()->fallbackTile(doc.cellTypeAt(x, y), T).toImage();
}

} // namespace MapRenderer
