#pragma once

#include <QImage>
#include <QWidget>

class MapDocument;

// Standalone top-level window showing a thumbnail of the whole map with the
// main view's current viewport drawn as a yellow rectangle. Clicking inside
// the thumbnail emits centerRequested with scene coordinates.
class MiniMap : public QWidget
{
    Q_OBJECT
public:
    explicit MiniMap(QWidget *parent = nullptr);

    void setDocument(const MapDocument *doc);

    // Update the highlighted viewport rectangle (scene coords).
    void setViewportRect(const QRectF &sceneRect);

signals:
    // User clicked inside the thumbnail; payload is the corresponding scene
    // point (centered on the click).
    void centerRequested(QPointF scenePos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void rebuildThumbnail();
    QPointF widgetToScene(QPoint widgetPt) const;

    const MapDocument *m_doc = nullptr;
    QImage m_thumb;          // one pixel per map tile
    int m_pxPerTile = 2;     // display scale factor
    QRectF m_viewportScene;  // last known visible scene rect
};
