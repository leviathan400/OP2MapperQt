// TilesetGrid: the tile grid inside the Tile Set palette.

#include "TilesetGrid.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>

TilesetGrid::TilesetGrid(QScrollArea *scroll, QWidget *parent)
    : QWidget(parent), m_scroll(scroll)
{
    setAutoFillBackground(false);
    updateSize();
}

int TilesetGrid::tileCount() const
{
    return (m_image && m_image->height() >= TileSize) ? m_image->height() / TileSize : 0;
}

// At least the viewport size, so the background fills it; rows of 33 px otherwise.
void TilesetGrid::updateSize()
{
    const int n = tileCount();
    const int rows = (n + Columns - 1) / Columns;
    const QSize view = m_scroll ? m_scroll->viewport()->size() : QSize(222, 393);
    resize(std::max(Columns * Stride, view.width()), std::max(rows * Stride, view.height()));
}

// The image belongs to the document's TileImageCache; the palette resets it when the map changes.
void TilesetGrid::setTilesetImage(const QImage *image)
{
    m_image = image;
    m_selected = -1;
    updateSize();
    update();
}

void TilesetGrid::setSelectedTile(int index)
{
    if (index == m_selected)
        return;
    m_selected = index;
    update();
}

// Only rows inside the exposed area are drawn. Grey border per tile, red 2 px box on the selection.
void TilesetGrid::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    if (!m_image || tileCount() == 0) {
        p.fillRect(rect(), QColor(211, 211, 211));
        QFont f(QStringLiteral("Arial"));
        f.setPixelSize(13);
        p.setFont(f);
        p.setPen(Qt::black);
        p.drawText(QPoint(10, 10 + p.fontMetrics().ascent()), tr("No tileset loaded"));
        return;
    }
    p.fillRect(event->rect(), QColor(169, 169, 169));
    const int n = tileCount();
    const QRect dirty = event->rect();
    const int r0 = std::max(0, dirty.top() / Stride);
    const int r1 = std::min((n - 1) / Columns, dirty.bottom() / Stride);
    for (int row = r0; row <= r1; ++row) {
        for (int col = 0; col < Columns; ++col) {
            const int i = row * Columns + col;
            if (i >= n)
                break;
            const QRect dst(col * Stride, row * Stride, TileSize, TileSize);
            p.drawImage(dst, *m_image, QRect(0, i * TileSize, TileSize, TileSize));
            p.setPen(QColor(128, 128, 128));
            p.setBrush(Qt::NoBrush);
            p.drawRect(dst);
            if (i == m_selected) {
                p.setPen(QPen(Qt::red, 2));
                p.drawRect(QRectF(dst).adjusted(0.5, 0.5, 0.5, 0.5));
            }
        }
    }
}

// Left click picks a tile; clicks in the empty strip right of the 6th column
// are ignored (they used to select the next row's first tile in VB). Right
// button starts a pan.
void TilesetGrid::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (event->button() == Qt::RightButton) {
        m_panning = true;
        m_panStart = event->globalPosition().toPoint();
        setCursor(Qt::OpenHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton || tileCount() == 0)
        return;
    const int col = pos.x() / Stride;
    const int row = pos.y() / Stride;
    if (col >= Columns || pos.x() < 0 || pos.y() < 0)
        return;
    const int idx = row * Columns + col;
    if (idx >= tileCount())
        return;
    emit tileClicked(idx);
}

// Right-drag scrolls the palette.
void TilesetGrid::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_panning || !(event->buttons() & Qt::RightButton) || !m_scroll)
        return;
    const QPoint g = event->globalPosition().toPoint();
    const QPoint d = m_panStart - g;
    m_panStart = g;
    m_scroll->horizontalScrollBar()->setValue(m_scroll->horizontalScrollBar()->value() + d.x());
    m_scroll->verticalScrollBar()->setValue(m_scroll->verticalScrollBar()->value() + d.y());
}

void TilesetGrid::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && m_panning) {
        m_panning = false;
        unsetCursor();
    }
}
