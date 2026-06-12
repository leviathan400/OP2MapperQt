#include "MapView.h"

#include "MapDocument.h"
#include "MapScene.h"
#include "TileClipboard.h"
#include "TileImageCache.h"

#include <OP2Utility.h>

#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QUndoStack>
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

    // Dashed yellow selection overlay. Lives on the scene; hidden until the
    // user makes a selection.
    QPen selPen(QColor(255, 230, 0));
    selPen.setWidth(0); // cosmetic — 1px at any zoom
    selPen.setStyle(Qt::DashLine);
    selPen.setCosmetic(true);
    m_selectionItem = new QGraphicsRectItem;
    m_selectionItem->setPen(selPen);
    m_selectionItem->setBrush(Qt::NoBrush);
    m_selectionItem->setZValue(100);
    m_selectionItem->setVisible(false);
    m_scene->addItem(m_selectionItem);

    m_pastePreview = new QGraphicsPixmapItem;
    m_pastePreview->setOpacity(0.5);
    m_pastePreview->setZValue(99);
    m_pastePreview->setVisible(false);
    m_scene->addItem(m_pastePreview);
}

void MapView::setDocument(MapDocument *doc)
{
    // If a paint stroke is still open (document swapped while the mouse
    // button is held — e.g. a keyboard shortcut fired mid-drag), close its
    // macro on the OLD document before switching. An endMacro() landing on
    // the new document's stack would have no matching beginMacro().
    endStrokeIfActive();

    m_doc = doc;
    m_scene->setDocument(doc);
    resetTransform();
    centerOn(m_scene->sceneRect().center());
    emit zoomChanged(zoom());
    m_selectedTileset = -1;
    m_selectedImage = -1;
    m_selectedCellType = -1;
    if (m_pasteMode)
        exitPasteMode();
    clearSelection();
    updatePaintCursor();
}

void MapView::setSelectedTile(int tilesetIdx, int imageIdx)
{
    m_selectedTileset = tilesetIdx;
    m_selectedImage = imageIdx;
    if (hasSelectedTile()) {
        m_selectedCellType = -1;
        if (m_pasteMode)
            exitPasteMode();
    }
    updatePaintCursor();
}

void MapView::setSelectedCellType(int cellTypeValue)
{
    m_selectedCellType = cellTypeValue;
    if (hasSelectedCellType()) {
        m_selectedTileset = -1;
        m_selectedImage = -1;
        if (m_pasteMode)
            exitPasteMode();
    }
    updatePaintCursor();
}

QPoint MapView::tileAtViewportPos(QPoint vpPos) const
{
    if (!m_doc || !m_doc->isLoaded())
        return QPoint(-1, -1);
    const QPointF scenePos = mapToScene(vpPos);
    const int tx = static_cast<int>(std::floor(scenePos.x() / TileImageCache::TileSize));
    const int ty = static_cast<int>(std::floor(scenePos.y() / TileImageCache::TileSize));
    if (tx < 0 || ty < 0 || tx >= m_doc->widthTiles() || ty >= m_doc->heightTiles())
        return QPoint(-1, -1);
    return QPoint(tx, ty);
}

void MapView::clearSelection()
{
    if (m_selection.isEmpty() && !m_selectionItem->isVisible())
        return;
    m_selection = QRect();
    m_selectionItem->setVisible(false);
    m_selecting = false;
    emit selectionChanged(QRect());
}

void MapView::updateSelectionItem()
{
    if (m_selection.isEmpty()) {
        m_selectionItem->setVisible(false);
        return;
    }
    constexpr int T = TileImageCache::TileSize;
    m_selectionItem->setRect(QRectF(m_selection.x() * T,
                                    m_selection.y() * T,
                                    m_selection.width() * T,
                                    m_selection.height() * T));
    m_selectionItem->setVisible(true);
}

void MapView::enterPasteMode(const TileClipboard *clip)
{
    if (!clip || clip->isEmpty() || !m_doc || !m_doc->isLoaded())
        return;
    m_clipboard = clip;
    m_pasteMode = true;
    // Paste excludes other paint modes — but selection rect can stay visible.
    m_selectedTileset = -1;
    m_selectedImage = -1;
    m_selectedCellType = -1;
    rebuildPastePreview();
    setDragMode(QGraphicsView::NoDrag);
    setCursor(Qt::CrossCursor);
    emit pasteModeChanged(true);
    emit selectionCleared(); // wipe palettes' visual selection
}

void MapView::exitPasteMode()
{
    if (!m_pasteMode)
        return;
    m_pasteMode = false;
    m_clipboard = nullptr;
    m_pastePreview->setVisible(false);
    updatePaintCursor();
    emit pasteModeChanged(false);
}

void MapView::enterCopyMode()
{
    // Wipe other modes — copy is exclusive.
    if (m_pasteMode)
        exitPasteMode();
    m_selectedTileset = -1;
    m_selectedImage = -1;
    m_selectedCellType = -1;
    clearSelection();
    m_copyClickStage = 1;
    setDragMode(QGraphicsView::NoDrag);
    setCursor(Qt::CrossCursor);
    emit copyModeStageChanged(1);
    emit selectionCleared(); // palettes drop their visual selection
}

