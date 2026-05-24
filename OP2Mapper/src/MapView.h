#pragma once

#include <QGraphicsView>

class MapDocument;
class MapScene;

class MapView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapView(QWidget *parent = nullptr);

    void setDocument(const MapDocument *doc);
    void setShowCellTypes(bool on);
    void setShowGrid(bool on);

signals:
    // 1-based OP2 coordinates. (0, 0) means cursor is off the map.
    void tileHovered(int x, int y);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    MapScene *m_scene = nullptr;
    const MapDocument *m_doc = nullptr;
};
