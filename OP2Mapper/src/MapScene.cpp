#include "MapScene.h"

#include "CellTypeInfo.h"
#include "MapDocument.h"
#include "TileImageCache.h"

#include <OP2Utility.h>

#include <QFont>
#include <QPainter>

namespace { constexpr int T = TileImageCache::TileSize; }

MapScene::MapScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setBackgroundBrush(QColor(20, 20, 24));
}

void MapScene::setDocument(const MapDocument *doc)
{
    m_doc = doc;
    if (doc && doc->isLoaded()) {
        setSceneRect(0, 0, doc->widthTiles() * T, doc->heightTiles() * T);
    } else {
        setSceneRect(QRectF());
    }
    invalidate();
}

void MapScene::setShowCellTypes(bool on)
{
    if (m_showCellTypes == on)
        return;
    m_showCellTypes = on;
    invalidate();
}

void MapScene::setShowGrid(bool on)
{
    if (m_showGrid == on)
        return;
    m_showGrid = on;
    invalidate();
}

void MapScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);
    if (!m_doc || !m_doc->isLoaded() || !m_doc->cache())
        return;

    const auto *map = m_doc->map();
    const auto *cache = m_doc->cache();

    const int mapW = m_doc->widthTiles();
    const int mapH = m_doc->heightTiles();

    // Clip the dirty rect to the tile grid.
    const int x0 = std::max(0, static_cast<int>(std::floor(rect.left() / T)));
    const int y0 = std::max(0, static_cast<int>(std::floor(rect.top() / T)));
    const int x1 = std::min(mapW, static_cast<int>(std::ceil(rect.right() / T)));
    const int y1 = std::min(mapH, static_cast<int>(std::ceil(rect.bottom() / T)));

    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t tsIdx = map->GetTilesetIndex(x, y);
            const std::size_t imgIdx = map->GetImageIndex(x, y);
            const QImage *strip = cache->tileset(tsIdx);
            if (!strip) {
                painter->fillRect(x * T, y * T, T, T, QColor(64, 0, 64));
                continue;
            }
            const int srcY = static_cast<int>(imgIdx) * T;
            if (srcY + T > strip->height()) {
                painter->fillRect(x * T, y * T, T, T, QColor(64, 32, 0));
                continue;
            }
            painter->drawImage(QPoint(x * T, y * T), *strip,
                               QRect(0, srcY, T, T));
        }
    }

    if (m_showGrid) {
        QPen pen(QColor(0, 0, 0, 200));
        pen.setCosmetic(true); // stay 1px regardless of zoom
        painter->setPen(pen);
        for (int x = x0; x <= x1; ++x)
            painter->drawLine(x * T, y0 * T, x * T, y1 * T);
        for (int y = y0; y <= y1; ++y)
            painter->drawLine(x0 * T, y * T, x1 * T, y * T);
    }

    if (m_showCellTypes) {
        QFont font = painter->font();
        font.setPixelSize(10);
        font.setBold(true);
        painter->setFont(font);

        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const auto ct = map->GetCellType(x, y);
                const QRect cell(x * T, y * T, T, T);
                painter->fillRect(cell, CellTypeInfo::color(ct));
                painter->setPen(Qt::black);
                painter->drawText(cell.adjusted(1, 1, 1, 1),
                                  Qt::AlignCenter, CellTypeInfo::label(ct));
                painter->setPen(Qt::white);
                painter->drawText(cell, Qt::AlignCenter, CellTypeInfo::label(ct));
            }
        }
    }
}
