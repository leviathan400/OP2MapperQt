#include "MainWindow.h"

#include "BlankMaps.h"
#include "CellTypeInfo.h"
#include "CellTypesWindow.h"
#include "MapDocument.h"
#include "MapJsonIO.h"
#include "MapView.h"
#include "MiniMap.h"
#include "NewMapDialog.h"
#include "Settings.h"
#include "TileClipboard.h"
#include "TileGroupIO.h"
#include "TileGroupsWindow.h"
#include "TileSelectWindow.h"
#include "TilesetWindow.h"

#include <OP2Utility.h>

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QUndoStack>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_doc(std::make_unique<MapDocument>())
    , m_clipboard(std::make_unique<TileClipboard>())
{
    setWindowTitle(tr("OP2 Mapper"));
    resize(1200, 800);

    m_mapView = new MapView(this);
    setCentralWidget(m_mapView);

    auto *newAction = new QAction(tr("&New Map..."), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::onNewMap);

    auto *openAction = new QAction(tr("&Open Map..."), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenMap);

    m_closeAction = new QAction(tr("&Close Map"), this);
    m_closeAction->setShortcut(QKeySequence::Close);
    m_closeAction->setEnabled(false);
    connect(m_closeAction, &QAction::triggered, this, &MainWindow::onCloseMap);

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setEnabled(false);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveMap);

    m_saveAsAction = new QAction(tr("Save &As..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setEnabled(false);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveMapAs);

    auto *folderAction = new QAction(tr("Set OP2 &Game Folder..."), this);
    connect(folderAction, &QAction::triggered, this, &MainWindow::onSetOp2Folder);

    auto *settingsAction = new QAction(tr("S&ettings..."), this);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onShowSettings);

    auto *aboutAction = new QAction(tr("&About OP2 Mapper..."), this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onShowAbout);

    auto *launchAction = new QAction(tr("&Launch Outpost 2"), this);
    connect(launchAction, &QAction::triggered, this, &MainWindow::onLaunchOp2);

    auto *openFolderAction = new QAction(tr("Open OP2 &Folder"), this);
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::onOpenOp2Folder);

    auto *importJsonAction = new QAction(tr("&Import from JSON..."), this);
    connect(importJsonAction, &QAction::triggered, this, &MainWindow::onImportFromJson);

    auto *exportImageAction = new QAction(tr("Export &Image..."), this);
    connect(exportImageAction, &QAction::triggered, this, &MainWindow::onExportImage);

    auto *exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    m_exportJsonAction = new QAction(tr("Export to &JSON..."), this);
    m_exportJsonAction->setEnabled(false);
    connect(m_exportJsonAction, &QAction::triggered, this, &MainWindow::onExportToJson);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    m_recentFilesMenu = fileMenu->addMenu(tr("Open &Recent"));
    rebuildRecentFilesMenu();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addAction(m_closeAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exportJsonAction);
    fileMenu->addAction(importJsonAction);
    fileMenu->addAction(exportImageAction);
    fileMenu->addSeparator();
    fileMenu->addAction(folderAction);
    fileMenu->addAction(settingsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    m_gridAction = new QAction(tr("Show &Grid"), this);
    m_gridAction->setCheckable(true);
    m_gridAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    m_gridAction->setEnabled(false);
    connect(m_gridAction, &QAction::toggled, m_mapView, &MapView::setShowGrid);

    m_largeGridAction = new QAction(tr("Show &Large Grid"), this);
    m_largeGridAction->setCheckable(true);
    m_largeGridAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    m_largeGridAction->setEnabled(false);
    connect(m_largeGridAction, &QAction::toggled, m_mapView, &MapView::setShowLargeGrid);

    m_coordsAction = new QAction(tr("Show Coor&dinates"), this);
    m_coordsAction->setCheckable(true);
    m_coordsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    m_coordsAction->setEnabled(false);
    connect(m_coordsAction, &QAction::toggled, m_mapView, &MapView::setShowCoords);

    m_resetZoomAction = new QAction(tr("&Reset Zoom"), this);
    m_resetZoomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    m_resetZoomAction->setEnabled(false);
    connect(m_resetZoomAction, &QAction::triggered, m_mapView, &MapView::resetZoom);

    // Render mode submenu — exclusive choice across 5 visual modes.
    auto *renderGroup = new QActionGroup(this);
    renderGroup->setExclusive(true);
    auto makeRenderModeAction = [this, renderGroup](const QString &text, int mode,
                                                    const QKeySequence &shortcut) {
        auto *a = new QAction(text, this);
        a->setCheckable(true);
        a->setShortcut(shortcut);
        a->setData(mode);
        renderGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode]() {
            m_mapView->setRenderMode(mode);
        });
        return a;
    };
    auto *modeGameMap = makeRenderModeAction(tr("Render: &Game Map"), 0,
        QKeySequence(Qt::CTRL | Qt::Key_1));
    auto *modeOverlay = makeRenderModeAction(tr("Render: Cell Types &Overlay"), 1,
        QKeySequence(Qt::CTRL | Qt::Key_2));
    auto *modeOverlayAll = makeRenderModeAction(tr("Render: Cell Types Overlay (&All)"), 2,
        QKeySequence(Qt::CTRL | Qt::Key_3));
    auto *modeBasic = makeRenderModeAction(tr("Render: Cell Types &Basic"), 3,
        QKeySequence(Qt::CTRL | Qt::Key_4));
    auto *modeAll = makeRenderModeAction(tr("Render: Cell Types A&ll Labels"), 4,
        QKeySequence(Qt::CTRL | Qt::Key_5));
    modeGameMap->setChecked(true);

    // Edit menu — Undo/Redo. The stack lives on m_doc and persists for the
    // app's lifetime (reset() on map-close clears it but doesn't destroy it),
    // so these actions stay live.
    auto *undoAction = m_doc->undoStack()->createUndoAction(this, tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    auto *redoAction = m_doc->undoStack()->createRedoAction(this, tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    m_copyAction = new QAction(tr("&Copy Tiles"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_copyAction->setEnabled(false);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::onCopyTiles);

    m_pasteAction = new QAction(tr("&Paste Tiles"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_pasteAction->setEnabled(false);
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::onPasteTiles);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    auto *renderMenu = viewMenu->addMenu(tr("Render &Mode"));
    renderMenu->addAction(modeGameMap);
    renderMenu->addAction(modeOverlay);
    renderMenu->addAction(modeOverlayAll);
    renderMenu->addAction(modeBasic);
    renderMenu->addAction(modeAll);
    viewMenu->addSeparator();
    viewMenu->addAction(m_gridAction);
    viewMenu->addAction(m_largeGridAction);
    viewMenu->addAction(m_coordsAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_resetZoomAction);

    auto *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(launchAction);
    toolsMenu->addAction(openFolderAction);

    // Help stays last — standard menu-bar convention.
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction);

    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setObjectName("MainToolBar");
    toolbar->addAction(newAction);
    toolbar->addAction(openAction);
    toolbar->addSeparator();
    toolbar->addAction(m_gridAction);
    toolbar->addAction(m_largeGridAction);
    toolbar->addAction(m_coordsAction);

    m_statusPath = new QLabel(tr("No map loaded"));
    statusBar()->addWidget(m_statusPath, 1);
    m_statusCoords = new QLabel;
    m_statusCoords->setMinimumWidth(120);
    statusBar()->addPermanentWidget(m_statusCoords);
    m_statusZoom = new QLabel;
    m_statusZoom->setMinimumWidth(70);
    statusBar()->addPermanentWidget(m_statusZoom);

    connect(m_mapView, &MapView::tileHovered, this, &MainWindow::onTileHovered);
    connect(m_mapView, &MapView::zoomChanged, this, &MainWindow::onZoomChanged);

    m_miniMap = new MiniMap(this);
    connect(m_mapView, &MapView::viewportChanged, this, [this] {
        m_miniMap->setViewportRect(m_mapView->visibleSceneRect());
    });
    connect(m_miniMap, &MiniMap::centerRequested,
            m_mapView, &MapView::centerOnScene);

    m_tilesetWindow = new TilesetWindow(this);
    connect(m_tilesetWindow, &TilesetWindow::tileSelected,
            m_mapView, &MapView::setSelectedTile);

    m_cellTypesWindow = new CellTypesWindow(this);
    connect(m_cellTypesWindow, &CellTypesWindow::cellTypeSelected,
            m_mapView, &MapView::setSelectedCellType);

    m_tileSelectWindow = new TileSelectWindow(this);
    m_tileSelectWindow->setClipboard(m_clipboard.get());

    m_tileGroupsWindow = new TileGroupsWindow(this);
    connect(m_tileGroupsWindow, &TileGroupsWindow::placeRequested, this,
            [this](const QString &jsonPath) {
                if (!m_doc || !m_doc->isLoaded())
                    return;
                QString err;
                if (!TileGroupIO::loadIntoClipboard(jsonPath, *m_doc,
                                                    *m_clipboard, err)) {
                    return;
                }
                m_tileSelectWindow->refresh();
                m_pasteAction->setEnabled(true);
                m_mapView->enterPasteMode(m_clipboard.get());
            });
    connect(m_tileSelectWindow, &TileSelectWindow::tileGroupSaved, this,
            [this](const QString &) {
                m_tileGroupsWindow->refresh(QStringLiteral("Exported"));
            });

    // Mutual exclusion between the two paint-tool palettes — when one fires,
    // wipe the other's visual selection so the user sees a single active tool.
    connect(m_tilesetWindow, &TilesetWindow::tileSelected,
            m_cellTypesWindow, [this](int t, int i) {
                if (t >= 0 && i >= 0)
                    m_cellTypesWindow->clearSelection();
            });
    connect(m_mapView, &MapView::selectionCleared,
            m_cellTypesWindow, &CellTypesWindow::clearSelection);

    // Window-title dirty marker updates whenever the undo stack toggles clean.
    connect(m_doc.get(), &MapDocument::dirtyChanged,
            this, [this](bool) { updateTitle(); });

    // Copy availability tracks the selection.
    connect(m_mapView, &MapView::selectionChanged,
            this, &MainWindow::onSelectionChanged);

    // Right-click on an empty view → contextual View menu.
    connect(m_mapView, &MapView::contextMenuRequested,
            this, [this](QPoint globalPos) {
                if (!m_doc || !m_doc->isLoaded())
                    return;
                QMenu menu(this);
                menu.addAction(m_gridAction);
                menu.addAction(m_largeGridAction);
                menu.addAction(m_coordsAction);
                menu.addSeparator();
                menu.addAction(m_resetZoomAction);
                menu.exec(globalPos);
            });

    // Restore window state from previous session.
    {
        QSettings s;
        const QByteArray geom = s.value(QStringLiteral("MainWindow/geometry"))
                                    .toByteArray();
        const QByteArray state = s.value(QStringLiteral("MainWindow/state"))
                                    .toByteArray();
        if (!geom.isEmpty())
            QWidget::restoreGeometry(geom);
        if (!state.isEmpty())
            QMainWindow::restoreState(state);
    }

    setAcceptDrops(true);
}

void MainWindow::rebuildRecentFilesMenu()
{
    if (!m_recentFilesMenu)
        return;
    m_recentFilesMenu->clear();
    const QStringList paths = Settings::recentFiles();
    if (paths.isEmpty()) {
        auto *a = m_recentFilesMenu->addAction(tr("(none)"));
        a->setEnabled(false);
        return;
    }
    for (const QString &p : paths) {
        auto *a = m_recentFilesMenu->addAction(QFileInfo(p).fileName());
        a->setToolTip(p);
        connect(a, &QAction::triggered, this, [this, p]() {
            if (!QFile::exists(p)) {
                Settings::removeRecentFile(p);
                rebuildRecentFilesMenu();
                QMessageBox::warning(this, tr("Open Recent"),
                    tr("File no longer exists:\n%1").arg(p));
                return;
            }
            if (!confirmDiscardChanges())
                return;
            loadMapFromPath(p, /*markUntitled=*/false);
        });
    }
    m_recentFilesMenu->addSeparator();
    auto *clear = m_recentFilesMenu->addAction(tr("Clear List"));
    connect(clear, &QAction::triggered, this, [this]() {
        QSettings().remove(QStringLiteral("recentFiles"));
        rebuildRecentFilesMenu();
    });
}

MainWindow::~MainWindow() = default;

QString MainWindow::ensureOp2Folder()
{
    QString folder = Settings::op2Folder();
    if (!folder.isEmpty() && QDir(folder).exists())
        return folder;

    QMessageBox::information(this, tr("OP2 Game Folder"),
        tr("Select your Outpost 2 installation folder (containing the .vol archives "
           "and/or loose well00XX.bmp tileset files)."));
    folder = QFileDialog::getExistingDirectory(this, tr("Select OP2 Game Folder"));
    if (!folder.isEmpty())
        Settings::setOp2Folder(folder);
    return folder;
}

void MainWindow::onSetOp2Folder()
{
    const QString folder = QFileDialog::getExistingDirectory(
        this, tr("Select OP2 Game Folder"), Settings::op2Folder());
    if (!folder.isEmpty())
        Settings::setOp2Folder(folder);
}

void MainWindow::onOpenMap()
{
    if (!confirmDiscardChanges())
        return;

    const QString op2 = ensureOp2Folder();
    if (op2.isEmpty())
        return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Outpost 2 Map"),
        Settings::lastMapDir(),
        tr("OP2 Maps (*.map);;All Files (*)"));
    if (path.isEmpty())
        return;
    Settings::setLastMapDir(QFileInfo(path).absolutePath());

    loadMapFromPath(path, /*markUntitled=*/false);
}

void MainWindow::onNewMap()
{
    if (!confirmDiscardChanges())
        return;

    const QString op2 = ensureOp2Folder();
    if (op2.isEmpty())
        return;

    NewMapDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString blankPath = BlankMaps::pathFor(dlg.selectedWidth(),
                                                 dlg.selectedHeight());
    if (blankPath.isEmpty()) {
        QMessageBox::critical(this, tr("New Map"),
            tr("Failed to extract the blank-map template."));
        return;
    }
    loadMapFromPath(blankPath, /*markUntitled=*/true);
}

bool MainWindow::confirmDiscardChanges()
{
    if (!m_doc || !m_doc->isLoaded() || !m_doc->isDirty())
        return true;
    const auto choice = QMessageBox::warning(this, tr("Unsaved Changes"),
        tr("The current map has unsaved changes.\n\nSave before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Cancel)
        return false;
    if (choice == QMessageBox::Discard)
        return true;
    // Save chosen.
    if (m_doc->currentPath().isEmpty()) {
        onSaveMapAs();
    } else {
        onSaveMap();
    }
    return !m_doc->isDirty();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmDiscardChanges()) {
        event->ignore();
        return;
    }
    QSettings s;
    s.setValue(QStringLiteral("MainWindow/geometry"), QWidget::saveGeometry());
    s.setValue(QStringLiteral("MainWindow/state"), QMainWindow::saveState());
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;
    for (const QUrl &u : event->mimeData()->urls()) {
        if (u.isLocalFile()
            && u.toLocalFile().endsWith(QStringLiteral(".map"), Qt::CaseInsensitive)) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    for (const QUrl &u : event->mimeData()->urls()) {
        if (!u.isLocalFile())
            continue;
        const QString path = u.toLocalFile();
        if (!path.endsWith(QStringLiteral(".map"), Qt::CaseInsensitive))
            continue;
        if (!confirmDiscardChanges())
            return;
        loadMapFromPath(path);
        Settings::setLastMapDir(QFileInfo(path).absolutePath());
        return;
    }
}

void MainWindow::onExportImage()
{
    if (!m_doc || !m_doc->isLoaded()) {
        QMessageBox::information(this, tr("Export Image"),
            tr("Open a map first."));
        return;
    }

    QStringList scales{
        tr("Full size (100%)"),
        tr("Half (50%)"),
        tr("Quarter (25%)"),
        tr("1/8 (12.5%)"),
        tr("1/16 (6.25%)"),
    };
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, tr("Export Image"),
        tr("Scale:"), scales, 0, false, &ok);
    if (!ok)
        return;
    const double factor = (choice == scales[0]) ? 1.0
                        : (choice == scales[1]) ? 0.5
                        : (choice == scales[2]) ? 0.25
                        : (choice == scales[3]) ? 0.125
                        : 0.0625;

    const QString defaultName = (m_doc->currentPath().isEmpty()
            ? QStringLiteral("Untitled")
            : QFileInfo(m_doc->currentPath()).completeBaseName())
        + QStringLiteral(".jpg");
    const QString defaultDir = m_doc->currentPath().isEmpty()
        ? Settings::lastMapDir()
        : QFileInfo(m_doc->currentPath()).absolutePath();
    const QString defaultPath = defaultDir.isEmpty()
        ? defaultName : defaultDir + QLatin1Char('/') + defaultName;

    const QString outPath = QFileDialog::getSaveFileName(this,
        tr("Export Image"), defaultPath,
        tr("JPEG (*.jpg);;BMP (*.bmp);;PNG (*.png)"));
    if (outPath.isEmpty())
        return;

    auto *scene = m_mapView->scene();
    const QRectF src = scene->sceneRect();
    const QSize outSize(static_cast<int>(src.width() * factor),
                        static_cast<int>(src.height() * factor));
    QImage img(outSize, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    scene->render(&p, QRectF(QPointF(0, 0), QSizeF(outSize)), src);
    p.end();

    if (!img.save(outPath)) {
        QMessageBox::critical(this, tr("Export Image"),
            tr("Failed to write file:\n%1").arg(outPath));
        return;
    }
    QMessageBox::information(this, tr("Export Image"),
        tr("Image saved:\n%1").arg(outPath));
}

void MainWindow::onShowSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Settings"));
    auto *layout = new QFormLayout(&dlg);

    auto makePathRow = [&](const QString &initial, std::function<void(const QString&)> onChange) {
        auto *row = new QWidget;
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        auto *edit = new QLineEdit(initial);
        auto *browse = new QPushButton(tr("Browse..."));
        auto *openInExplorer = new QPushButton(tr("Open"));
        h->addWidget(edit, 1);
        h->addWidget(browse);
        h->addWidget(openInExplorer);
        QObject::connect(browse, &QPushButton::clicked, &dlg, [edit, &dlg]() {
            const QString p = QFileDialog::getExistingDirectory(&dlg,
                tr("Select Folder"), edit->text());
            if (!p.isEmpty())
                edit->setText(p);
        });
        QObject::connect(openInExplorer, &QPushButton::clicked, edit, [edit]() {
            const QString p = edit->text();
            if (!p.isEmpty() && QDir(p).exists())
                QDesktopServices::openUrl(QUrl::fromLocalFile(p));
        });
        QObject::connect(&dlg, &QDialog::accepted, edit, [edit, onChange]() {
            onChange(edit->text());
        });
        return row;
    };

    layout->addRow(tr("OP2 game folder:"),
        makePathRow(Settings::op2Folder(), [](const QString &p) {
            Settings::setOp2Folder(p);
        }));
    layout->addRow(tr("Working folder:"),
        makePathRow(Settings::workingPath(), [](const QString &p) {
            Settings::setWorkingPath(p);
        }));
    layout->addRow(tr("Tilegroups folder:"),
        makePathRow(Settings::tileGroupsPath(), [this](const QString &p) {
            Settings::setTileGroupsPath(p);
            m_tileGroupsWindow->refresh();
        }));

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    dlg.resize(600, 200);
    dlg.exec();
}

void MainWindow::onShowAbout()
{
    QMessageBox::about(this, tr("About OP2 Mapper"),
        tr("<h3>OP2 Mapper (Qt port)</h3>"
           "<p>A modern Outpost 2: Divided Destiny map editor.</p>"
           "<p>Qt %1 — built with mingw-w64.</p>"
           "<p>OP2 file I/O via the vendored OP2Utility C++ library.</p>"
           "<p>JSON formats compatible with OP2Mapper3 and OP2MapJsonTools.</p>")
            .arg(QString::fromLatin1(qVersion())));
}

void MainWindow::onLaunchOp2()
{
    const QString op2 = Settings::op2Folder();
    if (op2.isEmpty() || !QDir(op2).exists()) {
        QMessageBox::warning(this, tr("Launch Outpost 2"),
            tr("Set the OP2 folder first (File → Set OP2 Game Folder)."));
        return;
    }
    const QString exe = QDir(op2).filePath(QStringLiteral("OPU/OPULauncher.exe"));
    if (!QFile::exists(exe)) {
        QMessageBox::warning(this, tr("Launch Outpost 2"),
            tr("OPULauncher.exe not found at:\n%1").arg(exe));
        return;
    }
    QProcess::startDetached(exe, QStringList(), QFileInfo(exe).absolutePath());
}

void MainWindow::onOpenOp2Folder()
{
    const QString op2 = Settings::op2Folder();
    if (op2.isEmpty() || !QDir(op2).exists()) {
        QMessageBox::warning(this, tr("Open OP2 Folder"),
            tr("Set the OP2 folder first (File → Set OP2 Game Folder)."));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(op2));
}

bool MainWindow::loadMapFromPath(const QString &path, bool markUntitled)
{
    const QString op2 = ensureOp2Folder();
    if (op2.isEmpty())
        return false;

    QString err;
    QStringList missing;
    if (!m_doc->load(path, op2, err, missing)) {
        QMessageBox::critical(this, tr("Open Map"),
            tr("Failed to load map:\n%1").arg(err));
        return false;
    }

    if (markUntitled) {
        // Blank-template loads (File → New) must not remember the template's
        // path — clearing it makes the first Save prompt for a location.
        m_doc->setCurrentPath(QString());
    } else {
        Settings::addRecentFile(path);
        rebuildRecentFilesMenu();
    }

    QString status = markUntitled
        ? tr("Untitled.map  —  %1 × %2 tiles")
              .arg(m_doc->widthTiles()).arg(m_doc->heightTiles())
        : tr("%1  —  %2 × %3 tiles")
              .arg(path).arg(m_doc->widthTiles()).arg(m_doc->heightTiles());
    if (!missing.isEmpty())
        status += tr("  —  missing tilesets: %1").arg(missing.join(", "));

    finishMapLoadUi(status);
    return true;
}

// Single post-load UI path shared by every way a map can arrive (File → Open,
// File → New, Open Recent, drag-drop, Import from JSON). Keeping this in one
// place is load-bearing: the import path used to hand-copy this block and
// silently drifted as features were added (palettes not shown, tile groups
// not refreshed, copy never enabled).
void MainWindow::finishMapLoadUi(const QString &statusText)
{
    m_mapView->setDocument(m_doc.get());

    m_statusPath->setText(statusText);
    updateTitle();
    m_closeAction->setEnabled(true);
    m_saveAction->setEnabled(true);
    m_saveAsAction->setEnabled(true);
    m_gridAction->setEnabled(true);
    m_largeGridAction->setEnabled(true);
    m_coordsAction->setEnabled(true);
    m_resetZoomAction->setEnabled(true);

    m_miniMap->setDocument(m_doc.get());
    m_miniMap->setViewportRect(m_mapView->visibleSceneRect());

    m_tilesetWindow->setDocument(m_doc.get());
    m_tileSelectWindow->setDocument(m_doc.get());
    m_tileGroupsWindow->refresh();

    // First-time defaults — only positioned if QSettings doesn't have a saved
    // geometry already (geometry restored in each palette's ctor). Lay them
    // out in 3 columns to the right of main, clamped to the primary screen
    // so nothing lands off-screen.
    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    auto clamp = [&](QWidget *w, QPoint p) {
        const int cx = std::clamp(p.x(),
            screen.left(), screen.right() - w->width());
        const int cy = std::clamp(p.y(),
            screen.top(), screen.bottom() - w->height());
        w->move(cx, cy);
    };

    const int col1X = x() + width() + 10;
    const int col2X = col1X + 240;
    const int col3X = col2X + 240;
    const int topY = y();

    if (!m_miniMap->isVisible()) {
        clamp(m_miniMap, { col1X, topY });
        m_miniMap->show();
    }
    if (!m_tilesetWindow->isVisible()) {
        clamp(m_tilesetWindow,
              { col1X, m_miniMap->y() + m_miniMap->height() + 20 });
        m_tilesetWindow->show();
    }
    if (!m_cellTypesWindow->isVisible()) {
        clamp(m_cellTypesWindow, { col2X, topY });
        m_cellTypesWindow->show();
    }
    if (!m_tileSelectWindow->isVisible()) {
        clamp(m_tileSelectWindow,
              { col2X, m_cellTypesWindow->y() + m_cellTypesWindow->height() + 20 });
        m_tileSelectWindow->show();
    }
    if (!m_tileGroupsWindow->isVisible()) {
        clamp(m_tileGroupsWindow, { col3X, topY });
        m_tileGroupsWindow->show();
    }

    m_pasteAction->setEnabled(!m_clipboard->isEmpty());
    m_copyAction->setEnabled(true);
    m_exportJsonAction->setEnabled(true);
}

void MainWindow::onCloseMap()
{
    if (!confirmDiscardChanges())
        return;
    m_doc->reset();
    m_mapView->setDocument(nullptr);
    m_statusPath->setText(tr("No map loaded"));
    m_statusCoords->clear();
    setWindowTitle(tr("OP2 Mapper"));
    m_closeAction->setEnabled(false);
    m_saveAction->setEnabled(false);
    m_saveAsAction->setEnabled(false);
    m_gridAction->setChecked(false);
    m_gridAction->setEnabled(false);
    m_largeGridAction->setChecked(false);
    m_largeGridAction->setEnabled(false);
    m_coordsAction->setChecked(false);
    m_coordsAction->setEnabled(false);
    m_resetZoomAction->setEnabled(false);
    m_statusZoom->clear();
    m_miniMap->setDocument(nullptr);
    m_tilesetWindow->setDocument(nullptr);
    m_cellTypesWindow->clearSelection();
    m_tileSelectWindow->setDocument(nullptr);
    m_clipboard->clear();
    m_tileSelectWindow->refresh();

    // Close all tool palettes — they're useless without a map, and reopen
    // alongside the next File → Open / File → New.
    m_miniMap->close();
    m_tilesetWindow->close();
    m_cellTypesWindow->close();
    m_tileSelectWindow->close();
    m_tileGroupsWindow->close();

    m_copyAction->setEnabled(false);
    m_pasteAction->setEnabled(false);
    m_exportJsonAction->setEnabled(false);
}

void MainWindow::onImportFromJson()
{
    if (!confirmDiscardChanges())
        return;
    const QString op2 = ensureOp2Folder();
    if (op2.isEmpty())
        return;

    const QString path = QFileDialog::getOpenFileName(this,
        tr("Import Map from JSON"), Settings::lastMapDir(),
        tr("JSON (*.json);;All Files (*)"));
    if (path.isEmpty())
        return;

    QStringList missing;
    QString err;
    if (!MapJsonIO::importFromJson(path, *m_doc, op2, missing, err)) {
        QMessageBox::critical(this, tr("Import from JSON"),
            tr("Failed to import:\n%1").arg(err));
        return;
    }

    // Mark as a fresh untitled map so saving prompts for location.
    m_doc->setCurrentPath(QString());

    QString status = tr("Imported from %1  —  %2 × %3 tiles")
        .arg(path).arg(m_doc->widthTiles()).arg(m_doc->heightTiles());
    if (!missing.isEmpty())
        status += tr("  —  missing tilesets: %1").arg(missing.join(", "));

    finishMapLoadUi(status);
}

void MainWindow::onExportToJson()
{
    if (!m_doc || !m_doc->isLoaded())
        return;

    const QString defaultName = m_doc->currentPath().isEmpty()
        ? QStringLiteral("Untitled.json")
        : QFileInfo(m_doc->currentPath()).completeBaseName() + QStringLiteral(".json");
    const QString defaultDir = m_doc->currentPath().isEmpty()
        ? Settings::lastMapDir()
        : QFileInfo(m_doc->currentPath()).absolutePath();
    const QString defaultPath = defaultDir.isEmpty()
        ? defaultName
        : defaultDir + QLatin1Char('/') + defaultName;

    const QString outPath = QFileDialog::getSaveFileName(
        this, tr("Export Map to JSON"),
        defaultPath,
        tr("JSON (*.json);;All Files (*)"));
    if (outPath.isEmpty())
        return;

    const QString mapFile = m_doc->currentPath().isEmpty()
        ? QStringLiteral("Untitled.map")
        : QFileInfo(m_doc->currentPath()).fileName();
    const QString mapName = QFileInfo(outPath).completeBaseName();

    QString err;
    if (!MapJsonIO::exportToJson(*m_doc, outPath, mapFile, mapName,
                                 QString(), QString(), err)) {
        QMessageBox::critical(this, tr("Export to JSON"),
            tr("Failed to export:\n%1").arg(err));
        return;
    }
    QMessageBox::information(this, tr("Export to JSON"),
        tr("Map exported to:\n%1").arg(outPath));
}

void MainWindow::onCopyTiles()
{
    if (!m_doc || !m_doc->isLoaded())
        return;
    const QRect sel = m_mapView->selectionTileRect();
    if (sel.isEmpty()) {
        // No selection yet — start the two-click pick workflow. When the
        // user finishes, the selectionChanged signal triggers the actual copy.
        m_copyAfterSelection = true;
        m_mapView->enterCopyMode();
        m_statusPath->setText(
            tr("Copy: click start tile, then click end tile (right-click to cancel)"));
        return;
    }
    m_clipboard->captureFrom(*m_doc, sel.x(), sel.y(), sel.width(), sel.height());
    m_pasteAction->setEnabled(!m_clipboard->isEmpty());
    m_tileSelectWindow->refresh();
    m_statusPath->setText(tr("Copied %1 × %2 tiles")
                              .arg(sel.width()).arg(sel.height()));
    // The selection rectangle has served its purpose — wipe it so the dashed
    // box doesn't linger after copy.
    m_mapView->clearSelection();
}

void MainWindow::onPasteTiles()
{
    if (!m_doc || !m_doc->isLoaded() || m_clipboard->isEmpty())
        return;
    m_mapView->enterPasteMode(m_clipboard.get());
}

void MainWindow::onSelectionChanged(QRect tileRect)
{
    // Copy is always available once a map is loaded — picks via two-click flow
    // when no selection exists yet.
    m_copyAction->setEnabled(m_doc && m_doc->isLoaded());

    if (m_copyAfterSelection && !tileRect.isEmpty()) {
        m_copyAfterSelection = false;
        onCopyTiles(); // selection now exists; copies immediately
    }
}

bool MainWindow::saveToPath(const QString &path)
{
    QString err;
    if (!m_doc->save(path, err)) {
        QMessageBox::critical(this, tr("Save Map"),
            tr("Failed to save:\n%1").arg(err));
        return false;
    }
    updateTitle();
    QMessageBox::information(this, tr("Save Map"),
        tr("Map saved:\n%1").arg(path));
    return true;
}

void MainWindow::onSaveMap()
{
    if (!m_doc || !m_doc->isLoaded())
        return;
    const QString path = m_doc->currentPath();
    if (path.isEmpty()) {
        onSaveMapAs();
        return;
    }
    saveToPath(path);
}

void MainWindow::onSaveMapAs()
{
    if (!m_doc || !m_doc->isLoaded())
        return;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Outpost 2 Map"),
        m_doc->currentPath().isEmpty() ? Settings::lastMapDir() : m_doc->currentPath(),
        tr("OP2 Maps (*.map);;All Files (*)"));
    if (path.isEmpty())
        return;
    Settings::setLastMapDir(QFileInfo(path).absolutePath());
    saveToPath(path);
}

void MainWindow::updateTitle()
{
    if (!m_doc || !m_doc->isLoaded()) {
        setWindowTitle(tr("OP2 Mapper"));
        return;
    }
    const QString name = QFileInfo(m_doc->currentPath()).fileName();
    setWindowTitle(tr("OP2 Mapper — %1%2")
                       .arg(name.isEmpty() ? tr("Untitled.map") : name)
                       .arg(m_doc->isDirty() ? QStringLiteral(" *") : QString()));
}

void MainWindow::onZoomChanged(double factor)
{
    m_statusZoom->setText(tr("Zoom: %1%").arg(qRound(factor * 100)));
}

void MainWindow::onTileHovered(int x, int y)
{
    if (x == 0 && y == 0) {
        m_statusCoords->clear();
        return;
    }
    QString text = tr("(%1, %2)").arg(x).arg(y);
    if (m_doc && m_doc->isLoaded()) {
        const auto ct = m_doc->cellTypeAt(x - 1, y - 1);
        text += QStringLiteral("   %1").arg(CellTypeInfo::name(ct));
    }
    m_statusCoords->setText(text);
}
