#include "MiniMap.h"

#include "MapDocument.h"
#include "TileImageCache.h"

#include <OP2Utility.h>

#include <QHash>
#include <QMouseEvent>
#include <QPainter>

namespace { constexpr int T = TileImageCache::TileSize; }

MiniMap::MiniMap(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(tr("Minimap"));
    setAttribute(Qt::WA_DeleteOnClose, false);
}

void MiniMap::setDocument(const MapDocument *doc)
{
    m_doc = doc;
    rebuildThumbnail();

    if (!m_doc || !m_doc->isLoaded()) {
        m_viewportScene = QRectF();
        hide();
        return;
    }

    // Pick a display scale so the window fits a reasonable max size.
    constexpr int maxDim = 512;
    const int w = m_doc->widthTiles();
    const int h = m_doc->heightTiles();
    if (w <= 0 || h <= 0) {
        hide();
        return;
    }
    m_pxPerTile = std::max(1, std::min(maxDim / w, maxDim / h));
    if (m_pxPerTile < 1) m_pxPerTile = 1;

    setFixedSize(w * m_pxPerTile, h * m_pxPerTile);
    update();
}

void MiniMap::setViewportRect(const QRectF &sceneRect)
{
    m_viewportScene = sceneRect;
    update();
}

void MiniMap::rebuildThumbnail()
{
    if (!m_doc || !m_doc->isLoaded() || !m_doc->cache()) {
        m_thumb = QImage();
        return;
    }

    const auto *map = m_doc->map();
    const auto *cache = m_doc->cache();
    const int w = m_doc->widthTiles();
    const int h = m_doc->heightTiles();

    m_thumb = QImage(w, h, QImage::Format_RGB32);
    m_thumb.fill(Qt::black);

    // Cache an averaged color per (tilesetIdx, imageIdx) pair — many tiles
    // reuse the same graphic, so we only compute the mean once per unique tile.
    QHash<quint32, QRgb> meanCache;

    auto meanColor = [&](std::size_t tsIdx, std::size_t imgIdx) -> QRgb {
        const quint32 key = (static_cast<quint32>(tsIdx) << 16)
                          | (static_cast<quint32>(imgIdx) & 0xFFFF);
        auto it = meanCache.constFind(key);
        if (it != meanCache.constEnd())
            return *it;

        const QImage *strip = cache->tileset(tsIdx);
        if (!strip) {
            meanCache.insert(key, qRgb(64, 0, 64));
            return qRgb(64, 0, 64);
        }
        const int srcY = static_cast<int>(imgIdx) * T;
        if (srcY + T > strip->height()) {
            meanCache.insert(key, qRgb(64, 32, 0));
            return qRgb(64, 32, 0);
        }

        quint64 sumR = 0, sumG = 0, sumB = 0;
        for (int py = 0; py < T; ++py) {
            const QRgb *line = reinterpret_cast<const QRgb *>(
                strip->constScanLine(srcY + py));
            for (int px = 0; px < T; ++px) {
                const QRgb c = line[px];
                sumR += qRed(c);
                sumG += qGreen(c);
                sumB += qBlue(c);
            }
        }
        constexpr int N = T * T;
        const QRgb mean = qRgb(static_cast<int>(sumR / N),
                               static_cast<int>(sumG / N),
                               static_cast<int>(sumB / N));
        meanCache.insert(key, mean);
        return mean;
    };

    for (int y = 0; y < h; ++y) {
        QRgb *row = reinterpret_cast<QRgb *>(m_thumb.scanLine(y));
        for (int x = 0; x < w; ++x) {
            row[x] = meanColor(map->GetTilesetIndex(x, y),
                               map->GetImageIndex(x, y));
        }
    }
}

void MiniMap::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_thumb.isNull())
        return;

    // Bilinear upscale — softens the per-tile mosaic into smoother terrain.
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(rect(), m_thumb);

    if (!m_viewportScene.isEmpty() && m_doc && m_doc->isLoaded()) {
        const double sx = static_cast<double>(width()) / (m_doc->widthTiles() * T);
        const double sy = static_cast<double>(height()) / (m_doc->heightTiles() * T);
        QRectF box(m_viewportScene.x() * sx,
                   m_viewportScene.y() * sy,
                   m_viewportScene.width() * sx,
                   m_viewportScene.height() * sy);
        QPen pen(QColor(255, 220, 0));
        pen.setWidth(2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(box.adjusted(1, 1, -1, -1));
    }
}

QPointF MiniMap::widgetToScene(QPoint widgetPt) const
{
    if (!m_doc || !m_doc->isLoaded())
        return {};
    const double sx = static_cast<double>(m_doc->widthTiles() * T) / width();
    const double sy = static_cast<double>(m_doc->heightTiles() * T) / height();
    return { widgetPt.x() * sx, widgetPt.y() * sy };
}

void MiniMap::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit centerRequested(widgetToScene(event->pos()));
}

void MiniMap::mouseMoveEvent(QMouseEvent *event)
{
    // Drag to scrub.
    if (event->buttons() & Qt::LeftButton)
        emit centerRequested(widgetToScene(event->pos()));
}
