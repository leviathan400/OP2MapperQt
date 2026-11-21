// MapView: the map panel. Holds the zoom and the pixel offset of the map
// origin, paints the map plus every overlay in OP2Mapper3's order, and turns
// mouse input into Editor calls (left button) or panning (right button).

#include "MapView.h"

#include "Editor.h"
#include "MapDocument.h"
#include "MapRenderer.h"
#include "TileImageCache.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFont>
#include <QFontMetrics>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr int T = TileImageCache::TileSize;
constexpr int kPanMargin = 64;
constexpr double kZoomSteps[] = { 0.10, 0.25, 0.50, 0.75, 1.00, 1.25, 1.50 };
constexpr qint64 kMaxCachePixels = 4096LL * 4096LL;
} // namespace

MapView::MapView(Editor *editor, QWidget *parent)
    : QWidget(parent), m_editor(editor), m_doc(editor->document())
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(200, 150);

    connect(m_doc, &MapDocument::regionChanged, this, &MapView::onRegionChanged);
    connect(m_editor, &Editor::overlayChanged, this, qOverload<>(&QWidget::update));
    connect(m_editor, &Editor::overlaysChanged, this, qOverload<>(&QWidget::update));
    connect(m_editor, &Editor::renderModeChanged, this, [this] {
        m_cacheMode = -1;
        rebuildCache();
        update();
    });
    connect(m_editor, &Editor::cursorChanged, this, &MapView::updateCursor);
}

// Tiles are drawn at a whole number of pixels, so zoomed views never show seams.
int MapView::tilePx() const
{
    return std::max(1, static_cast<int>(std::lround(T * m_zoom)));
}

// New or closed map: back to 100% at the top-left.
void MapView::documentReplaced()
{
    m_zoom = 1.0;
    m_offsetX = 0;
    m_offsetY = 0;
    m_cache = QImage();
    m_cacheMode = -1;
    m_panning = false;
    rebuildCache();
    clampOffsets();
    emit zoomChanged(m_zoom);
    emit viewportChanged();
    update();
}

// View > Enable Zoom (Ctrl+E). Turning it off snaps back to 100%.
void MapView::setZoomEnabled(bool on)
{
    m_zoomEnabled = on;
    if (!on && std::abs(m_zoom - 1.0) > 1e-9) {
        m_zoom = 1.0;
        rebuildCache();
        clampOffsets();
        emit viewportChanged();
        update();
    }
    emit zoomChanged(m_zoom);
}

// Ctrl+R / Reset Zoom: 100%, a small map centred, a large one at the top-left.
void MapView::resetView()
{
    if (!m_doc->isLoaded())
        return;
    m_zoom = 1.0;
    const int mapW = m_doc->widthTiles() * T;
    const int mapH = m_doc->heightTiles() * T;
    m_offsetX = mapW < width() ? (width() - mapW) / 2 : 0;
    m_offsetY = mapH < height() ? (height() - mapH) / 2 : 0;
    rebuildCache();
    clampOffsets();
    emit zoomChanged(m_zoom);
    emit viewportChanged();
    update();
}

// OP2Mapper3 ClampOffsets: a map smaller than the panel stays fully visible;
// a larger one may be panned at most 64 px past each edge.
void MapView::clampOffsets()
{
    if (!m_doc->isLoaded())
        return;
    const int s = tilePx();
    const int scaledW = m_doc->widthTiles() * s;
    const int scaledH = m_doc->heightTiles() * s;
    if (scaledW <= width())
        m_offsetX = std::clamp(m_offsetX, 0, width() - scaledW);
    else
        m_offsetX = std::clamp(m_offsetX, width() - scaledW - kPanMargin, kPanMargin);
    if (scaledH <= height())
        m_offsetY = std::clamp(m_offsetY, 0, height() - scaledH);
    else
        m_offsetY = std::clamp(m_offsetY, height() - scaledH - kPanMargin, kPanMargin);
}

