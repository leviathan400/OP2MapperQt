#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QWidget>

class Editor;
class MapDocument;

// The map panel (OP2Mapper3 fMain.MapPanel): a plain widget with a zoom
// level and a pixel offset of the map origin. Right-drag pans (a right
// click that moved < 5 px opens the context menu), the wheel steps the
// zoom through 10..150% around the cursor, Ctrl+wheel resizes the terrain
// brush. Tiles are drawn at an integer size, round(32 * zoom) px.
class MapView : public QWidget
{
    Q_OBJECT
public:
    MapView(Editor *editor, QWidget *parent = nullptr);

    void documentReplaced();     // new map: reset zoom/offset, rebuild cache
    double zoom() const { return m_zoom; }
    int tilePx() const;
    bool zoomEnabled() const { return m_zoomEnabled; }
    void setZoomEnabled(bool on);
    void resetView();
    void panToCenterTile(int tx, int ty);

    // Zoom and offset, to keep the view when a generated map is replaced by
    // another of the same size.
    struct ViewState { double zoom; int offsetX; int offsetY; };
    ViewState viewState() const { return { m_zoom, m_offsetX, m_offsetY }; }
    void setViewState(const ViewState &v);
    // The largest of 100/75/50/25/10% that shows the whole map, centred
    // (a newly generated map is seen at once).
    void fitMapToView();

    // Visible area in fractional tiles (for the mini map).
    QRectF visibleTileRect() const;
    QPoint tileAt(const QPoint &widgetPos) const;
    QPoint lastContextPos() const { return m_contextPos; }

    // Grab what the panel shows (F12 screenshot).
    QImage grabView();

signals:
    void hoverTile(int x, int y);     // 0-based, only while inside the map
    void zoomChanged(double zoom);
    void viewportChanged();
    void contextMenuRequested(const QPoint &globalPos);
    void filesDropped(const QStringList &paths);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void clampOffsets();
    void setZoomAround(double newZoom, const QPointF &anchor);
    void rebuildCache();
    void updateCacheRegion(const QRect &tiles);
    void onRegionChanged(const QRect &tiles);
    void updateCursor();
    QRect tileScreenRect(const QRect &tiles) const;
    void paintOverlays(class QPainter &p, const QRect &visibleTiles);

    Editor *m_editor;
    MapDocument *m_doc;
    double m_zoom = 1.0;
    int m_offsetX = 0;
    int m_offsetY = 0;
    bool m_zoomEnabled = true;

    bool m_panning = false;
    QPoint m_rightDownPos;
    QPoint m_lastMouse;
    QPoint m_contextPos;

    // Mode layer of the whole map at the current tile size, when it fits.
    QImage m_cache;
    int m_cacheTilePx = 0;
    int m_cacheMode = -1;
};
