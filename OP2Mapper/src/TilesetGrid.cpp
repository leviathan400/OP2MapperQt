#include "TilesetGrid.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>

TilesetGrid::TilesetGrid(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(80, 80, 80));
    setPalette(pal);
}

void TilesetGrid::setTilesetImage(const QImage *image)
{
    m_image = image;
    m_selectedTile = -1;
    updateGeometryForImage();
    update();
}

void TilesetGrid::setSelectedTile(int imageIdx)
{
    if (imageIdx == m_selectedTile)
        return;
    m_selectedTile = imageIdx;
    update();
}

int TilesetGrid::tileCount() const
{
    if (!m_image || m_image->height() < TileSize)
        return 0;
    return m_image->height() / TileSize;
}

void TilesetGrid::updateGeometryForImage()
{
    const int cellStride = TileSize + TilePadding;
    const int rows = (tileCount() + TilesPerRow - 1) / TilesPerRow;
    setMinimumSize(TilesPerRow * cellStride, rows * cellStride);
    resize(minimumSize());
    updateGeometry();
}

QSize TilesetGrid::sizeHint() const
{
    return minimumSizeHint();
}

QSize TilesetGrid::minimumSizeHint() const
{
    const int cellStride = TileSize + TilePadding;
    const int rows = std::max(1, (tileCount() + TilesPerRow - 1) / TilesPerRow);
    return QSize(TilesPerRow * cellStride, rows * cellStride);
}

void TilesetGrid::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.fillRect(event->rect(), QColor(80, 80, 80));

    if (!m_image || tileCount() == 0) {
        p.setPen(Qt::black);
        p.drawText(rect().adjusted(8, 8, -8, -8),
                   Qt::AlignLeft | Qt::AlignTop,
                   tr("No tileset loaded"));
        return;
    }

    const int cellStride = TileSize + TilePadding;
    const QRect clip = event->rect();

    const int startCol = std::max(0, clip.left() / cellStride);
    const int endCol = std::min(TilesPerRow - 1, clip.right() / cellStride);
    const int startRow = std::max(0, clip.top() / cellStride);
    const int total = tileCount();
    const int totalRows = (total + TilesPerRow - 1) / TilesPerRow;
    const int endRow = std::min(totalRows - 1, clip.bottom() / cellStride);

    const QPen borderPen(QColor(140, 140, 140));
    const QPen selectionPen(Qt::red, 2);

    for (int row = startRow; row <= endRow; ++row) {
        for (int col = startCol; col <= endCol; ++col) {
            const int imageIdx = row * TilesPerRow + col;
            if (imageIdx >= total)
                continue;

            const QRect dest(col * cellStride, row * cellStride, TileSize, TileSize);
            const QRect src(0, imageIdx * TileSize, TileSize, TileSize);
            p.drawImage(dest, *m_image, src);

            p.setPen(borderPen);
            p.drawRect(dest);

            if (imageIdx == m_selectedTile) {
                p.setPen(selectionPen);
                p.drawRect(dest.adjusted(0, 0, -1, -1));
            }
        }
    }
}

void TilesetGrid::mousePressEvent(QMouseEvent *event)
{
    if (!m_image || tileCount() == 0) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::RightButton) {
        m_selectedTile = -1;
        update();
        emit clearSelectionRequested();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const int cellStride = TileSize + TilePadding;
    const int col = event->position().x() / cellStride;
    const int row = event->position().y() / cellStride;
    if (col < 0 || col >= TilesPerRow || row < 0)
        return;

    const int imageIdx = row * TilesPerRow + col;
    if (imageIdx < 0 || imageIdx >= tileCount())
        return;

    m_selectedTile = imageIdx;
    update();
    emit tileClicked(imageIdx);
}