// Mini map click: centre the clicked tile.
void MapView::panToCenterTile(int tx, int ty)
{
    if (!m_doc->isLoaded())
        return;
    const int s = tilePx();
    m_offsetX = static_cast<int>(std::lround(width() / 2.0 - (tx + 0.5) * s));
    m_offsetY = static_cast<int>(std::lround(height() / 2.0 - (ty + 0.5) * s));
    clampOffsets();
    emit viewportChanged();
    update();
}

// Visible area in fractional tiles (the mini map's yellow box).
QRectF MapView::visibleTileRect() const
{
    const double s = tilePx();
    return QRectF(-m_offsetX / s, -m_offsetY / s, width() / s, height() / s);
}

QPoint MapView::tileAt(const QPoint &pos) const
{
    const int s = tilePx();
    return QPoint(static_cast<int>(std::floor((pos.x() - m_offsetX) / static_cast<double>(s))),
                  static_cast<int>(std::floor((pos.y() - m_offsetY) / static_cast<double>(s))));
}

QRect MapView::tileScreenRect(const QRect &tiles) const
{
    const int s = tilePx();
    return QRect(m_offsetX + tiles.x() * s, m_offsetY + tiles.y() * s, tiles.width() * s, tiles.height() * s);
}

// Changes the zoom keeping the map point under `anchor` (the cursor) fixed.
void MapView::setZoomAround(double newZoom, const QPointF &anchor)
{
    if (!m_doc->isLoaded())
        return;
    const double oldPx = tilePx();
    const double mapX = (anchor.x() - m_offsetX) / oldPx;
    const double mapY = (anchor.y() - m_offsetY) / oldPx;
    m_zoom = newZoom;
    const double newPx = tilePx();
    m_offsetX = static_cast<int>(std::lround(anchor.x() - mapX * newPx));
    m_offsetY = static_cast<int>(std::lround(anchor.y() - mapY * newPx));
    rebuildCache();
    clampOffsets();
    emit zoomChanged(m_zoom);
    emit viewportChanged();
    update();
}

// ---- Cache -------------------------------------------------------------------

// When zoomed out, the whole map's mode layer is kept as one image so panning
// is just a blit; edits update it tile by tile. At 50% and above (or when the
// image would be huge) the visible tiles are drawn directly instead.
void MapView::rebuildCache()
{
    if (!m_doc->isLoaded()) {
        m_cache = QImage();
        return;
    }
    const int s = tilePx();
    const int mode = m_editor->renderMode();
    const qint64 px = static_cast<qint64>(m_doc->widthTiles()) * s
                    * static_cast<qint64>(m_doc->heightTiles()) * s;
    // Cache the whole map only when zoomed out (< 50%), where drawing every
    // visible tile on each paint would be slow; otherwise draw directly.
    if (px > kMaxCachePixels || s >= 16) {
        m_cache = QImage();
        m_cacheTilePx = s;
        m_cacheMode = mode;
        return;
    }
    if (!m_cache.isNull() && m_cacheTilePx == s && m_cacheMode == mode)
        return;
    m_cache = QImage(m_doc->widthTiles() * s, m_doc->heightTiles() * s, QImage::Format_RGB32);
    if (m_cache.isNull())
        return;
    m_cacheTilePx = s;
    m_cacheMode = mode;
    m_cache.fill(Qt::black);
    QPainter p(&m_cache);
    MapRenderer::drawTiles(p, *m_doc, QRect(0, 0, m_doc->widthTiles(), m_doc->heightTiles()),
                           s, QPoint(0, 0), mode);
}

// Redraws edited tiles into the cached image.
void MapView::updateCacheRegion(const QRect &tiles)
{
    if (m_cache.isNull())
        return;
    const int s = m_cacheTilePx;
    QPainter p(&m_cache);
    const QRect r(tiles.x() * s, tiles.y() * s, tiles.width() * s, tiles.height() * s);
    p.fillRect(r, Qt::black);
    p.setClipRect(r);
    MapRenderer::drawTiles(p, *m_doc, tiles, s, QPoint(0, 0), m_cacheMode);
}

