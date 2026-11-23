#pragma once

#include "MapRenderer.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QVector>
#include <memory>
#include <vector>

class MapDocument;
class QWidget;
namespace Op2Lib { class BlendMap; }

// Copied map region: raw mapping indices and cell types, row-major.
struct TileClipboardData {
    int width = 0;
    int height = 0;
    std::vector<int> mappings;
    std::vector<int> cellTypes;
    quint64 generation = 0; // bumps on every new copy (Tile Selection auto-fit)
    bool isEmpty() const { return width <= 0 || height <= 0; }
    int mappingAt(int x, int y) const { return mappings[static_cast<std::size_t>(y * width + x)]; }
    int cellTypeAt(int x, int y) const { return cellTypes[static_cast<std::size_t>(y * width + x)]; }
};

// The tile group loaded in the Tile Groups palette.
struct TileGroupData {
    int width = 0;
    int height = 0;
    std::vector<int> mappings; // row-major
    QString fileName;          // without extension
    bool isLoaded() const { return width > 0 && height > 0 && !mappings.empty(); }
};

// Text and colours for the sidebar "Editing" section.
struct ToolLabels {
    QString tool;
    QColor toolColor;
    QString placement;
    QColor placementColor;
    QPixmap swatch;   // 32x32 tool icon (may be null)
    QColor swatchBack;
};

// All editing state and the tool/mode state machine of OP2Mapper3
// (GlobalVariables + fMain.Editing/Clipboard/TileGroups/Terrain/MapAnalysis).
// MapView forwards mouse input here; palettes and MainWindow call the
// public operations; changes are announced through signals.
class Editor : public QObject
{
    Q_OBJECT
public:
    Editor(MapDocument *doc, QWidget *dialogParent, QObject *parent = nullptr);
    ~Editor() override;

    MapDocument *document() const { return m_doc; }
    bool hasMap() const;

    // ---- Map lifecycle
    void onMapLoaded();  // after a successful load: resets tools/view state
    void onMapClosed();

    // ---- Edit mode
    bool editMode() const { return m_editMode; }
    void setEditMode(bool on);
    bool singleTileMode() const { return m_singleTile; }
    void setSingleTileMode(bool on);

    // ---- View state
    int renderMode() const { return m_renderMode; }
    void setRenderMode(int mode);
    const MapRenderer::Overlays &overlays() const { return m_overlays; }
    void setShowGrid(bool on);
    void setShowLargeGrid(bool on);
    void setShowCoords(bool on);
    void setGridSizes(int largeGridSize, int coordSpacing);

    // ---- Selected tile (white box + sidebar "Tile Under Cursor")
    QPoint currentTile() const { return m_currentTile; }
    void setCurrentTile(const QPoint &t);

    // ---- Tools
    int activeMapping() const { return m_activeMapping; }
    bool tileToolActive() const { return m_activeMapping >= 0 && !m_selectedTileImage.isNull(); }
    bool cellToolActive() const { return m_cellToolActive; }
    int activeCellType() const { return m_activeCellType; }
    int selectedTilesetIndex() const { return m_selectedTilesetIndex; }
    int selectedImageIndex() const { return m_selectedImageIndex; }
    // Tile Set palette click. Returns false if refused (copy mode,
    // restricted render mode) so the palette keeps its old highlight.
    bool pickTilesetTile(int tilesetIndex, int imageIndex);
    void pickCellType(int cellType);
    void selectTileUnderCursor(const QPoint &tile);
    void clearCurrentTool(); // "Clear Tool Selection"

    // ---- Copy / paste
    bool copyMode() const { return m_copyMode; }
    int copyStage() const { return m_copyStage; }
    QRect copyRect() const; // tile rect being selected / just copied
    void beginCopy();       // Edit > Copy Tiles / Ctrl+C
    void exitCopyMode();
    const TileClipboardData &clipboard() const { return m_clipboard; }
    void clearClipboard();
    bool pasteMode() const { return m_pasteMode; }
    void beginPaste();      // Edit > Paste Tiles / Ctrl+V
    void exitPasteMode();
    QPoint pastePreviewPos() const { return m_pastePreview; }
    const QImage &pastePreviewImage() const { return m_pastePreviewImage; }
    bool pasteCellTypes() const;
    void setPasteCellTypes(bool on);

    // ---- Tile groups
    const TileGroupData &tileGroup() const { return m_tileGroup; }
    void setTileGroup(const TileGroupData &g);
    void clearTileGroup();
    bool tileGroupMode() const { return m_groupMode; }
    void beginTileGroupPlacement(bool fromButton);
    void exitTileGroupPlacement();
    QPoint tileGroupPreviewPos() const { return m_groupPreview; }
    const QImage &tileGroupPreviewImage() const { return m_groupPreviewImage; }

    // ---- Terrain brush
    bool terrainToolActive() const; // VB IsTerrainToolActive predicate
    int terrainGround() const { return m_ground; }
    QString terrainGroundName() const { return m_groundName; }
    QColor terrainGroundColor() const { return m_groundColor; }
    int brushSize() const { return m_brushSize; }
    void setBrushSize(int size);
    void stepBrushSize(int delta);
    void armTerrain(int ground, const QString &name, const QColor &color);
    void disarmTerrain();
    void stopTerrainTools(); // brush + cliff
    QPoint brushHover() const { return m_brushHover; }
    void cleanUpTransitions();

