#pragma once

#include <QWidget>

// Scrollable picker showing every tile of one tileset as a fixed 6-column
// grid of 32×32 thumbnails (the column count matches OP2Mapper3's fTileSet
// layout, so the two editors feel the same). The widget paints directly
// from the TileImageCache strip — tileset images are stored as a single
// 32px-wide column of tiles, so a thumbnail at imageIdx is just the slice
// at y = imageIdx * 32. Left-click selects (red outline), right-click asks
// the owner to clear the active selection.
class TilesetGrid : public QWidget
{
    Q_OBJECT
public:
    static constexpr int TileSize = 32;
    static constexpr int TilePadding = 1;
    static constexpr int TilesPerRow = 6;

    explicit TilesetGrid(QWidget *parent = nullptr);

    // image is owned by the TileImageCache. Pass nullptr to clear.
    void setTilesetImage(const QImage *image);

    int selectedTile() const { return m_selectedTile; }
    void setSelectedTile(int imageIdx);

signals:
    // Left-click on a thumbnail; imageIdx is the tile's index within the
    // currently displayed tileset (not a map tileMappingIndex).
    void tileClicked(int imageIdx);
    // Right-click anywhere in the grid — owner should drop the paint tool.
    void clearSelectionRequested();

protected:
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int tileCount() const;
    void updateGeometryForImage();

    const QImage *m_image = nullptr;
    int m_selectedTile = -1;
};
