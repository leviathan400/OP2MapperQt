#include "MainWindow.h"

#include "AppInfo.h"
#include "AppLog.h"
#include "BlankMaps.h"
#include "CellTypeInfo.h"
#include "CellTypesWindow.h"
#include "Editor.h"
#include "MapDocument.h"
#include "MapJsonIO.h"
#include "MapRenderer.h"
#include "MapView.h"
#include "MiniMap.h"
#include "OP2Launcher.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "Sidebar.h"
#include "TerrainWindow.h"
#include "TileGroupsWindow.h"
#include "TileImageCache.h"
#include "TileSelectWindow.h"
#include "TilesetWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QScreen>
#include <QShortcut>
#include <QSpinBox>
#include <QStatusBar>
#include <QUrl>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace {

const QString kAppName = QString::fromLatin1(AppInfo::Name);

enum GridMode { NoGrid = 0, SmallGrid, LargeGrid };

bool saveJpegSafely(const QImage &img, const QString &path, int quality, QString &error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        error = f.errorString();
        return false;
    }
    QImageWriter w(&f, "jpg");
    w.setQuality(quality);
    if (!w.write(img)) {
        error = w.errorString();
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) {
        error = f.errorString();
        return false;
    }
    return true;
}

} // namespace

MainWindow::MainWindow()
    : m_doc(std::make_unique<MapDocument>())
{
    setWindowTitle(kAppName);
    setWindowIcon(QIcon(QStringLiteral(":/images/IconLarge.ico")));
    resize(1200, 800);

    m_editor = new Editor(m_doc.get(), this, this);
    m_view = new MapView(m_editor, this);
    m_sidebar = new Sidebar(this);
    auto *central = new QWidget(this);
    auto *h = new QHBoxLayout(central);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    h->addWidget(m_view, 1);
    h->addWidget(m_sidebar);
    setCentralWidget(central);

    m_tileset = new TilesetWindow(m_editor, this);
    m_cellTypes = new CellTypesWindow(m_editor, this);
    m_tileSelect = new TileSelectWindow(m_editor, this);
    m_tileGroups = new TileGroupsWindow(m_editor, this);
    m_miniMap = new MiniMap(m_editor, this);
    m_terrain = new TerrainWindow(m_editor, this);

    buildMenus();
    buildStatusBar();
    wireSignals();

    m_positionTimer.setSingleShot(true);
    m_positionTimer.setInterval(400);
    connect(&m_positionTimer, &QTimer::timeout, this, &MainWindow::saveMainPosition);

    restoreMainPosition();
    updateMapActions();
    updateUndoRedo();
    updateToolLabels();
    updateTitle();
    updateStatus();
}

MainWindow::~MainWindow() = default;

// ---- Menus -------------------------------------------------------------------

