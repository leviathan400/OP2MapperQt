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
    if (m_tileChangedConn) {
        QObject::disconnect(m_tileChangedConn);
        m_tileChangedConn = {};
    }
    m_doc = doc;
    if (doc && doc->isLoaded()) {
        setSceneRect(0, 0, doc->widthTiles() * T, doc->heightTiles() * T);
        m_tileChangedConn = connect(doc, &MapDocument::tileChanged,
                                    this, &MapScene::invalidateTile);
    } else {
        setSceneRect(QRectF());
    }
    invalidate();
}

void MapScene::setRenderMode(RenderMode mode)
{
    if (m_renderMode == mode)
        return;
    m_renderMode = mode;
    invalidate();
}

void MapScene::setShowGrid(bool on)
{
    if (m_showGrid == on)
        return;
    m_showGrid = on;
    invalidate();
}

void MapScene::setShowLargeGrid(bool on)
{
    if (m_showLargeGrid == on)
        return;
    m_showLargeGrid = on;
    invalidate();
}

void MapScene::setShowCoords(bool on)
{
    if (m_showCoords == on)
        return;
    m_showCoords = on;
    invalidate();
}

void MapScene::invalidateTile(int x, int y)
{
    invalidate(QRectF(x * T, y * T, T, T), QGraphicsScene::BackgroundLayer);
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

    const bool drawTiles = (m_renderMode == RenderMode::GameMap
                          || m_renderMode == RenderMode::CellTypeOverlay
                          || m_renderMode == RenderMode::CellTypeOverlayAll);
    const bool drawCellColors = (m_renderMode != RenderMode::GameMap);
    const bool drawCellLabels = (m_renderMode == RenderMode::CellTypeOverlayAll
                              || m_renderMode == RenderMode::CellTypeAll);
    const bool solidCellColors = (m_renderMode == RenderMode::CellTypeBasic
                               || m_renderMode == RenderMode::CellTypeAll);

    if (drawTiles) {
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
    }

    if (drawCellColors) {
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const auto ct = map->GetCellType(x, y);
                const QRect cell(x * T, y * T, T, T);
                QColor c = CellTypeInfo::color(ct);
                if (solidCellColors) {
                    c.setAlpha(255);
                    painter->fillRect(cell, c);
                    painter->setPen(QColor(40, 40, 40));
                    painter->drawRect(cell);
                } else {
                    painter->fillRect(cell, c);
                }
            }
        }
    }

    if (drawCellLabels) {
        QFont font = painter->font();
        font.setPixelSize(10);
        font.setBold(true);
        painter->setFont(font);
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const auto ct = map->GetCellType(x, y);
                const QRect cell(x * T, y * T, T, T);
                painter->setPen(Qt::black);
                painter->drawText(cell.adjusted(1, 1, 1, 1),
                                  Qt::AlignCenter, CellTypeInfo::label(ct));
                painter->setPen(Qt::white);
                painter->drawText(cell, Qt::AlignCenter, CellTypeInfo::label(ct));
            }
        }
    }

    if (m_showGrid) {
        QPen pen(QColor(0, 0, 0, 200));
        pen.setCosmetic(true);
        painter->setPen(pen);
        for (int x = x0; x <= x1; ++x)
            painter->drawLine(x * T, y0 * T, x * T, y1 * T);
        for (int y = y0; y <= y1; ++y)
            painter->drawLine(x0 * T, y * T, x1 * T, y * T);
    }

    if (m_showLargeGrid) {
        constexpr int kLargeStep = 8;
        QPen pen(QColor(255, 200, 0, 220));
        pen.setCosmetic(true);
        pen.setWidth(2);
        painter->setPen(pen);
        const int lx0 = ((x0 + kLargeStep - 1) / kLargeStep) * kLargeStep;
        const int ly0 = ((y0 + kLargeStep - 1) / kLargeStep) * kLargeStep;
        for (int x = lx0; x <= x1; x += kLargeStep)
            painter->drawLine(x * T, y0 * T, x * T, y1 * T);
        for (int y = ly0; y <= y1; y += kLargeStep)
            painter->drawLine(x0 * T, y * T, x1 * T, y * T);
    }

    if (m_showCoords) {
        QFont font = painter->font();
        font.setPixelSize(9);
        font.setBold(false);
        painter->setFont(font);
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const QRect cell(x * T, y * T, T, T);
                const QString text = QStringLiteral("%1,%2").arg(x + 1).arg(y + 1);
                painter->setPen(QColor(0, 0, 0, 230));
                painter->drawText(cell.adjusted(2, 1, 0, 0),
                                  Qt::AlignTop | Qt::AlignLeft, text);
                painter->setPen(QColor(255, 255, 255, 230));
                painter->drawText(cell.adjusted(1, 0, 0, 0),
                                  Qt::AlignTop | Qt::AlignLeft, text);
            }
        }
    }
}
