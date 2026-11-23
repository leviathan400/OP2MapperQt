#pragma once

#include "NewMapDialog.h"

#include <QMainWindow>
#include <QStringList>
#include <QTimer>
#include <memory>

class CellTypesWindow;
class Editor;
class MapDocument;
class GenerateMapDialog;
class MapView;
class MiniMap;
class Sidebar;
class TerrainWindow;
class TileGroupsWindow;
class TileSelectWindow;
class TilesetWindow;
class ToolPalette;
class QAction;
class QLabel;
class QMenu;
struct MapLoadInfo;
namespace Op2Lib { struct GeneratedMap; }

// Application shell (OP2Mapper3 fMain): menus, map panel, sidebar, status
// bar, the six tool palettes (shown with Edit Mode), and the file flows.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

    // Startup after construction: command line (/op2dir, map file) and the
    // first-run folder check. Returns false if the app should exit.
    bool startup(const QStringList &args);
    // A second launch forwarded its arguments to this instance.
    void handleSecondInstance(const QStringList &args);

protected:
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void buildMenus();
    void buildStatusBar();
    void wireSignals();

    // File flows
    bool ensureOp2Folder();
    bool loadMapFile(const QString &path, bool includeInRecent);
    void showTilesetWarnings(const MapLoadInfo &info);
    bool confirmDiscardChanges();
    void onNewMap();
    void onGenerateMap();
    bool isUntouchedGeneratedMap() const;
    bool showGeneratedMap(const Op2Lib::GeneratedMap &gen, const QString &fileName);
    bool createNewMap(const NewMapDialog::Result &r);
    void onOpenMap();
    bool onSaveMap();
    void onCloseMap();
    void onSettings();
    void rebuildRecentMenu();
    void openDroppedFiles(const QStringList &paths);
    void onExportJson();
    void onImportJson();

    // UI refresh
    void updateTitle();
    void updateStatus();
    void updateTileInfo();
    void updateToolLabels();
    void updateUndoRedo();
    void updateZoomLabel();
    void updateMapActions();
    void refreshSidebarMap();
    void syncViewChecks();

    // Edit mode
    void toggleEditMode();
    void onEditModeChanged(bool on);
    QList<ToolPalette *> palettes() const;

    // Image
    void exportImage(double scale, const QString &suffix, int gridMode);
    void exportOverview();
    void takeScreenshot();
    void configureGrid();
    QString exportFolder() const;

    void restoreMainPosition();
    void saveMainPosition();

    std::unique_ptr<MapDocument> m_doc;
    Editor *m_editor = nullptr;
    MapView *m_view = nullptr;
    Sidebar *m_sidebar = nullptr;

    TilesetWindow *m_tileset = nullptr;
    CellTypesWindow *m_cellTypes = nullptr;
    TileSelectWindow *m_tileSelect = nullptr;
    TileGroupsWindow *m_tileGroups = nullptr;
    MiniMap *m_miniMap = nullptr;
    TerrainWindow *m_terrain = nullptr;

    QLabel *m_statusName = nullptr;
    QLabel *m_statusPath = nullptr;
    QLabel *m_statusCoords = nullptr;

    QAction *m_saveAct = nullptr;
    QAction *m_closeAct = nullptr;
    QAction *m_exportJsonAct = nullptr;
    QAction *m_undoAct = nullptr;
    QAction *m_redoAct = nullptr;
    QAction *m_copyAct = nullptr;
    QAction *m_pasteAct = nullptr;
    QAction *m_pasteCellsAct = nullptr;
    QAction *m_singleTileAct = nullptr;
    QAction *m_highlightAct = nullptr;
    QAction *m_gridAct = nullptr;
    QAction *m_largeGridAct = nullptr;
    QAction *m_coordsAct = nullptr;
    QAction *m_zoomAct = nullptr;
    QAction *m_onTopAct = nullptr;
    QMenu *m_recentMenu = nullptr;
    QMenu *m_renderMenu = nullptr;
    QMenu *m_imageMenu = nullptr;

    QTimer m_positionTimer;
    bool m_positionRestored = false;

    GenerateMapDialog *m_generateDialog = nullptr;
    // True while the open map is the last one the generator produced.
    bool m_generatedMapShown = false;
};
