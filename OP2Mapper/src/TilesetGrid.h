#pragma once

#include <QPoint>
#include <QWidget>

class QScrollArea;

// The tile grid of the Tile Set palette (OP2Mapper3 fTileSet): 6 columns,
// 33 px stride, grey tile borders, red 2 px box on the selected tile.
// Left-click picks a tile (clicks right of the 6th column are ignored);
// right-drag pans the scroll area.
class TilesetGrid : public QWidget
{
    Q_OBJECT
public:
    static constexpr int TileSize = 32;
    static constexpr int Stride = 33;
    static constexpr int Columns = 6;

    TilesetGrid(QScrollArea *scroll, QWidget *parent = nullptr);

    void setTilesetImage(const QImage *image);
    int selectedTile() const { return m_selected; }
    void setSelectedTile(int index);

signals:
    void tileClicked(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int tileCount() const;
    void updateSize();

    QScrollArea *m_scroll;
    const QImage *m_image = nullptr;
    int m_selected = -1;
    bool m_panning = false;
    QPoint m_panStart;
};