void MapView::exitCopyMode()
{
    if (m_copyClickStage == 0)
        return;
    m_copyClickStage = 0;
    updatePaintCursor();
    emit copyModeStageChanged(0);
}

void MapView::rebuildPastePreview()
{
    if (!m_clipboard || m_clipboard->isEmpty() || !m_doc || !m_doc->cache()) {
        m_pastePreview->setVisible(false);
        return;
    }

    constexpr int T = TileImageCache::TileSize;
    const int W = m_clipboard->width();
    const int H = m_clipboard->height();
    QImage img(W * T, H * T, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    QPainter p(&img);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const auto &c = m_clipboard->at(x, y);
            if (c.tilesetIdx < 0 || c.imageIdx < 0)
                continue;
            const QImage *strip = m_doc->cache()->tileset(
                static_cast<std::size_t>(c.tilesetIdx));
            if (!strip)
                continue;
            const int srcY = c.imageIdx * T;
            if (srcY + T > strip->height())
                continue;
            p.drawImage(QPoint(x * T, y * T), *strip, QRect(0, srcY, T, T));
        }
    }
    p.end();

    m_pastePreview->setPixmap(QPixmap::fromImage(img));
    m_pastePreview->setVisible(true);
}

void MapView::movePastePreviewToTile(QPoint tilePt)
{
    if (tilePt.x() < 0 || tilePt.y() < 0) {
        m_pastePreview->setVisible(false);
        return;
    }
    constexpr int T = TileImageCache::TileSize;
    m_pastePreview->setPos(tilePt.x() * T, tilePt.y() * T);
    m_pastePreview->setVisible(true);
}

void MapView::updatePaintCursor()
{
    if (hasSelectedTile() || hasSelectedCellType()) {
        setDragMode(QGraphicsView::NoDrag);
        setCursor(Qt::CrossCursor);
    } else {
        setDragMode(QGraphicsView::ScrollHandDrag);
        unsetCursor();
    }
}

bool MapView::paintAtViewportPos(QPoint vpPos)
{
    if (!m_doc || !m_doc->isLoaded())
        return false;
    const QPointF scenePos = mapToScene(vpPos);
    const int tx = static_cast<int>(std::floor(scenePos.x() / TileImageCache::TileSize));
    const int ty = static_cast<int>(std::floor(scenePos.y() / TileImageCache::TileSize));
    if (tx < 0 || ty < 0 || tx >= m_doc->widthTiles() || ty >= m_doc->heightTiles())
        return false;

    // The stroke macro opens lazily, right before the first command that will
    // actually change the map. Opening it eagerly on mouse-press would commit
    // an empty macro for no-op clicks (same tile, bottom row, off-map), which
    // both pollutes the undo history and flips the stack's clean flag —
    // showing a phantom "unsaved changes" marker.
    auto ensureStrokeMacro = [this](const QString &text) {
        if (m_inStroke && !m_strokeMacroOpen) {
            m_doc->undoStack()->beginMacro(text);
            m_strokeMacroOpen = true;
        }
    };

    if (hasSelectedTile()) {
        if (!m_doc->wouldPaintTileChange(tx, ty, m_selectedTileset, m_selectedImage))
            return false;
        ensureStrokeMacro(tr("Paint tiles"));
        m_doc->pushPaintTile(tx, ty, m_selectedTileset, m_selectedImage);
        return true;
    }
    if (hasSelectedCellType()) {
        const auto ct = static_cast<OP2Utility::CellType>(m_selectedCellType);
        if (!m_doc->wouldPaintCellTypeChange(tx, ty, ct))
            return false;
        ensureStrokeMacro(tr("Paint cell types"));
        m_doc->pushPaintCellType(tx, ty, ct);
        return true;
    }
    return false;
}