void MainWindow::buildMenus()
{
    // File
    QMenu *file = menuBar()->addMenu(tr("&File"));
    QAction *a = file->addAction(tr("&New Map"), this, &MainWindow::onNewMap);
    a->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    a = file->addAction(tr("&Open Map"), this, &MainWindow::onOpenMap);
    a->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    m_recentMenu = file->addMenu(tr("Recent Files"));
    rebuildRecentMenu();
    m_saveAct = file->addAction(tr("&Save Map"), this, [this] { onSaveMap(); });
    m_saveAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    m_closeAct = file->addAction(tr("&Close Map"), this, &MainWindow::onCloseMap);
    file->addSeparator();
    m_exportJsonAct = file->addAction(tr("Export to &JSON..."), this, &MainWindow::onExportJson);
    file->addAction(tr("&Import from JSON..."), this, &MainWindow::onImportJson);
    file->addSeparator();
    file->addAction(tr("S&ettings"), this, &MainWindow::onSettings);
    file->addSeparator();
    file->addAction(tr("Launch Outpost 2"), this, [this] { OP2Launcher::launchOutpost2(this); });
    file->addAction(tr("Launch OP2 into DLL Picker"), this, [this] { OP2Launcher::launchDllPicker(this); });
    file->addAction(tr("Open Outpost 2 Folder"), this, [this] { OP2Launcher::openGameFolder(this); });
    file->addSeparator();
    file->addAction(tr("E&xit"), this, &QWidget::close);

    // Edit
    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    m_undoAct = edit->addAction(tr("Undo"), this, [this] { m_doc->undo(); });
    m_undoAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    m_redoAct = edit->addAction(tr("Redo"), this, [this] { m_doc->redo(); });
    m_redoAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    edit->addSeparator();
    m_copyAct = edit->addAction(tr("Copy Tiles"), m_editor, &Editor::beginCopy);
    m_copyAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
    m_pasteAct = edit->addAction(tr("Paste Tiles"), m_editor, &Editor::beginPaste);
    m_pasteAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
    m_pasteCellsAct = edit->addAction(tr("Paste Includes Cell Types"));
    m_pasteCellsAct->setCheckable(true);
    m_pasteCellsAct->setChecked(Settings::pasteCellTypes());
    connect(m_pasteCellsAct, &QAction::toggled, m_editor, &Editor::setPasteCellTypes);
    edit->addSeparator();
    m_singleTileAct = edit->addAction(tr("Single Tile Edit Mode"));
    m_singleTileAct->setCheckable(true);
    m_singleTileAct->setChecked(true);
    connect(m_singleTileAct, &QAction::toggled, m_editor, &Editor::setSingleTileMode);
    edit->addSeparator();
    edit->addAction(tr("Find && Fix Enclosed Areas..."), m_editor, &Editor::findAndFixEnclosedAreas);
    m_highlightAct = edit->addAction(tr("Highlight Enclosed Areas"));
    m_highlightAct->setCheckable(true);
    connect(m_highlightAct, &QAction::triggered, m_editor, &Editor::setHighlightEnclosed);

    // View
    QMenu *view = menuBar()->addMenu(tr("&View"));
    m_gridAct = view->addAction(tr("Show Grid"));
    m_gridAct->setCheckable(true);
    m_gridAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(m_gridAct, &QAction::triggered, m_editor, &Editor::setShowGrid);
    m_largeGridAct = view->addAction(tr("Show Large Grid"));
    m_largeGridAct->setCheckable(true);
    m_largeGridAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(m_largeGridAct, &QAction::triggered, m_editor, &Editor::setShowLargeGrid);
    m_coordsAct = view->addAction(tr("Show Coordinates"));
    m_coordsAct->setCheckable(true);
    m_coordsAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(m_coordsAct, &QAction::triggered, m_editor, &Editor::setShowCoords);
    m_zoomAct = view->addAction(tr("Enable Zoom"));
    m_zoomAct->setCheckable(true);
    m_zoomAct->setChecked(true);
    m_zoomAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_zoomAct, &QAction::triggered, this, [this](bool on) {
        m_view->setZoomEnabled(on);
        updateZoomLabel();
    });
    a = view->addAction(tr("Reset Zoom"), this, [this] { m_view->resetView(); });
    a->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    view->addSeparator();
    m_onTopAct = view->addAction(tr("Tool Windows Always On Top"));
    m_onTopAct->setCheckable(true);
    m_onTopAct->setChecked(ToolPalette::allToolWindowsAlwaysOnTop());
    connect(m_onTopAct, &QAction::triggered, this, [](bool on) { ToolPalette::setAllAlwaysOnTop(on); });
    view->addSeparator();
    view->addAction(tr("Configure Grid Size..."), this, &MainWindow::configureGrid);

    // Render
    m_renderMenu = menuBar()->addMenu(tr("&Render"));
    for (int mode = 0; mode < 5; ++mode) {
        m_renderMenu->addAction(MapRenderer::modeName(mode), this, [this, mode] {
            m_editor->setRenderMode(mode);
        });
    }

    // Image
    m_imageMenu = menuBar()->addMenu(tr("&Image"));
    a = m_imageMenu->addAction(tr("Screenshot"), this, &MainWindow::takeScreenshot);
    a->setShortcut(QKeySequence(Qt::Key_F12));
    m_imageMenu->addSeparator();
    QMenu *exp = m_imageMenu->addMenu(tr("Export Image"));
    exp->addAction(tr("Full Size"), this, [this] { exportImage(1.0, QString(), NoGrid); });
    exp->addAction(tr("1/8th Size"), this, [this] { exportImage(0.125, QStringLiteral("small"), NoGrid); });
    exp->addAction(tr("1/16th Size"), this, [this] { exportImage(0.0625, QStringLiteral("mini"), NoGrid); });
    exp->addAction(tr("Full Size (Showing Grid)"), this, [this] { exportImage(1.0, QStringLiteral("grid"), SmallGrid); });
    exp->addAction(tr("Full Size (Showing Large Grid)"), this, [this] { exportImage(1.0, QStringLiteral("largegrid"), LargeGrid); });
    m_imageMenu->addSeparator();
    m_imageMenu->addAction(tr("Export Overview"), this, &MainWindow::exportOverview);

    // Help
    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("About"), this, [this] { showAboutDialog(this); });

    // Keys handled before anything else (OP2Mapper3 ProcessCmdKey).
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, [this] { m_editor->handleEscape(); });
    for (int key : { int(Qt::Key_Return), int(Qt::Key_Enter), int(Qt::Key_Backspace) }) {
        auto *sc = new QShortcut(QKeySequence(key), this);
        connect(sc, &QShortcut::activated, this, [this, key] { m_editor->handleCliffKey(key); });
    }
    auto *ctrlB = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), this);
    connect(ctrlB, &QShortcut::activated, this, [this] {
        if (m_doc->isLoaded() && m_editor->editMode())
            m_editor->beginTileGroupPlacement(false);
    });
}

void MainWindow::buildStatusBar()
{
    m_statusName = new QLabel;
    m_statusPath = new QLabel(tr("N/A"));
    m_statusCoords = new QLabel(QStringLiteral("0,0"));
    statusBar()->addWidget(m_statusName);
    statusBar()->addWidget(m_statusPath, 1);
    statusBar()->addPermanentWidget(m_statusCoords);
}

