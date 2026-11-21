#include "MiniMap.h"

#include "Editor.h"
#include "MapDocument.h"
#include "MapRenderer.h"
#include "TileImageCache.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

MiniMap::MiniMap(Editor *editor, QWidget *mainWindow)
    : ToolPalette(QStringLiteral("MiniMap"), QStringLiteral("fMiniMap"), mainWindow)
    , m_editor(editor)
{
    setWindowTitle(tr("Mini Map"));
    setFixedSize(433, 355);
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(300);
    connect(&m_refreshTimer, &QTimer::timeout, this, &MiniMap::refreshThumbnail);
}

void MiniMap::scheduleRefresh()
{
    if (isVisible())
        m_refreshTimer.start();
}

void MiniMap::clearThumbnail()
{
    m_thumb = QImage();
    m_thumbBounds = QRect();
    update();
}

void MiniMap::refreshThumbnail()
{
    const MapDocument *doc = m_editor->document();
    if (!doc || !doc->isLoaded()) {
        clearThumbnail();
        return;
    }
    const int W = doc->widthTiles(), H = doc->heightTiles();
    const int cw = width(), ch = height();
    const double displayScale = std::min(cw / double(W * 32), ch / double(H * 32));
    const int ppt = std::max(1, static_cast<int>(std::ceil(displayScale * 32)));
    m_thumb = MapRenderer::renderFullMap(*doc, ppt, MapRenderer::GameMap, MapRenderer::Overlays());
    const int dw = static_cast<int>(std::lround(W * 32 * displayScale));
    const int dh = static_cast<int>(std::lround(H * 32 * displayScale));
    m_thumbBounds = QRect((cw - dw) / 2, (ch - dh) / 2, dw, dh);
    update();
}

void MiniMap::setViewportTiles(const QRectF &visibleTiles)
{
    if (visibleTiles == m_visible)
        return;
    m_visible = visibleTiles;
    update();
}

void MiniMap::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_thumb.isNull() || m_thumbBounds.isEmpty())
        return;
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(m_thumbBounds, m_thumb);

    const MapDocument *doc = m_editor->document();
    if (!doc || !doc->isLoaded())
        return;
    const int W = doc->widthTiles(), H = doc->heightTiles();
    const QRectF vis = m_visible.intersected(QRectF(0, 0, W, H));
    if (vis.isEmpty())
        return;
    const double sx = m_thumbBounds.width() / double(W);
    const double sy = m_thumbBounds.height() / double(H);
    const double L = m_thumbBounds.x() + vis.left() * sx;
    const double Tt = m_thumbBounds.y() + vis.top() * sy;
    const double R = m_thumbBounds.x() + vis.right() * sx;
    const double B = m_thumbBounds.y() + vis.bottom() * sy;
    const int rx = static_cast<int>(std::floor(L));
    const int ry = static_cast<int>(std::floor(Tt));
    const int rw = std::max(1, static_cast<int>(std::ceil(R)) - rx);
    const int rh = std::max(1, static_cast<int>(std::ceil(B)) - ry);
    p.setPen(QPen(QColor(255, 255, 0), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rx, ry, rw, rh);
}

void MiniMap::jumpTo(const QPoint &pos)
{
    const MapDocument *doc = m_editor->document();
    if (m_thumb.isNull() || !doc || !doc->isLoaded() || m_thumbBounds.isEmpty())
        return;
    const int W = doc->widthTiles(), H = doc->heightTiles();
    const double relX = (pos.x() - m_thumbBounds.x()) / double(m_thumbBounds.width());
    const double relY = (pos.y() - m_thumbBounds.y()) / double(m_thumbBounds.height());
    const int tx = std::clamp(static_cast<int>(std::floor(relX * W)), 0, W - 1);
    const int ty = std::clamp(static_cast<int>(std::floor(relY * H)), 0, H - 1);
    emit jumpToTile(tx, ty);
}

void MiniMap::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        jumpTo(event->position().toPoint());
    }
}

void MiniMap::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging)
        jumpTo(event->position().toPoint());
}

void MiniMap::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
}