void MapView::onRegionChanged(const QRect &tiles)
{
    updateCacheRegion(tiles);
    update(tileScreenRect(tiles).adjusted(-2, -2, 2, 2));
}

// ---- Paint -------------------------------------------------------------------

// Black background, the mode layer (cached or direct), grid / large grid /
// coordinates, then the editing overlays.
void MapView::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.fillRect(event->rect(), Qt::black);
    if (!m_doc->isLoaded())
        return;

    const int s = tilePx();
    const QRect dirty = event->rect();
    // Visible tiles with a 1-tile margin, clipped to the map.
    QRect tiles(QPoint(static_cast<int>(std::floor((dirty.left() - m_offsetX) / double(s))) - 1,
                       static_cast<int>(std::floor((dirty.top() - m_offsetY) / double(s))) - 1),
                QPoint(static_cast<int>(std::ceil((dirty.right() - m_offsetX) / double(s))) + 1,
                       static_cast<int>(std::ceil((dirty.bottom() - m_offsetY) / double(s))) + 1));
    tiles = tiles.intersected(QRect(0, 0, m_doc->widthTiles(), m_doc->heightTiles()));
    if (tiles.isEmpty())
        return;

    const QPoint origin(m_offsetX, m_offsetY);
    if (!m_cache.isNull() && m_cacheTilePx == s && m_cacheMode == m_editor->renderMode()) {
        const QRect src(tiles.x() * s, tiles.y() * s, tiles.width() * s, tiles.height() * s);
        p.drawImage(src.topLeft() + origin, m_cache, src);
    } else {
        MapRenderer::drawTiles(p, *m_doc, tiles, s, origin, m_editor->renderMode());
    }
    MapRenderer::drawOverlays(p, *m_doc, tiles, s, origin, rect(), m_editor->overlays());
    paintOverlays(p, tiles);
}

