#pragma once

#include <QGraphicsView>
#include <QRect>

class MapDocument;
class MapScene;
class TileClipboard;
class QGraphicsPixmapItem;
class QGraphicsRectItem;

// The interactive viewport. One mode is active at a time, with this
// precedence in the mouse handlers (highest first):
//
//   copy-click  — two-click rectangle pick (enterCopyMode)
//   paste       — ghost preview follows cursor, click commits (enterPasteMode)
//   shift-drag  — rectangle selection (modifier-triggered, any mode below)
//   paint       — tile graphic or cell type, whichever palette selected last
//   pan         — default ScrollHandDrag when nothing is armed
//
// Entering any mode exits the conflicting ones, and right-click always
// cancels the active mode before falling through to the context menu.
// MainWindow owns the mode transitions triggered from menus/palettes; this
// class owns the ones triggered by mouse input.
class MapView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapView(QWidget *parent = nullptr);

    void setDocument(MapDocument *doc);
    void setShowGrid(bool on);
    void setShowLargeGrid(bool on);
    void setShowCoords(bool on);
    void setRenderMode(int mode); // MapScene::RenderMode cast to int

    double zoom() const; // 1.0 == 100%

    // Rect of the scene currently visible in the viewport (scene coords).
    QRectF visibleSceneRect() const;

    // Editing: when a tile is selected, left-click paints. Pass (-1,-1) to
    // clear and return to pan mode. Selecting a tile clears any cell-type selection.
    void setSelectedTile(int tilesetIdx, int imageIdx);
    bool hasSelectedTile() const { return m_selectedTileset >= 0 && m_selectedImage >= 0; }

    // Editing: when a cell type is selected, left-click paints it. Pass -1 to
    // clear. Selecting a cell type clears any tile selection.
    void setSelectedCellType(int cellTypeValue);
    bool hasSelectedCellType() const { return m_selectedCellType >= 0; }

    // Tile-region selection (Shift+drag). Empty rect when no selection.
    QRect selectionTileRect() const { return m_selection; }
    bool hasSelection() const { return !m_selection.isEmpty(); }
    void clearSelection();

    // VB-style two-click selection mode. First click sets the start tile,
    // second click finalizes the rectangle (selectionChanged emits). Right-
    // click cancels. Mutually exclusive with paint/paste modes.
    void enterCopyMode();
    void exitCopyMode();
    bool inCopyMode() const { return m_copyClickStage > 0; }

    // Paste mode — left-click commits the paste, right-click cancels.
    // The clipboard pointer must outlive the paste mode.
    void enterPasteMode(const TileClipboard *clip);
    void exitPasteMode();
    bool inPasteMode() const { return m_pasteMode; }

signals:
    // Emitted when MapView clears its own selection (e.g. on right-click).
    // Tool palettes listen so their visual selection stays in sync.
    void selectionCleared();

    // Emitted when the selection rectangle changes (including becoming empty).
    void selectionChanged(QRect tileRect);

    // Paste mode entered/exited.
    void pasteModeChanged(bool active);

    // Copy-mode (two-click) stage: 0 idle, 1 awaiting start, 2 awaiting end.
    void copyModeStageChanged(int stage);

    // Fired on right-click when no paint/paste/copy mode is active and there
    // is no selection. Position is in global screen coords so MainWindow can
    // pop a menu at the cursor.
    void contextMenuRequested(QPoint globalPos);

public slots:
    void resetZoom();
    void centerOnScene(QPointF scenePos);

signals:
    // 1-based OP2 coordinates. (0, 0) means cursor is off the map.
    void tileHovered(int x, int y);
    void zoomChanged(double factor);
    void viewportChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool paintAtViewportPos(QPoint vpPos);

private:
    void updatePaintCursor();
    void endStrokeIfActive();
    QPoint tileAtViewportPos(QPoint vpPos) const;
    void updateSelectionItem();
    void rebuildPastePreview();
    void movePastePreviewToTile(QPoint tilePt);

    MapScene *m_scene = nullptr;
    MapDocument *m_doc = nullptr;
    int m_selectedTileset = -1;
    int m_selectedImage = -1;
    int m_selectedCellType = -1;

    // Paint-stroke lifecycle. m_inStroke spans the whole press→release
    // gesture; m_strokeMacroOpen flips only once a paint in the stroke
    // actually changes the map (the undo macro is opened lazily so no-op
    // clicks never commit an empty macro / spurious dirty flag).
    bool m_inStroke = false;
    bool m_strokeMacroOpen = false;

    QRect m_selection;                  // in tile coords; empty == none
    QPoint m_selectionDragStart;        // tile coords at mousedown
    bool m_selecting = false;
    QGraphicsRectItem *m_selectionItem = nullptr;

    const TileClipboard *m_clipboard = nullptr;
    bool m_pasteMode = false;
    QGraphicsPixmapItem *m_pastePreview = nullptr;

    int m_copyClickStage = 0; // 0 idle, 1 awaiting start, 2 awaiting end
};