void MainWindow::wireSignals()
{
    connect(m_doc.get(), &MapDocument::dirtyChanged, this, &MainWindow::updateTitle);
    connect(m_doc.get(), &MapDocument::undoRedoChanged, this, &MainWindow::updateUndoRedo);
    connect(m_doc.get(), &MapDocument::edited, this, [this] {
        m_miniMap->scheduleRefresh();
        updateUndoRedo();
    });
    connect(m_doc.get(), &MapDocument::regionChanged, this, [this](const QRect &r) {
        if (r.contains(m_editor->currentTile()))
            updateTileInfo();
    });

    connect(m_editor, &Editor::toolChanged, this, &MainWindow::updateToolLabels);
    connect(m_editor, &Editor::statusChanged, this, &MainWindow::updateStatus);
    connect(m_editor, &Editor::currentTileChanged, this, &MainWindow::updateTileInfo);
    connect(m_editor, &Editor::editModeChanged, this, &MainWindow::onEditModeChanged);
    connect(m_editor, &Editor::overlaysChanged, this, &MainWindow::syncViewChecks);
    connect(m_editor, &Editor::highlightChanged, m_highlightAct, &QAction::setChecked);
    connect(m_editor, &Editor::pasteAvailabilityChanged, this, &MainWindow::updateMapActions);
    connect(m_editor, &Editor::cellTypeSelectionCleared, m_cellTypes, [this] {
        // VB keeps the list highlight when a tile is picked; only an explicit
        // tool reset clears it.
        if (!m_editor->cellToolActive() && !m_editor->tileToolActive())
            m_cellTypes->clearSelection();
    });
    connect(m_tileSelect, &TileSelectWindow::tileGroupExported, this, [this] {
        m_tileGroups->refreshFromDisk(QStringLiteral("Exported"));
    });

    connect(m_view, &MapView::hoverTile, this, [this](int x, int y) {
        m_statusCoords->setText(tr("Coordinates: %1, %2").arg(x + 1).arg(y + 1));
    });
    connect(m_view, &MapView::zoomChanged, this, &MainWindow::updateZoomLabel);
    connect(m_view, &MapView::viewportChanged, this, [this] {
        m_miniMap->setViewportTiles(m_view->visibleTileRect());
    });
    connect(m_view, &MapView::filesDropped, this, &MainWindow::openDroppedFiles);
    connect(m_view, &MapView::contextMenuRequested, this, [this](const QPoint &global) {
        if (!m_doc->isLoaded())
            return;
        QMenu menu(this);
        QAction *select = menu.addAction(tr("Select Tile Under Cursor"));
        menu.addAction(m_gridAct);
        menu.addAction(m_largeGridAct);
        menu.addAction(m_coordsAct);
        menu.addAction(m_zoomAct);
        if (menu.exec(global) == select)
            m_editor->selectTileUnderCursor(m_view->tileAt(m_view->lastContextPos()));
    });
    connect(m_miniMap, &MiniMap::jumpToTile, m_view, &MapView::panToCenterTile);

    connect(m_sidebar, &Sidebar::resetZoomClicked, m_view, &MapView::resetView);
    connect(m_sidebar, &Sidebar::editModeClicked, this, &MainWindow::toggleEditMode);
    connect(m_sidebar, &Sidebar::clearToolClicked, m_editor, &Editor::clearCurrentTool);

    if (ToolPalette *n = ToolPalette::notifier())
        connect(n, &ToolPalette::allAlwaysOnTopChanged, m_onTopAct, &QAction::setChecked);
}

// ---- Refresh -----------------------------------------------------------------

void MainWindow::updateTitle()
{
    if (!m_doc->isLoaded() || m_doc->currentPath().isEmpty()) {
        setWindowTitle(kAppName);
        return;
    }
    setWindowTitle((m_doc->isDirty() ? QStringLiteral("* ") : QString()) + kAppName
                   + QStringLiteral(" - ") + QFileInfo(m_doc->currentPath()).fileName());
}

void MainWindow::updateStatus()
{
    m_statusName->setText(m_editor->statusName());
    m_statusPath->setText(m_doc->isLoaded() && !m_doc->currentPath().isEmpty()
                              ? QDir::toNativeSeparators(m_doc->currentPath()) : tr("N/A"));
}

void MainWindow::updateTileInfo()
{
    const QPoint t = m_editor->currentTile();
    if (!m_doc->isLoaded() || !m_doc->inBounds(t.x(), t.y())) {
        m_sidebar->setTileInfo(false, {}, {}, 0, 0, {}, {}, {});
        return;
    }
    const int mapping = m_doc->mappingIndexAt(t.x(), t.y());
    const QString ts = m_doc->tilesetNameOfMapping(mapping);
    m_sidebar->setTileInfo(true, Editor::formatTile(t), ts, m_doc->imageIndexOfMapping(mapping), mapping,
                           CellTypeInfo::enumName(m_doc->cellTypeAt(t.x(), t.y())),
                           MapRenderer::mappingImage(*m_doc, mapping), ts);
}

void MainWindow::updateToolLabels()
{
    m_sidebar->setToolLabels(m_editor->toolLabels(), m_editor->editMode());
}

void MainWindow::updateUndoRedo()
{
    const bool loaded = m_doc->isLoaded();
    m_undoAct->setEnabled(loaded && m_doc->canUndo());
    m_undoAct->setText(m_doc->canUndo() ? tr("Undo %1").arg(m_doc->undoText()) : tr("Undo"));
    m_redoAct->setEnabled(loaded && m_doc->canRedo());
    m_redoAct->setText(m_doc->canRedo() ? tr("Redo %1").arg(m_doc->redoText()) : tr("Redo"));
}

void MainWindow::updateZoomLabel()
{
    if (!m_doc->isLoaded()) {
        m_sidebar->setZoomText(tr("Zoom Level: N/A"), false);
        return;
    }
    if (!m_view->zoomEnabled())
        m_sidebar->setZoomText(tr("Zoom Level: 100% (Locked)"), true);
    else
        m_sidebar->setZoomText(tr("Zoom Level: %1%").arg(qRound(m_view->zoom() * 100)), false);
}

void MainWindow::updateMapActions()
{
    const bool loaded = m_doc->isLoaded();
    m_saveAct->setEnabled(loaded);
    m_closeAct->setEnabled(loaded);
    m_exportJsonAct->setEnabled(loaded);
    m_renderMenu->setEnabled(loaded);
    m_imageMenu->setEnabled(loaded);
    m_copyAct->setEnabled(loaded && m_editor->editMode());
    m_pasteAct->setEnabled(loaded && m_editor->editMode() && !m_editor->clipboard().isEmpty());
}

void MainWindow::syncViewChecks()
{
    const auto &o = m_editor->overlays();
    m_gridAct->setChecked(o.grid);
    m_largeGridAct->setChecked(o.largeGrid);
    m_coordsAct->setChecked(o.coords);
}

void MainWindow::refreshSidebarMap()
{
    if (!m_doc->isLoaded()) {
        m_sidebar->clearMap();
        return;
    }
    const auto *map = m_doc->map();
    m_sidebar->setMapInfo(QFileInfo(m_doc->currentPath()).fileName(), m_doc->widthTiles(),
                          m_doc->heightTiles(), static_cast<int>(map->tiles.size()));
    QStringList rows;
    int missing = 0;
    for (std::size_t i = 0; i < map->tilesetSources.size(); ++i) {
        const auto &src = map->tilesetSources[i];
        if (src.IsEmpty())
            continue;
        QString name = QString::fromStdString(src.tilesetFilename);
        if (!name.endsWith(QLatin1String(".bmp"), Qt::CaseInsensitive))
            name += QStringLiteral(".bmp");
        const int n = m_doc->cache() ? m_doc->cache()->tileCount(i) : 0;
        if (n > 0) {
            rows << tr("%1  (%2 %3)").arg(name).arg(n).arg(n == 1 ? tr("tile") : tr("tiles"));
        } else {
            rows << tr("%1  (not found)").arg(name);
            ++missing;
        }
    }
    m_sidebar->setTilesets(rows, missing);
    m_sidebar->setEditModeEnabled(true, m_editor->editMode());
}

