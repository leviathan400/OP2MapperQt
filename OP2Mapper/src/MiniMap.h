#pragma once

#include "ToolPalette.h"

#include <QImage>
#include <QRectF>
#include <QTimer>

class Editor;

// "Mini Map" palette (OP2Mapper3 fMiniMap, tool 5): a GameMap render of
// the whole map letterboxed in a fixed 433x355 client area, with the main
// view's visible area as a yellow box. Click or drag to jump.
class MiniMap : public ToolPalette
{
    Q_OBJECT
public:
    MiniMap(Editor *editor, QWidget *mainWindow);

    void refreshThumbnail();
    void scheduleRefresh(); // 300 ms debounce
    void clearThumbnail();
    void setViewportTiles(const QRectF &visibleTiles);

signals:
    void jumpToTile(int tx, int ty);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void jumpTo(const QPoint &pos);

    Editor *m_editor;
    QImage m_thumb;
    QRect m_thumbBounds;
    QRectF m_visible;
    bool m_dragging = false;
    QTimer m_refreshTimer;
};