void MapView::mousePressEvent(QMouseEvent *event)
{
    const bool painting = hasSelectedTile() || hasSelectedCellType();
    const bool shift = (event->modifiers() & Qt::ShiftModifier);

    // Two-click copy mode — first click sets start, second click finalizes.
    if (m_copyClickStage > 0) {
        if (event->button() == Qt::RightButton) {
            clearSelection();
            exitCopyMode();
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            const QPoint tp = tileAtViewportPos(event->position().toPoint());
            if (tp.x() < 0) { event->accept(); return; }
            if (m_copyClickStage == 1) {
                m_selectionDragStart = tp;
                m_selection = QRect(tp, QSize(1, 1));
                updateSelectionItem();
                m_copyClickStage = 2;
                emit copyModeStageChanged(2);
            } else {
                const int x0 = std::min(m_selectionDragStart.x(), tp.x());
                const int y0 = std::min(m_selectionDragStart.y(), tp.y());
                const int x1 = std::max(m_selectionDragStart.x(), tp.x());
                const int y1 = std::max(m_selectionDragStart.y(), tp.y());
                m_selection = QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
                updateSelectionItem();
                exitCopyMode();
                emit selectionChanged(m_selection);
            }
            event->accept();
            return;
        }
    }

    // Paste mode takes precedence (entered explicitly via Ctrl+V).
    if (m_pasteMode) {
        if (event->button() == Qt::LeftButton && m_clipboard) {
            const QPoint tp = tileAtViewportPos(event->position().toPoint());
            if (tp.x() >= 0 && m_doc) {
                m_doc->pushPlaceTiles(tp.x(), tp.y(), *m_clipboard);
            }
            exitPasteMode();
            event->accept();
            return;
        }
        if (event->button() == Qt::RightButton) {
            exitPasteMode();
            event->accept();
            return;
        }
    }

    // Shift+left starts (or restarts) a rectangular selection drag.
    if (event->button() == Qt::LeftButton && shift && m_doc && m_doc->isLoaded()) {
        const QPoint tp = tileAtViewportPos(event->position().toPoint());
        if (tp.x() >= 0) {
            m_selecting = true;
            m_selectionDragStart = tp;
            m_selection = QRect(tp, QSize(1, 1));
            updateSelectionItem();
            emit selectionChanged(m_selection);
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && painting && m_doc) {
        // Marks the gesture only — the undo macro itself opens lazily inside
        // paintAtViewportPos once a tile actually changes.
        m_inStroke = true;
        paintAtViewportPos(event->position().toPoint());
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton && painting) {
        m_selectedTileset = -1;
        m_selectedImage = -1;
        m_selectedCellType = -1;
        updatePaintCursor();
        emit selectionCleared();
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton && hasSelection()) {
        clearSelection();
        event->accept();
        return;
    }
    // Right-click on an idle view → context menu (handled by MainWindow).
    if (event->button() == Qt::RightButton) {
        emit contextMenuRequested(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_selecting) {
            m_selecting = false;
            emit selectionChanged(m_selection);
        }
        endStrokeIfActive();
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void MapView::endStrokeIfActive()
{
    if (!m_inStroke)
        return;
    m_inStroke = false;
    // The macro only exists if at least one paint in this stroke changed the
    // map (see paintAtViewportPos) — no-op strokes have nothing to close.
    if (m_strokeMacroOpen && m_doc)
        m_doc->undoStack()->endMacro();
    m_strokeMacroOpen = false;
}

double MapView::zoom() const
{
    return transform().m11();
}

void MapView::resetZoom()
{
    resetTransform();
    emit zoomChanged(zoom());
    emit viewportChanged();
}

QRectF MapView::visibleSceneRect() const
{
    return mapToScene(viewport()->rect()).boundingRect();
}

void MapView::centerOnScene(QPointF scenePos)
{
    centerOn(scenePos);
    emit viewportChanged();
}

void MapView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    emit viewportChanged();
}

void MapView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    emit viewportChanged();
}

void MapView::setShowGrid(bool on)
{
    m_scene->setShowGrid(on);
}

void MapView::setShowLargeGrid(bool on)
{
    m_scene->setShowLargeGrid(on);
}

void MapView::setShowCoords(bool on)
{
    m_scene->setShowCoords(on);
}

void MapView::setRenderMode(int mode)
{
    m_scene->setRenderMode(static_cast<MapScene::RenderMode>(mode));
}

void MapView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);

    // While Shift-drag-selecting, extend the rectangle.
    if (m_selecting && (event->buttons() & Qt::LeftButton)) {
        const QPoint tp = tileAtViewportPos(event->position().toPoint());
        if (tp.x() >= 0) {
            const int x0 = std::min(m_selectionDragStart.x(), tp.x());
            const int y0 = std::min(m_selectionDragStart.y(), tp.y());
            const int x1 = std::max(m_selectionDragStart.x(), tp.x());
            const int y1 = std::max(m_selectionDragStart.y(), tp.y());
            m_selection = QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
            updateSelectionItem();
        }
    }

    // Two-click copy stage 2 — preview the rectangle from start to cursor.
    if (m_copyClickStage == 2) {
        const QPoint tp = tileAtViewportPos(event->position().toPoint());
        if (tp.x() >= 0) {
            const int x0 = std::min(m_selectionDragStart.x(), tp.x());
            const int y0 = std::min(m_selectionDragStart.y(), tp.y());
            const int x1 = std::max(m_selectionDragStart.x(), tp.x());
            const int y1 = std::max(m_selectionDragStart.y(), tp.y());
            m_selection = QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
            updateSelectionItem();
        }
    }

    // Drag-paint when a tile or cell-type is selected and left button is held.
    if (!m_selecting && (hasSelectedTile() || hasSelectedCellType())
        && (event->buttons() & Qt::LeftButton)) {
        paintAtViewportPos(event->position().toPoint());
    }

    // Move paste preview to follow the cursor's tile.
    if (m_pasteMode) {
        movePastePreviewToTile(tileAtViewportPos(event->position().toPoint()));
    }

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
        emit zoomChanged(zoom());
        emit viewportChanged();
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}
