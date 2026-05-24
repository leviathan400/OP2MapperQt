#include "MainWindow.h"

#include "CellTypeInfo.h"
#include "MapDocument.h"
#include "MapView.h"
#include "Settings.h"

#include <OP2Utility.h>

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_doc(std::make_unique<MapDocument>())
{
    setWindowTitle(tr("OP2 Mapper"));
    resize(1200, 800);

    m_mapView = new MapView(this);
    setCentralWidget(m_mapView);

    auto *openAction = new QAction(tr("&Open Map..."), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenMap);

    m_closeAction = new QAction(tr("&Close Map"), this);
    m_closeAction->setShortcut(QKeySequence::Close);
    m_closeAction->setEnabled(false);
    connect(m_closeAction, &QAction::triggered, this, &MainWindow::onCloseMap);

    auto *folderAction = new QAction(tr("Set OP2 &Game Folder..."), this);
    connect(folderAction, &QAction::triggered, this, &MainWindow::onSetOp2Folder);

    auto *exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(m_closeAction);
    fileMenu->addSeparator();
    fileMenu->addAction(folderAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    m_cellTypesAction = new QAction(tr("Show &Cell Types"), this);
    m_cellTypesAction->setCheckable(true);
    m_cellTypesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    m_cellTypesAction->setEnabled(false);
    connect(m_cellTypesAction, &QAction::toggled, m_mapView, &MapView::setShowCellTypes);

    m_gridAction = new QAction(tr("Show &Grid"), this);
    m_gridAction->setCheckable(true);
    m_gridAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    m_gridAction->setEnabled(false);
    connect(m_gridAction, &QAction::toggled, m_mapView, &MapView::setShowGrid);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_gridAction);
    viewMenu->addAction(m_cellTypesAction);

    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setObjectName("MainToolBar");
    toolbar->addAction(openAction);
    toolbar->addSeparator();
    toolbar->addAction(m_gridAction);
    toolbar->addAction(m_cellTypesAction);

    m_statusPath = new QLabel(tr("No map loaded"));
    statusBar()->addWidget(m_statusPath, 1);
    m_statusCoords = new QLabel;
    m_statusCoords->setMinimumWidth(120);
    statusBar()->addPermanentWidget(m_statusCoords);

    connect(m_mapView, &MapView::tileHovered, this, &MainWindow::onTileHovered);
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

    QString err;
    QStringList missing;
    if (!m_doc->load(path, op2, err, missing)) {
        QMessageBox::critical(this, tr("Open Map"),
            tr("Failed to load map:\n%1").arg(err));
        return;
    }

    m_mapView->setDocument(m_doc.get());

    QString status = tr("%1  —  %2 × %3 tiles")
        .arg(path).arg(m_doc->widthTiles()).arg(m_doc->heightTiles());
    if (!missing.isEmpty())
        status += tr("  —  missing tilesets: %1").arg(missing.join(", "));
    m_statusPath->setText(status);
    setWindowTitle(tr("OP2 Mapper — %1").arg(QFileInfo(path).fileName()));
    m_closeAction->setEnabled(true);
    m_cellTypesAction->setEnabled(true);
    m_gridAction->setEnabled(true);
}

void MainWindow::onCloseMap()
{
    m_doc = std::make_unique<MapDocument>();
    m_mapView->setDocument(nullptr);
    m_statusPath->setText(tr("No map loaded"));
    m_statusCoords->clear();
    setWindowTitle(tr("OP2 Mapper"));
    m_closeAction->setEnabled(false);
    m_cellTypesAction->setChecked(false);
    m_cellTypesAction->setEnabled(false);
    m_gridAction->setChecked(false);
    m_gridAction->setEnabled(false);
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