    // ---- Draw cliff
    bool cliffToolActive() const;
    void armCliff();
    void disarmCliff();
    int cliffSide() const { return m_cliffSide; }
    int cliffHeight() const { return m_cliffHeight; }
    void setCliffOptions(int side, int height);
    const QVector<QPoint> &cliffPoints() const { return m_cliffPoints; }
    QPoint cliffCursor() const { return m_cliffCursor; }
    void placeCliff();
    // Enter / Backspace / Esc while drawing. Returns true if consumed.
    bool handleCliffKey(int key);

    // ---- Generated map markers (spawn points and volcanoes). Outpost 2 maps
    // don't store them, so they are only a guide and are cleared when another
    // map is loaded or the map is closed.
    void setGeneratedMarkers(const QVector<QPoint> &spawns, const QVector<QPoint> &volcanoes);
    const QVector<QPoint> &generatedSpawns() const { return m_genSpawns; }
    const QVector<QPoint> &generatedVolcanoes() const { return m_genVolcanoes; }

    // ---- Enclosed areas
    bool highlightEnclosed() const { return m_highlightOn; }
    void setHighlightEnclosed(bool on);
    const QVector<QPoint> &enclosedTiles() const { return m_enclosed; }
    void findAndFixEnclosedAreas();

    // ---- Mouse input from MapView (tile coords, already in bounds unless noted)
    void mapLeftPress(const QPoint &tile);
    void mapMouseMove(const QPoint &tile, bool inside, bool leftHeld);
    void mapLeftRelease();
    // Esc in the main window. Returns true if consumed.
    bool handleEscape();
    void endEditDrag();
    bool editDragging() const { return m_editDragging; }

    ToolLabels toolLabels() const;
    QString statusName() const; // status-bar item 0 text
    void setStatusOverride(const QString &text);

    static QString formatTile(const QPoint &t) { return QStringLiteral("%1, %2").arg(t.x() + 1).arg(t.y() + 1); }

signals:
    void toolChanged();          // labels, palettes' highlight
    void overlayChanged();       // repaint map overlays only
    void renderModeChanged(int mode);
    void overlaysChanged();      // grid / large grid / coords
    void editModeChanged(bool on);
    void clipboardChanged();
    void tileGroupChanged();
    void currentTileChanged();
    void statusChanged();
    void terrainStateChanged();  // terrain palette sync
    void highlightChanged(bool on);
    void tilesetSelectionCleared();
    void cellTypeSelectionCleared();
    void pasteAvailabilityChanged();
    void cursorChanged();

private:
    void resetToolUi(bool keepCopyData);
    void finishCopySelection(const QPoint &end);
    void completeCopy();
    void onCopyTimer();
    void rebuildPastePreview();
    void rebuildGroupPreview();
    void executePaste(const QPoint &tile);
    void executeTileGroupPlacement(const QPoint &tile);
    void handleTilePlacement(const QPoint &tile);
    void handleCellTypeClick(const QPoint &tile);
    void beginTerrainStroke(const QPoint &tile);
    void terrainDab(const QPoint &tile);
    void endTerrainStroke();
    void applyBlendChanges(const std::vector<struct TileChange> &changes, const QString &text);
    void cliffClick(const QPoint &tile);
    void recomputeEnclosed();
    bool refuseInRestrictedMode(const QString &action);
    bool lockLastRow() const;
    void info(const QString &title, const QString &text);
    void warn(const QString &title, const QString &text);

    MapDocument *m_doc;
    QWidget *m_dialogParent;

    bool m_editMode = false;
    bool m_singleTile = true;
    int m_renderMode = MapRenderer::GameMap;
    MapRenderer::Overlays m_overlays;
    QPoint m_currentTile{ -1, -1 };

    int m_activeMapping = -1;
    QImage m_selectedTileImage;
    QString m_selectedTilesetName;
    int m_selectedTilesetIndex = -1;
    int m_selectedImageIndex = -1;
    bool m_cellToolActive = false;
    int m_activeCellType = 0;

    bool m_editDragging = false;
    QPoint m_lastEditTile{ -1, -1 };

    bool m_copyMode = false;
    int m_copyStage = 0;
    QPoint m_copyStart{ -1, -1 };
    QPoint m_copyEnd{ -1, -1 };
    QPoint m_copyHover{ -1, -1 };
    bool m_copyDragging = false;
    QTimer m_copyTimer;
    TileClipboardData m_clipboard;
    quint64 m_nextGeneration = 1;

    bool m_pasteMode = false;
    QPoint m_pastePreview{ -1, -1 };
    QImage m_pastePreviewImage;

    TileGroupData m_tileGroup;
    bool m_groupMode = false;
    QPoint m_groupPreview{ -1, -1 };
    QImage m_groupPreviewImage;

    bool m_terrainActive = false;
    int m_ground = -1;
    QString m_groundName;
    QColor m_groundColor = Qt::white;
    int m_brushSize = 5;
    QPoint m_brushHover{ -1, -1 };
    std::unique_ptr<Op2Lib::BlendMap> m_strokeBlend;
    quint64 m_strokeSeed = 0;
    QPoint m_lastDab{ -1, -1 };

    bool m_cliffActive = false;
    QVector<QPoint> m_cliffPoints;
    QPoint m_cliffCursor{ -1, -1 };
    int m_cliffSide = 0;
    int m_cliffHeight = 3;

    QVector<QPoint> m_genSpawns;
    QVector<QPoint> m_genVolcanoes;

    bool m_highlightOn = false;
    QVector<QPoint> m_enclosed;
    QTimer m_enclosedTimer;

    QString m_statusOverride;
};