// ---- Edit mode ---------------------------------------------------------------

QList<ToolPalette *> MainWindow::palettes() const
{
    return { m_tileset, m_cellTypes, m_tileSelect, m_tileGroups, m_miniMap, m_terrain };
}

void MainWindow::toggleEditMode()
{
    if (!m_doc->isLoaded())
        return;
    m_editor->setEditMode(!m_editor->editMode());
}

void MainWindow::onEditModeChanged(bool on)
{
    if (on) {
        Sounds::playFile(QDir(Settings::op2Path()).filePath(QStringLiteral("OPU/base/sounds/beep9.wav")));
        m_tileset->reloadFromDocument();
        m_cellTypes->clearSelection();
        m_tileGroups->refreshFromDisk();
        m_tileSelect->refreshPreview();
        QList<ToolPalette *> unplaced;
        for (ToolPalette *p : palettes()) {
            if (!p->restorePosition())
                unplaced << p;
            p->open();
        }
        if (!unplaced.isEmpty())
            ToolPalette::arrange(this, palettes(), true);
        m_miniMap->refreshThumbnail();
        m_miniMap->setViewportTiles(m_view->visibleTileRect());
        activateWindow();
    } else {
        for (ToolPalette *p : palettes()) {
            p->savePosition();
            p->hide();
        }
    }
    m_sidebar->setEditModeEnabled(m_doc->isLoaded(), on);
    updateMapActions();
    updateToolLabels();
    updateStatus();
}

// ---- File flows --------------------------------------------------------------

bool MainWindow::ensureOp2Folder()
{
    const QString op2 = Settings::op2Path();
    if (!op2.isEmpty() && QDir(op2).exists())
        return true;
    SettingsDialog dlg(this);
    dlg.exec();
    const QString again = Settings::op2Path();
    return !again.isEmpty() && QDir(again).exists();
}

bool MainWindow::confirmDiscardChanges()
{
    if (!m_doc->isLoaded() || !m_doc->isDirty())
        return true;
    const auto choice = QMessageBox::warning(this, tr("Unsaved Changes"),
        tr("The current map has unsaved changes. Save before continuing?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (choice == QMessageBox::Cancel)
        return false;
    if (choice == QMessageBox::No)
        return true;
    onSaveMap();
    return !m_doc->isDirty();
}

void MainWindow::showTilesetWarnings(const MapLoadInfo &info)
{
    if (!info.tilesetDirFound) {
        const QString op2 = Settings::op2Path();
        QMessageBox::warning(this, tr("Tilesets Not Found"),
            tr("Could not find the Outpost 2 tileset folder, so the map will show placeholder tiles.\n\n"
               "Looked in these folders under the Outpost 2 path:\n  OPU\\base\\tilesets\n  base\\tilesets\n  tilesets\n\n"
               "Outpost 2 path: %1\nCheck it in File → Settings.")
                .arg(op2.isEmpty() ? tr("(not set)") : op2));
        return;
    }
    if (info.tilesetsTotal > 0 && info.tilesetsLoaded == 0) {
        QMessageBox::warning(this, tr("Tileset Loading Warning"),
                             tr("Could not load any tileset files. The map may not display correctly."));
    } else if (info.tilesetsLoaded < info.tilesetsTotal) {
        QMessageBox::warning(this, tr("Tileset Loading Warning"),
            tr("Loaded %1 of %2 tilesets. Some tiles may not display correctly.")
                .arg(info.tilesetsLoaded).arg(info.tilesetsTotal));
    }
}

bool MainWindow::loadMapFile(const QString &path, bool includeInRecent)
{
    if (!ensureOp2Folder())
        return false;
    if (m_editor->editMode())
        m_editor->setEditMode(false);
    QString err;
    MapLoadInfo info;
    if (!m_doc->load(path, Settings::op2Path(), err, info)) {
        AppLog::write(QStringLiteral("Map"), QStringLiteral("Could not open %1: %2").arg(QDir::toNativeSeparators(path), err));
        QMessageBox::critical(this, tr("Error"), tr("Error loading map: %1").arg(err));
        return false;
    }
    AppLog::write(QStringLiteral("Map"), QStringLiteral("Opened %1").arg(QDir::toNativeSeparators(path)));
    m_editor->onMapLoaded();
    m_view->documentReplaced();
    showTilesetWarnings(info);
    if (includeInRecent) {
        Settings::addRecentFile(path);
        rebuildRecentMenu();
    }
    refreshSidebarMap();
    updateTileInfo();
    updateZoomLabel();
    updateMapActions();
    updateUndoRedo();
    updateTitle();
    updateStatus();
    syncViewChecks();
    if (m_miniMap->isVisible())
        m_miniMap->refreshThumbnail();
    return true;
}

void MainWindow::onOpenMap()
{
    if (!confirmDiscardChanges())
        return;
    if (!ensureOp2Folder())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Outpost 2 Map"), Settings::op2Path(),
        tr("Outpost 2 Map Files (*.map);;All Files (*.*)"));
    if (path.isEmpty())
        return;
    if (loadMapFile(path, true))
        QMessageBox::information(this, tr("Map Loaded"), tr("Map loaded successfully."));
}

void MainWindow::openDroppedFiles(const QStringList &paths)
{
    if (paths.isEmpty())
        return;
    const QString path = paths.first();
    if (!QFileInfo::exists(path))
        return;
    if (!confirmDiscardChanges())
        return;
    if (loadMapFile(path, true))
        QMessageBox::information(this, tr("Map Loaded"), tr("Map loaded successfully."));
}

bool MainWindow::onSaveMap()
{
    if (!m_doc->isLoaded())
        return false;
    QString start = m_doc->currentPath();
    if (start.isEmpty())
        start = QDir(Settings::workingPath()).filePath(QStringLiteral("New_Map.map"));
    else if (!QDir(QFileInfo(start).absolutePath()).exists())
        start = QFileInfo(start).fileName();
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Outpost 2 Map"), start,
        tr("Outpost 2 Map Files (*.map);;All Files (*.*)"));
    if (path.isEmpty())
        return false;
    QString err;
    if (!m_doc->save(path, err)) {
        AppLog::write(QStringLiteral("Map"), QStringLiteral("Could not save %1: %2").arg(QDir::toNativeSeparators(path), err));
        QMessageBox::critical(this, tr("Error"), tr("Error saving map: %1").arg(err));
        return false;
    }
    AppLog::write(QStringLiteral("Map"), QStringLiteral("Saved %1").arg(QDir::toNativeSeparators(path)));
    updateTitle();
    updateStatus();
    refreshSidebarMap();
    QMessageBox::information(this, tr("Success"), tr("Map saved successfully."));
    return true;
}