// Overlays in OP2Mapper3's order and colours: enclosed areas (magenta), paste
// preview, tile-group preview, copy rectangle (lime while selecting, sky blue
// once copied, with its size label), brush outline, cliff preview, and the
// white box on the selected tile.
void MapView::paintOverlays(QPainter &p, const QRect &visibleTiles)
{
    const int s = tilePx();

    // Enclosed-area highlight
    if (m_editor->highlightEnclosed()) {
        const QColor magenta(255, 0, 255, 120);
        for (const QPoint &t : m_editor->enclosedTiles()) {
            if (visibleTiles.contains(t))
                p.fillRect(m_offsetX + t.x() * s, m_offsetY + t.y() * s, s, s, magenta);
        }
    }

    // Paste and tile-group previews (only while edit mode is on)
    if (m_editor->editMode() && m_editor->pasteMode() && m_editor->pastePreviewPos().x() >= 0) {
        const auto &c = m_editor->clipboard();
        const QPoint pp = m_editor->pastePreviewPos();
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawImage(QRect(m_offsetX + pp.x() * s, m_offsetY + pp.y() * s, c.width * s, c.height * s),
                    m_editor->pastePreviewImage());
    }
    if (m_editor->editMode() && m_editor->tileGroupMode() && m_editor->tileGroupPreviewPos().x() >= 0) {
        const auto &g = m_editor->tileGroup();
        const QPoint gp = m_editor->tileGroupPreviewPos();
        p.drawImage(QRect(m_offsetX + gp.x() * s, m_offsetY + gp.y() * s, g.width * s, g.height * s),
                    m_editor->tileGroupPreviewImage());
    }

    // Copy selection
    if (m_editor->copyMode() && m_editor->copyStage() >= 1) {
        const QRect tr = m_editor->copyRect();
        if (!tr.isEmpty()) {
            const bool copied = m_editor->copyStage() == 2;
            const QColor colour = copied ? QColor(0, 191, 255) : QColor(0, 255, 0);
            const QRect r = tileScreenRect(tr);
            QColor fill = colour;
            fill.setAlpha(50);
            p.fillRect(r, fill);
            p.setPen(QPen(Qt::black, 3));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);
            QPen dash(colour, 2);
            dash.setStyle(Qt::DashLine);
            p.setPen(dash);
            p.drawRect(r);
            QFont f(QStringLiteral("Segoe UI"));
            f.setPixelSize(12);
            f.setBold(true);
            p.setFont(f);
            const QString text = QStringLiteral("%1x%2").arg(tr.width()).arg(tr.height())
                               + (copied ? QStringLiteral(" copied") : QString());
            const QFontMetrics fm(f);
            const QRect box(r.left() + 2, r.top() + 2, fm.horizontalAdvance(text) + 4, fm.height());
            p.fillRect(box, QColor(0, 0, 0, 170));
            p.setPen(colour);
            p.drawText(box, Qt::AlignCenter, text);
        }
    }

    // Terrain brush outline
    if (m_editor->terrainToolActive() && m_editor->brushHover().x() >= 0) {
        const QPoint h = m_editor->brushHover();
        const int half = m_editor->brushSize() / 2;
        const QRect r = tileScreenRect(QRect(h.x() - half, h.y() - half, 2 * half + 1, 2 * half + 1));
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::black, 3));
        p.drawEllipse(r);
        p.setPen(QPen(m_editor->terrainGroundColor(), 1.5));
        p.drawEllipse(r);
        p.setRenderHint(QPainter::Antialiasing, false);
    }

    // Cliff preview
    if (m_editor->cliffToolActive() && !m_editor->cliffPoints().isEmpty()) {
        const QColor colour(255, 210, 90);
        const auto &pts = m_editor->cliffPoints();
        auto centre = [&](const QPoint &t) {
            return QPointF(m_offsetX + (t.x() + 0.5) * s, m_offsetY + (t.y() + 0.5) * s);
        };
        p.setRenderHint(QPainter::Antialiasing, true);
        for (int i = 1; i < pts.size(); ++i) {
            p.setPen(QPen(Qt::black, 5));
            p.drawLine(centre(pts[i - 1]), centre(pts[i]));
            p.setPen(QPen(colour, 3));
            p.drawLine(centre(pts[i - 1]), centre(pts[i]));
        }
        if (m_editor->cliffCursor().x() >= 0) {
            QPen dash(colour, 2);
            dash.setStyle(Qt::DashLine);
            p.setPen(dash);
            p.drawLine(centre(pts.last()), centre(m_editor->cliffCursor()));
        }
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(colour, 3));
        const double side = 2.0 * s;
        for (int i = 0; i < pts.size(); ++i) {
            const QPointF c = centre(pts[i]);
            const QRectF box(c.x() - side / 2, c.y() - side / 2, side, side);
            p.drawRect(box);
            if (i == 0 && pts.size() > 1)
                p.drawRect(box.adjusted(4, 4, -4, -4));
        }
    }

    // Selected tile box
    const QPoint cur = m_editor->currentTile();
    if (cur.x() >= 0 && m_doc->inBounds(cur.x(), cur.y())) {
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(m_offsetX + cur.x() * s + 1, m_offsetY + cur.y() * s + 1, s - 2, s - 2));
    }
}

// ---- Input -------------------------------------------------------------------

void MapView::updateCursor()
{
    if (m_panning)
        setCursor(Qt::OpenHandCursor);
    else if (m_editor->pasteMode() || m_editor->tileGroupMode() || m_editor->terrainToolActive()
             || m_editor->cliffToolActive())
        setCursor(Qt::CrossCursor);
    else
        unsetCursor();
}

// Left button goes to the Editor; right button starts a pan.
void MapView::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    if (!m_doc->isLoaded())
        return;
    const QPoint pos = event->position().toPoint();
    if (event->button() == Qt::LeftButton) {
        const QPoint t = tileAt(pos);
        if (m_doc->inBounds(t.x(), t.y()))
            m_editor->mapLeftPress(t);
    } else if (event->button() == Qt::RightButton) {
        m_panning = true;
        m_rightDownPos = pos;
        m_lastMouse = pos;
        setCursor(Qt::OpenHandCursor);
    }
}

