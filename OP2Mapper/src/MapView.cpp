#include "MapView.h"

#include "MapDocument.h"
#include "MapScene.h"
#include "TileImageCache.h"

#include <QMouseEvent>
#include <QWheelEvent>

MapView::MapView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new MapScene(this);
    setScene(m_scene);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHint(QPainter::SmoothPixmapTransform, false);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
}

void MapView::setDocument(const MapDocument *doc)
{
    m_doc = doc;
    m_scene->setDocument(doc);
    resetTransform();
    centerOn(m_scene->sceneRect().center());
}

void MapView::setShowCellTypes(bool on)
{
    m_scene->setShowCellTypes(on);
}

void MapView::setShowGrid(bool on)
{
    m_scene->setShowGrid(on);
}

void MapView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);

    if (!m_doc || !m_doc->isLoaded()) {
        emit tileHovered(0, 0);
        return;
    }
    const QPointF scenePos = mapToScene(event->position().toPoint());
    const int tx = static_cast<int>(std::floor(scenePos.x() / TileImageCache::TileSize));
    const int ty = static_cast<int>(std::floor(scenePos.y() / TileImageCache::TileSize));
    if (tx < 0 || ty < 0 || tx >= m_doc->widthTiles() || ty >= m_doc->heightTiles()) {
        emit tileHovered(0, 0);
        return;
    }
    // OP2 uses 1-based map coordinates.
    emit tileHovered(tx + 1, ty + 1);
}

void MapView::leaveEvent(QEvent *event)
{
    emit tileHovered(0, 0);
    QGraphicsView::leaveEvent(event);
}

void MapView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
        scale(factor, factor);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}