void MainWindow::onCloseMap()
{
    if (!m_doc->isLoaded())
        return;
    if (m_doc->isDirty()) {
        if (!confirmDiscardChanges())
            return;
    } else if (QMessageBox::question(this, tr("Close Map"), tr("Close the current map?"),
                                     QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    m_editor->onMapClosed();
    m_doc->reset();
    m_view->documentReplaced();
    m_miniMap->clearThumbnail();
    m_sidebar->clearMap();
    m_statusCoords->setText(QStringLiteral("0,0"));
    updateTileInfo();
    updateMapActions();
    updateUndoRedo();
    updateTitle();
    updateStatus();
    updateToolLabels();
}

void MainWindow::onNewMap()
{
    if (!confirmDiscardChanges())
        return;
    if (!ensureOp2Folder())
        return;
    NewMapDialog dlg([this](const NewMapDialog::Result &r) { return createNewMap(r); }, this);
    dlg.show();
    dlg.reportTemplateProblems();
    dlg.exec();
}

bool MainWindow::createNewMap(const NewMapDialog::Result &r)
{
    if (!confirmDiscardChanges())
        return false;
    const QString blank = BlankMaps::pathFor(r.width, r.height);
    if (blank.isEmpty()) {
        QMessageBox::critical(this, tr("Error"),
            tr("Could not find blank map template: blank_%1x%2.map").arg(r.width).arg(r.height));
        return false;
    }
    if (!loadMapFile(blank, false))
        return false;

    // Starting biome: random raw mapping indices, written directly (the
    // starting state, not an undo step). Bottom row Impassible1 always.
    int lo = -1, hi = -1;
    switch (r.biome) {
    case NewMapDialog::Terrain0: lo = 1; hi = 32; break;
    case NewMapDialog::Terrain1: lo = 1206; hi = 1221; break;
    case NewMapDialog::Terrain2: lo = 439; hi = 454; break;
    default: break;
    }
    const int W = m_doc->widthTiles(), H = m_doc->heightTiles();
    auto *rng = QRandomGenerator::global();
    if (lo >= 0 && hi < m_doc->mappingCount()) {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                m_doc->setMappingIndexAt(x, y, lo + static_cast<int>(rng->bounded(hi - lo + 1)));
    } else if (lo >= 0) {
        QMessageBox::critical(this, tr("Error"),
            tr("Error initializing map terrain: %1").arg(tr("the template's tile mapping table is too small.")));
    }
    for (int x = 0; x < W; ++x)
        m_doc->setCellTypeAt(x, H - 1, 6 /* Impassible1 */);
    m_doc->flushChanges();

    QString working = Settings::workingPath();
    if (working.isEmpty())
        working = Settings::op2Path();
    m_doc->setCurrentPath(QDir(working).filePath(r.fileName));
    m_doc->markNewUnsaved();
    m_miniMap->scheduleRefresh();
    refreshSidebarMap();
    updateTitle();
    updateStatus();
    activateWindow();
    return true;
}

void MainWindow::rebuildRecentMenu()
{
    m_recentMenu->clear();
    const QStringList list = Settings::recentFiles();
    if (list.isEmpty()) {
        m_recentMenu->addAction(tr("(empty)"))->setEnabled(false);
        return;
    }
    for (int i = 0; i < list.size(); ++i) {
        const QString p = list[i];
        QAction *a = m_recentMenu->addAction(QStringLiteral("%1. %2").arg(i + 1).arg(QFileInfo(p).fileName()));
        a->setToolTip(p);
        connect(a, &QAction::triggered, this, [this, p] {
            if (!confirmDiscardChanges())
                return;
            if (!QFileInfo::exists(p)) {
                QMessageBox::warning(this, tr("Recent File Missing"), tr("File no longer exists:\n%1").arg(p));
                Settings::removeRecentFile(p);
                rebuildRecentMenu();
                return;
            }
            loadMapFile(p, true);
        });
    }
    m_recentMenu->setToolTipsVisible(true);
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
    m_tileGroups->refreshFromDisk();
}

void MainWindow::onExportJson()
{
    if (!m_doc->isLoaded())
        return;
    const QFileInfo cur(m_doc->currentPath());
    const QString def = QDir(exportFolder()).filePath(
        (cur.completeBaseName().isEmpty() ? QStringLiteral("Untitled") : cur.completeBaseName()) + QStringLiteral(".json"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Export Map to JSON"), def, tr("JSON (*.json);;All Files (*)"));
    if (out.isEmpty())
        return;
    QString err;
    if (!MapJsonIO::exportToJson(*m_doc, out, cur.fileName().isEmpty() ? QStringLiteral("Untitled.map") : cur.fileName(),
                                 QFileInfo(out).completeBaseName(), QString(), QString(), err)) {
        QMessageBox::critical(this, tr("Export to JSON"), tr("Failed to export:\n%1").arg(err));
        return;
    }
    QMessageBox::information(this, tr("Export to JSON"), tr("Map exported to:\n%1").arg(QDir::toNativeSeparators(out)));
}

void MainWindow::onImportJson()
{
    if (!confirmDiscardChanges() || !ensureOp2Folder())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Map from JSON"), Settings::workingPath(),
                                                      tr("JSON (*.json);;All Files (*)"));
    if (path.isEmpty())
        return;
    if (m_editor->editMode())
        m_editor->setEditMode(false);
    QString msg;
    MapLoadInfo info;
    if (!MapJsonIO::importFromJson(path, *m_doc, Settings::op2Path(), info, msg)) {
        QMessageBox::critical(this, tr("Import from JSON"), tr("Failed to import:\n%1").arg(msg));
        return;
    }
    m_editor->onMapLoaded();
    m_view->documentReplaced();
    showTilesetWarnings(info);
    const QString working = Settings::workingPath().isEmpty() ? Settings::op2Path() : Settings::workingPath();
    m_doc->setCurrentPath(QDir(working).filePath(QFileInfo(path).completeBaseName() + QStringLiteral(".map")));
    m_doc->markNewUnsaved();
    refreshSidebarMap();
    updateTileInfo();
    updateZoomLabel();
    updateMapActions();
    updateUndoRedo();
    updateTitle();
    updateStatus();
    if (!msg.isEmpty())
        QMessageBox::warning(this, tr("Import from JSON"), msg);
}

// ---- Images ------------------------------------------------------------------

QString MainWindow::exportFolder() const
{
    const QString cur = m_doc->currentPath();
    if (!cur.isEmpty() && QDir(QFileInfo(cur).absolutePath()).exists())
        return QFileInfo(cur).absolutePath();
    const QString working = Settings::workingPath();
    if (!working.isEmpty() && QDir(working).exists())
        return working;
    return QCoreApplication::applicationDirPath();
}

void MainWindow::exportImage(double scale, const QString &suffix, int gridMode)
{
    if (!m_doc->isLoaded())
        return;
    const QString base = QFileInfo(m_doc->currentPath()).completeBaseName().isEmpty()
        ? QStringLiteral("Untitled") : QFileInfo(m_doc->currentPath()).completeBaseName();
    const QString name = base + QLatin1Char('_') + MapRenderer::modeName(m_editor->renderMode())
                       + (suffix.isEmpty() ? QString() : QLatin1Char('_') + suffix) + QStringLiteral(".jpg");
    const QString path = QDir(exportFolder()).filePath(name);
    const int tilePx = std::max(1, static_cast<int>(std::lround(32 * scale)));
    const qint64 w = qint64(m_doc->widthTiles()) * tilePx;
    const qint64 h = qint64(m_doc->heightTiles()) * tilePx;
    const QLocale loc;
    if (w * h > 16384LL * 16384LL) {
        QMessageBox::critical(this, tr("Export Error"), tr("Error exporting image: %1").arg(
            tr("The image would be %1 x %2 pixels, which is too large to create. Use a smaller export size (e.g. 12.5%) for this map.")
                .arg(loc.toString(w), loc.toString(h))));
        return;
    }
    MapRenderer::Overlays o = m_editor->overlays();
    o.grid = gridMode == SmallGrid;
    o.largeGrid = gridMode == LargeGrid;
    o.coords = false;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage img = MapRenderer::renderFullMap(*m_doc, tilePx, m_editor->renderMode(), o);
    QApplication::restoreOverrideCursor();
    if (img.isNull()) {
        QMessageBox::critical(this, tr("Export Error"), tr("Error exporting image: %1").arg(
            tr("Not enough memory to create a %1 x %2 pixel image. Try a smaller export size.").arg(loc.toString(w), loc.toString(h))));
        return;
    }
    QString err;
    if (!saveJpegSafely(img, path, 95, err)) {
        QMessageBox::critical(this, tr("Export Error"), tr("Error exporting image: %1").arg(err));
        return;
    }
    const QString shown = QDir::toNativeSeparators(path);
    QMessageBox::information(this, tr("Export Complete"), scale == 1.0
        ? tr("Map image exported successfully to: %1").arg(shown)
        : tr("Map image exported successfully at %1% size to: %2").arg(qRound(scale * 100)).arg(shown));
}

void MainWindow::exportOverview()
{
    if (!m_doc->isLoaded()) {
        QMessageBox::information(this, tr("No Map Loaded"), tr("Please load a map first."));
        return;
    }
    QDialog progress(this, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    progress.setWindowTitle(tr("Generating Overview..."));
    progress.setFixedSize(300, 100);
    auto *label = new QLabel(tr("Creating overview image..."), &progress);
    label->setGeometry(0, 0, 300, 100);
    label->setAlignment(Qt::AlignCenter);
    progress.show();
    QCoreApplication::processEvents();

    const int tilePx = 4; // 12.5%
    QList<QImage> images;
    const char *names[] = { "GameMap", "CellTypeOverlay", "CellTypeOverlayAll", "CellTypeBasic", "CellTypeAll" };
    for (int mode = 0; mode < 5; ++mode) {
        label->setText(tr("Rendering %1...").arg(QString::fromLatin1(names[mode])));
        QCoreApplication::processEvents();
        images << MapRenderer::renderFullMap(*m_doc, tilePx, mode, MapRenderer::Overlays());
    }
    label->setText(tr("Creating overview image..."));
    QCoreApplication::processEvents();

    const int w = images[0].width(), h = images[0].height();
    constexpr int pad = 6;
    QImage out(3 * w + 4 * pad, 2 * h + 3 * pad, QImage::Format_RGB32);
    out.fill(Qt::white);
    QPainter p(&out);
    auto origin = [&](int col, int row) { return QPoint(pad + col * (w + pad), pad + row * (h + pad)); };
    const QPoint cells[5] = { origin(1, 0), origin(2, 0), origin(0, 1), origin(1, 1), origin(2, 1) };
    for (int i = 0; i < 5; ++i)
        p.drawImage(cells[i], images[i]);
    // Info panel
    const QPoint io = origin(0, 0);
    p.setClipRect(QRect(io, QSize(w, h)));
    p.fillRect(QRect(io, QSize(w, h)), QColor(240, 240, 240));
    p.setPen(QColor(169, 169, 169));
    p.drawRect(QRect(io, QSize(w - 1, h - 1)));
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setPen(Qt::black);
    QFont title(QStringLiteral("Arial"));
    title.setPixelSize(21);
    title.setBold(true);
    p.setFont(title);
    p.drawText(io + QPoint(10, 10 + p.fontMetrics().ascent()), tr("Map Overview"));
    QFont body(QStringLiteral("Arial"));
    body.setPixelSize(16);
    p.setFont(body);
    const int asc = p.fontMetrics().ascent();
    int y = 36;
    const QLocale loc;
    const QStringList info = {
        tr("Map: %1").arg(QFileInfo(m_doc->currentPath()).fileName()),
        tr("Size: %1 x %2").arg(m_doc->widthTiles()).arg(m_doc->heightTiles()),
        tr("Tiles: %1").arg(loc.toString(static_cast<qlonglong>(m_doc->map()->tiles.size()))),
    };
    for (const QString &s : info) {
        p.drawText(io + QPoint(10, y + asc), s);
        y += 20;
    }
    y += 16;
    p.drawText(io + QPoint(10, y + asc), tr("Render Modes:"));
    const QStringList modes = { tr("1: Game Map (top-middle)"), tr("2: CellType Overlay (top-right)"),
                                tr("3: CellType Overlay All (bottom-left)"), tr("4: CellType Basic (bottom-middle)"),
                                tr("5: CellType All (bottom-right)") };
    for (const QString &s : modes) {
        y += 20;
        p.drawText(io + QPoint(10, y + asc), s);
    }
    p.drawText(io + QPoint(10, h - 30 + asc),
               tr("Generated: %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))));
    p.end();

    const QString base = QFileInfo(m_doc->currentPath()).completeBaseName().isEmpty()
        ? QStringLiteral("Untitled") : QFileInfo(m_doc->currentPath()).completeBaseName();
    const QString path = QDir(exportFolder()).filePath(base + QStringLiteral("_overview.jpg"));
    QString err;
    const bool ok = saveJpegSafely(out, path, 95, err);
    progress.close();
    if (!ok) {
        QMessageBox::critical(this, tr("Export Error"), tr("Error exporting overview image: %1").arg(err));
        return;
    }
    QMessageBox::information(this, tr("Export Complete"),
                             tr("Overview image exported successfully to: %1").arg(QDir::toNativeSeparators(path)));
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::takeScreenshot()
{
    if (!m_doc->isLoaded() || m_view->width() <= 0 || m_view->height() <= 0)
        return;
    QString dir = Settings::workingPath();
    if (dir.isEmpty() || !QDir(dir).exists())
        dir = QCoreApplication::applicationDirPath();
    const QString base = QFileInfo(m_doc->currentPath()).completeBaseName().isEmpty()
        ? QStringLiteral("screenshot") : QFileInfo(m_doc->currentPath()).completeBaseName();
    const QString path = QDir(dir).filePath(base + QStringLiteral("_screenshot_")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")) + QStringLiteral(".jpg"));
    QImageWriter w(path, "jpg");
    w.setQuality(90);
    if (!w.write(m_view->grabView())) {
        QMessageBox::critical(this, tr("Screenshot Error"), tr("Error saving screenshot: %1").arg(w.errorString()));
        return;
    }
    QMessageBox::information(this, tr("Screenshot"), tr("Screenshot saved to:\n%1").arg(QDir::toNativeSeparators(path)));
}

void MainWindow::configureGrid()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Configure Grid Settings"));
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dlg.setFixedSize(300, 200);
    auto *form = new QFormLayout(&dlg);
    auto *large = new QSpinBox;
    large->setRange(8, 64);
    large->setSingleStep(8);
    large->setValue(m_editor->overlays().largeGridSize);
    auto *coords = new QSpinBox;
    coords->setRange(8, 64);
    coords->setSingleStep(8);
    coords->setValue(m_editor->overlays().coordSpacing);
    form->addRow(tr("Large Grid Size (in tiles):"), large);
    form->addRow(tr("Coordinate Spacing (in tiles):"), coords);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() == QDialog::Accepted)
        m_editor->setGridSizes(large->value(), coords->value());
}

// ---- Startup / shutdown ------------------------------------------------------

namespace {
struct ParsedArgs {
    QString op2Dir;
    QString mapFile;
};

ParsedArgs parseArgs(const QStringList &args)
{
    ParsedArgs out;
    for (int i = 0; i < args.size(); ++i) {
        const QString a = args[i];
        QString name, value;
        bool isOption = false;
        if (a.startsWith(QLatin1Char('/')) || a.startsWith(QLatin1Char('-'))) {
            QString body = a;
            while (body.startsWith(QLatin1Char('/')) || body.startsWith(QLatin1Char('-')))
                body.remove(0, 1);
            const int sep = body.indexOf(QRegularExpression(QStringLiteral("[:=]")));
            name = sep >= 0 ? body.left(sep) : body;
            if (name.compare(QLatin1String("op2dir"), Qt::CaseInsensitive) == 0) {
                isOption = true;
                if (sep >= 0)
                    value = body.mid(sep + 1);
                else if (i + 1 < args.size())
                    value = args[++i];
            }
        }
        if (isOption) {
            value = value.trimmed();
            while (value.startsWith(QLatin1Char('"')))
                value.remove(0, 1);
            while (value.endsWith(QLatin1Char('"')))
                value.chop(1);
            while (value.endsWith(QLatin1Char('\\')) || value.endsWith(QLatin1Char('/')))
                value.chop(1);
            if (!value.isEmpty())
                out.op2Dir = value;
            continue;
        }
        if (out.mapFile.isEmpty())
            out.mapFile = a;
    }
    return out;
}
} // namespace

bool MainWindow::startup(const QStringList &args)
{
    const ParsedArgs pa = parseArgs(args);
    if (!pa.op2Dir.isEmpty()) {
        if (QDir(pa.op2Dir).exists())
            Settings::setOp2Path(pa.op2Dir);
        else
            QMessageBox::warning(this, tr("Outpost 2 Folder Not Found"),
                tr("The Outpost 2 folder given on the command line doesn't exist:\n\n%1").arg(pa.op2Dir));
    }
    if (Settings::workingPath().isEmpty() && !Settings::op2Path().isEmpty())
        Settings::setWorkingPath(Settings::op2Path());
    if (Settings::tileGroupsPath().isEmpty())
        Settings::setTileGroupsPath(Settings::defaultTileGroupsPath());
    QDir().mkpath(Settings::tileGroupsPath());

    const QString op2 = Settings::op2Path();
    if (op2.isEmpty() || !QDir(op2).exists()) {
        QMessageBox::information(nullptr, tr("Welcome"),
            tr("Welcome. First step is to browse for the Outpost 2 (1.4.1) folder. Click the browse button to locate the folder."));
        SettingsDialog dlg(nullptr);
        dlg.exec();
        const QString again = Settings::op2Path();
        if (again.isEmpty() || !QDir(again).exists()) {
            QMessageBox::warning(nullptr, tr("No Outpost 2 Folder"),
                tr("No Outpost 2 folder is set, so OP2Mapper will now close.\n\nRun it again and choose the "
                   "Outpost 2 (1.4.1) folder in Settings, or start it with /op2dir \"<folder>\"."));
            return false;
        }
        if (Settings::workingPath().isEmpty())
            Settings::setWorkingPath(again);
    }
    m_tileGroups->refreshFromDisk();
    show();
    if (!pa.mapFile.isEmpty() && QFileInfo::exists(pa.mapFile))
        loadMapFile(pa.mapFile, true);
    return true;
}

void MainWindow::handleSecondInstance(const QStringList &args)
{
    AppLog::write(QStringLiteral("Start"),
                  QStringLiteral("Second launch handed to this copy, arguments: %1")
                      .arg(args.isEmpty() ? QStringLiteral("(none)") : args.join(QLatin1Char(' '))));
    if (isMinimized())
        showNormal();
    raise();
    activateWindow();
    const ParsedArgs pa = parseArgs(args);
    if (pa.mapFile.isEmpty())
        return;
    if (!QFileInfo::exists(pa.mapFile)) {
        QMessageBox::warning(this, tr("Open Map"), tr("Map file not found:\n%1").arg(pa.mapFile));
        return;
    }
    if (!confirmDiscardChanges())
        return;
    loadMapFile(pa.mapFile, true);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmDiscardChanges()) {
        event->ignore();
        return;
    }
    AppLog::write(QStringLiteral("Stop"), QStringLiteral("Main window closing"));
    saveMainPosition();
    for (ToolPalette *p : palettes())
        p->savePosition();
    event->accept();
    QApplication::quit();
}

void MainWindow::restoreMainPosition()
{
    if (!Settings::hasWindowValue(QStringLiteral("MainLeft")) || !Settings::hasWindowValue(QStringLiteral("MainTop")))
        return;
    const int x = Settings::windowInt(QStringLiteral("MainLeft"), 0);
    const int y = Settings::windowInt(QStringLiteral("MainTop"), 0);
    int w = Settings::windowInt(QStringLiteral("MainWidth"), 0);
    int h = Settings::windowInt(QStringLiteral("MainHeight"), 0);
    if (w <= 0)
        w = frameGeometry().width();
    if (h <= 0)
        h = frameGeometry().height();
    const QRect r(x, y, w, h);
    bool visible = false;
    for (QScreen *s : QGuiApplication::screens()) {
        const QRect i = s->availableGeometry().intersected(r);
        if (i.width() >= 200 && i.height() >= 150)
            visible = true;
    }
    if (!visible)
        return;
    move(x, y);
    const QMargins frame(0, 0, 0, 0);
    resize(std::max(400, w - (frameGeometry().width() - width())), std::max(300, h - (frameGeometry().height() - height())));
    if (Settings::windowInt(QStringLiteral("MainWindowState"), 0) == 2)
        setWindowState(Qt::WindowMaximized);
    m_positionRestored = true;
}

void MainWindow::saveMainPosition()
{
    if (!isVisible() || isMinimized())
        return;
    QRect r = isMaximized() ? normalGeometry() : frameGeometry();
    if (isMaximized()) {
        const QMargins m(geometry().left() - frameGeometry().left(), geometry().top() - frameGeometry().top(),
                         frameGeometry().right() - geometry().right(), frameGeometry().bottom() - geometry().bottom());
        r = r.marginsAdded(m);
    }
    Settings::setWindowInt(QStringLiteral("MainLeft"), r.x());
    Settings::setWindowInt(QStringLiteral("MainTop"), r.y());
    Settings::setWindowInt(QStringLiteral("MainWidth"), r.width());
    Settings::setWindowInt(QStringLiteral("MainHeight"), r.height());
    Settings::setWindowInt(QStringLiteral("MainWindowState"), isMaximized() ? 2 : 0);
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    if (isVisible())
        m_positionTimer.start();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (isVisible())
        m_positionTimer.start();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isVisible() && !isMinimized())
        m_positionTimer.start();
}
