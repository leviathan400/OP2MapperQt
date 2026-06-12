#pragma once

#include <QGraphicsScene>
#include <QMetaObject>

class MapDocument;

// Renders the map itself. There are no QGraphicsItems for tiles — the whole
// tile layer is painted in drawBackground() straight from the document,
// clipped to the exposed rect, so repaint cost scales with the visible
// area rather than the map size. Edits invalidate per-tile rects only
// (tileChanged → invalidateTile), keeping single-tile paints cheap at any
// zoom. Items added by MapView (selection rect, paste ghost) float above
// the background layer.
class MapScene : public QGraphicsScene
{
    Q_OBJECT
public:
    // Mirrors the 5 render modes from VB MapRenderer.vb.
    enum class RenderMode {
        GameMap = 0,            // tile graphics, no overlay
        CellTypeOverlay,        // tile graphics + 45%-alpha cell-type colors
        CellTypeOverlayAll,     // tile graphics + colors + per-tile labels
        CellTypeBasic,          // solid cell-type colors only (no game tiles)
        CellTypeAll,            // solid colors + labels (no game tiles)
    };
    Q_ENUM(RenderMode)

    explicit MapScene(QObject *parent = nullptr);

    void setDocument(const MapDocument *doc);

    void setRenderMode(RenderMode mode);
    RenderMode renderMode() const { return m_renderMode; }

    void setShowGrid(bool on);
    bool showGrid() const { return m_showGrid; }
    void setShowLargeGrid(bool on);
    bool showLargeGrid() const { return m_showLargeGrid; }
    void setShowCoords(bool on);
    bool showCoords() const { return m_showCoords; }

    // Invalidate just the rect of one map tile (cheap repaint after edit).
    void invalidateTile(int x, int y);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    const MapDocument *m_doc = nullptr;
    QMetaObject::Connection m_tileChangedConn;
    RenderMode m_renderMode = RenderMode::GameMap;
    bool m_showGrid = false;
    bool m_showLargeGrid = false;
    bool m_showCoords = false;
};
