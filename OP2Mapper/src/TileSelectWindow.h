#pragma once

#include "ToolPalette.h"

#include <QImage>
#include <QPoint>

class Editor;
class QScrollArea;
class TileSelectCanvas;

// "Tile Selection" palette (OP2Mapper3 fTileSelect, tool 3): shows the
// copied region. Everything is in menus: the title-bar menu (Large Window,
// Zoom 100/75/50/25, Zoom to Fit Window, Show Cell Types) and a right-click
// menu (Export Tile Group, Show Cell Types). Ctrl+wheel steps the zoom;
// right-drag pans.
class TileSelectWindow : public ToolPalette
{
    Q_OBJECT
public:
    TileSelectWindow(Editor *editor, QWidget *mainWindow);

    void refreshPreview();

signals:
    void tileGroupExported(); // refresh Tile Groups on "Exported"

protected:
    void addMenuExtras(QMenu *menu) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    friend class TileSelectCanvas;
    void applyWindowSize();
    void setLargeWindow(bool on);
    void setZoom(double zoom, bool fit);
    void setShowCellTypes(bool on);
    void stepZoom(int delta);
    int previewTileSize(int w, int h) const;
    void pickZoomForNewCopy(int w, int h);
    void exportTileGroup();
    void showContextMenu(const QPoint &globalPos);

    Editor *m_editor;
    QScrollArea *m_scroll = nullptr;
    TileSelectCanvas *m_canvas = nullptr;
    QImage m_preview;
    bool m_large = false;
    double m_zoom = 1.0;
    bool m_fit = false;
    bool m_showCellTypes = false;
    quint64 m_autoZoomedGeneration = 0;
};