// Hover coordinates, editor tracking (also off the map, for the copy corner), and panning.
void MapView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_doc->isLoaded())
        return;
    const QPoint pos = event->position().toPoint();
    const QPoint t = tileAt(pos);
    const bool inside = m_doc->inBounds(t.x(), t.y());
    if (inside)
        emit hoverTile(t.x(), t.y());
    if (!m_panning)
        m_editor->mapMouseMove(t, inside, event->buttons() & Qt::LeftButton);

    if (m_panning && (event->buttons() & Qt::RightButton)) {
        m_offsetX += pos.x() - m_lastMouse.x();
        m_offsetY += pos.y() - m_lastMouse.y();
        clampOffsets();
        emit viewportChanged();
        update();
    }
    m_lastMouse = pos;
}

// A right click that moved less than 5 px opens the context menu; a longer drag was a pan.
void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (event->button() == Qt::LeftButton) {
        m_editor->mapLeftRelease();
    } else if (event->button() == Qt::RightButton && m_panning) {
        m_panning = false;
        updateCursor();
        const QPoint d = pos - m_rightDownPos;
        const double dist = std::sqrt(double(d.x()) * d.x() + double(d.y()) * d.y());
        if (dist < 5 && m_doc->isLoaded() && !m_editor->editDragging()) {
            m_contextPos = pos;
            emit contextMenuRequested(event->globalPosition().toPoint());
        }
    }
}

// Ctrl+wheel with the terrain brush resizes the brush. Otherwise the wheel
// steps the zoom through 10/25/50/75/100/125/150% around the cursor (no
// scrolling), unless zoom is locked.
void MapView::wheelEvent(QWheelEvent *event)
{
    event->accept();
    if (!m_doc->isLoaded())
        return;
    const int delta = event->angleDelta().y();
    if (delta == 0)
        return;
    if ((event->modifiers() & Qt::ControlModifier) && m_editor->terrainToolActive()) {
        m_editor->stepBrushSize(delta > 0 ? 2 : -2);
        return;
    }
    if (!m_zoomEnabled)
        return;
    int idx = -1;
    int nearest = 0;
    double best = 1e9;
    for (int i = 0; i < int(std::size(kZoomSteps)); ++i) {
        const double d = std::abs(kZoomSteps[i] - m_zoom);
        if (d < 0.01)
            idx = i;
        if (d < best) {
            best = d;
            nearest = i;
        }
    }
    if (idx < 0)
        idx = nearest;
    idx = delta > 0 ? std::min(idx + 1, int(std::size(kZoomSteps)) - 1) : std::max(idx - 1, 0);
    if (std::abs(kZoomSteps[idx] - m_zoom) < 1e-9)
        return;
    setZoomAround(kZoomSteps[idx], event->position());
}

// Keeps a small map centred and stops a large one showing empty space.
void MapView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_doc->isLoaded()) {
        const int s = tilePx();
        const int mapW = m_doc->widthTiles() * s;
        const int mapH = m_doc->heightTiles() * s;
        if (mapW < width())
            m_offsetX = (width() - mapW) / 2;
        else
            m_offsetX = std::clamp(m_offsetX, width() - mapW, 0);
        if (mapH < height())
            m_offsetY = (height() - mapH) / 2;
        else
            m_offsetY = std::clamp(m_offsetY, height() - mapH, 0);
        clampOffsets();
    }
    emit viewportChanged();
}

void MapView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

// Dropped files are handed to MainWindow, which opens the first one.
void MapView::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &u : event->mimeData()->urls()) {
        if (u.isLocalFile())
            paths << u.toLocalFile();
    }
    if (!paths.isEmpty())
        emit filesDropped(paths);
}

QImage MapView::grabView()
{
    return grab().toImage();
}
