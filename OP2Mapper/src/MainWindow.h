#pragma once

#include <QMainWindow>
#include <QRect>
#include <memory>

class CellTypesWindow;
class MapDocument;
class MapView;
class MiniMap;
class TileClipboard;
class TileGroupsWindow;
class TileSelectWindow;
class TilesetWindow;
class QLabel;
class QMenu;

// Application shell. Owns the single MapDocument and TileClipboard for the
// app's lifetime (both survive map close — the document just reset()s, so
// QActions bound to its undo stack never dangle), the central MapView, and
// the five floating tool palettes (MiniMap, TilesetWindow, CellTypesWindow,
// TileSelectWindow, TileGroupsWindow). Palettes hide when no map is loaded
// and reappear via finishMapLoadUi() on the next load.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNewMap();
    void onOpenMap();
    void onCloseMap();
    void onSaveMap();
    void onSaveMapAs();
    void onSetOp2Folder();
    void onTileHovered(int x, int y);
    void onZoomChanged(double factor);
    void onCopyTiles();
    void onPasteTiles();
    void onSelectionChanged(QRect tileRect);
    void onExportToJson();
    void onImportFromJson();
    void onLaunchOp2();
    void onOpenOp2Folder();
    void onShowSettings();
    void onShowAbout();
    void onExportImage();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QString ensureOp2Folder();
    void updateTitle();
    bool saveToPath(const QString &path);
    // Loads map from disk and performs all the after-load UI setup. If
    // markUntitled is true, clears the document's current path so Save
    // prompts for a new location (used by File → New).
    bool loadMapFromPath(const QString &path, bool markUntitled = false);
    // Shared post-load UI setup (view wiring, palettes, action enables).
    // Every map-arrival path (open/new/recent/drop/import) must funnel
    // through this — see the definition for why it must stay one code path.
    void finishMapLoadUi(const QString &statusText);
    // If the current map has unsaved changes, asks the user. Returns true
    // if it's OK to proceed (no changes, Discard, or Save-succeeded); false
    // if the user cancelled or the save failed.
    bool confirmDiscardChanges();
    void rebuildRecentFilesMenu();

    MapView *m_mapView = nullptr;
    QLabel *m_statusPath = nullptr;
    QLabel *m_statusCoords = nullptr;
    QAction *m_closeAction = nullptr;
    QAction *m_gridAction = nullptr;
    QAction *m_largeGridAction = nullptr;
    QAction *m_coordsAction = nullptr;
    QAction *m_resetZoomAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_exportJsonAction = nullptr;
    QMenu *m_recentFilesMenu = nullptr;
    bool m_copyAfterSelection = false;
    QLabel *m_statusZoom = nullptr;
    MiniMap *m_miniMap = nullptr;
    TilesetWindow *m_tilesetWindow = nullptr;
    CellTypesWindow *m_cellTypesWindow = nullptr;
    TileSelectWindow *m_tileSelectWindow = nullptr;
    TileGroupsWindow *m_tileGroupsWindow = nullptr;
    std::unique_ptr<MapDocument> m_doc;
    std::unique_ptr<TileClipboard> m_clipboard;
};
