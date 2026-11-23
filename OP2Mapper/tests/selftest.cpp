// Headless self-test for the OP2Mapper editing core. Drives the same
// Editor / MapDocument / MapView code the app uses, answers message boxes
// automatically, and writes rendered images for visual inspection.
//
//   QT_QPA_PLATFORM=offscreen op2mapper_selftest <op2 folder> <map file> <out dir>
//
// Set QT_QPA_FONTDIR (e.g. C:/Windows/Fonts) so text renders offscreen.
// The test keeps its settings in <out dir>/Mapper3.ini (OP2MAPPER_INI), so a
// real Mapper3.ini next to the exe is left alone.

#include "Editor.h"
#include "MainWindow.h"
#include "ToolPalette.h"

#include <QPushButton>
#include "IniFile.h"
#include "MapDocument.h"
#include "MapJsonIO.h"
#include "MapRenderer.h"
#include "MapView.h"
#include "Op2Lib.h"
#include "Settings.h"
#include "TileGroupIO.h"
#include "TileImageCache.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QPainter>
#include <QUndoStack>
#include <QTimer>
#include <QWidget>

#include <cstdio>

namespace {

int g_failures = 0;
int g_checks = 0;
QStringList g_boxes; // texts of message boxes that were answered

void check(bool ok, const char *what)
{
    ++g_checks;
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++g_failures;
}

// Answers any modal message box: Yes if offered, otherwise OK.
void startAutoAnswer()
{
    auto *t = new QTimer(qApp);
    QObject::connect(t, &QTimer::timeout, [] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            g_boxes << box->windowTitle() + QStringLiteral(": ") + box->text();
            QAbstractButton *b = box->button(QMessageBox::Yes);
            if (!b)
                b = box->button(QMessageBox::Ok);
            if (b)
                b->click();
            else
                box->reject();
        }
    });
    t->start(20);
}

bool sawBox(const QString &needle)
{
    for (const QString &s : g_boxes)
        if (s.contains(needle))
            return true;
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc < 4) {
        std::printf("usage: op2mapper_selftest <op2 folder> <map file> <out dir>\n");
        return 2;
    }
    const QString op2 = QString::fromLocal8Bit(argv[1]);
    const QString mapPath = QString::fromLocal8Bit(argv[2]);
    const QString out = QString::fromLocal8Bit(argv[3]);
    QDir().mkpath(out);
    // Use a Mapper3.ini of our own, so a real one next to the exe is never touched.
    const QString ini = QDir(out).filePath(QStringLiteral("Mapper3.ini"));
    QFile::remove(ini);
    qputenv("OP2MAPPER_INI", ini.toUtf8());
    startAutoAnswer();

    // The main-window phase reads Mapper3.ini; give it the Outpost 2 folder so
    // it doesn't stop at the first-run Settings dialog.
    Settings::setOp2Path(op2);
    if (Settings::workingPath().isEmpty())
        Settings::setWorkingPath(out);

    MapDocument doc;
    QWidget parent;
    Editor ed(&doc, &parent);

    // ---- INI rules
    IniFile::setValue(QStringLiteral("SelfTest"), QStringLiteral("Path"), QStringLiteral("C:\\a b\\c"));
    check(IniFile::value(QStringLiteral("selftest"), QStringLiteral("PATH")) == QStringLiteral("C:\\a b\\c"),
          "INI: raw backslash value, case-insensitive section/key");
    IniFile::setBool(QStringLiteral("SelfTest"), QStringLiteral("Flag"), true);
    check(IniFile::boolValue(QStringLiteral("SelfTest"), QStringLiteral("Flag"), false), "INI: bool round trip");

    // ---- Load
    QString err;
    MapLoadInfo info;
    QElapsedTimer timer;
    timer.start();
    const bool loaded = doc.load(mapPath, op2, err, info);
    std::printf("      load took %lld ms\n", static_cast<long long>(timer.elapsed()));
    check(loaded, "load map");
    if (!loaded) {
        std::printf("      error: %s\n", qPrintable(err));
        return 1;
    }
    ed.onMapLoaded();
    check(info.tilesetDirFound, "tileset folder found");
    check(info.tilesetsLoaded == info.tilesetsTotal && info.tilesetsTotal > 0, "all tilesets loaded");
    check(!doc.isDirty(), "fresh load is clean");
    std::printf("      map %dx%d, %d mappings, %d/%d tilesets\n", doc.widthTiles(), doc.heightTiles(),
                doc.mappingCount(), info.tilesetsLoaded, info.tilesetsTotal);

    // Failed load keeps the current map
    MapLoadInfo bad;
    check(!doc.load(QStringLiteral("Z:/does/not/exist.map"), op2, err, bad) && doc.isLoaded(),
          "failed load leaves the current map loaded");

    // ---- Rendering
    for (int mode = 0; mode < 5; ++mode) {
        MapRenderer::Overlays o;
        o.grid = mode == 3;
        o.largeGrid = mode == 0;
        o.coords = mode == 0;
        QImage img(32 * 12, 32 * 10, QImage::Format_RGB32);
        img.fill(Qt::black);
        QPainter p(&img);
        const QRect tiles(20, 20, 12, 10);
        MapRenderer::drawTiles(p, doc, tiles, 32, QPoint(-20 * 32, -20 * 32), mode);
        MapRenderer::drawOverlays(p, doc, tiles, 32, QPoint(-20 * 32, -20 * 32), img.rect(), o);
        p.end();
        img.save(QDir(out).filePath(QStringLiteral("mode%1_%2.png").arg(mode).arg(MapRenderer::modeName(mode))));
    }
    for (int px : { 3, 8, 16 }) {
        for (int mode : { 0, 2, 4 }) {
            QElapsedTimer rt;
            rt.start();
            const QImage tmp = MapRenderer::renderFullMap(doc, px, mode, MapRenderer::Overlays());
            std::printf("      full render %2d px/tile mode %d: %lld ms\n", px, mode, static_cast<long long>(rt.elapsed()));
        }
    }
    const QImage full = MapRenderer::renderFullMap(doc, 4, MapRenderer::GameMap, MapRenderer::Overlays());
    check(full.width() == doc.widthTiles() * 4, "full map render at 12.5%");
    full.save(QDir(out).filePath(QStringLiteral("full_gamemap_4px.png")));

    // ---- Tile painting + undo
    ed.setEditMode(true);
    check(ed.editMode(), "edit mode on");
    const int ts = doc.tilesetIndexOfMapping(doc.mappingIndexAt(5, 5));
    const int img = doc.imageIndexOfMapping(doc.mappingIndexAt(5, 5));
    const QPoint target(10, 10);
    const int before = doc.mappingIndexAt(target.x(), target.y());
    check(ed.pickTilesetTile(ts, img), "pick a tile from the tileset");
    check(ed.tileToolActive(), "tile tool armed");
    ed.mapLeftPress(target);
    ed.mapLeftRelease();
    check(doc.mappingIndexAt(target.x(), target.y()) == doc.findMapping(ts, img), "tile painted");
    check(doc.isDirty(), "dirty after edit");
    check(doc.undoText().startsWith(QStringLiteral("Change tile at (10,10)")), "single-tile undo text");
    doc.undo();
    check(doc.mappingIndexAt(target.x(), target.y()) == before, "undo restores tile");
    check(!doc.isDirty(), "undo to save point is clean");
    doc.redo();
    doc.undo();

    // Drag stroke (Single Tile off) = one undo step
    ed.setSingleTileMode(false);
    const int undoBefore = doc.undoStack()->index();
    ed.mapLeftPress(QPoint(1, 1));
    ed.mapMouseMove(QPoint(2, 1), true, true);
    ed.mapMouseMove(QPoint(3, 1), true, true);
    ed.mapLeftRelease();
    check(doc.undoStack()->index() == undoBefore + 1, "drag stroke is one undo step");
    check(doc.undoText().startsWith(QStringLiteral("Paint (")), "stroke undo text 'Paint (N tiles)'");
    doc.undo();
    ed.setSingleTileMode(true);

    // ---- Cell types + bottom row lock
    ed.pickCellType(6);
    check(ed.cellToolActive() && !ed.tileToolActive(), "cell tool replaces tile tool");
    ed.mapLeftPress(QPoint(4, 4));
    ed.mapLeftRelease();
    check(doc.cellTypeAt(4, 4) == 6, "cell type painted");
    check(ed.renderMode() == MapRenderer::CellTypeOverlayAll, "cell paint switches to CellTypeOverlayAll");
    g_boxes.clear();
    const int lastRowType = doc.cellTypeAt(4, doc.lastRow());
    ed.pickCellType(0);
    ed.mapLeftPress(QPoint(4, doc.lastRow()));
    QApplication::processEvents();
    ed.mapLeftRelease();
    check(doc.cellTypeAt(4, doc.lastRow()) == lastRowType && sawBox(QStringLiteral("Cannot Edit Last Row")),
          "bottom row cell type locked with warning");
    doc.undo();
    ed.setRenderMode(MapRenderer::GameMap);

    // ---- Copy / paste
    ed.clearCurrentTool();
    ed.beginCopy();
    ed.mapLeftPress(QPoint(2, 2));
    ed.mapMouseMove(QPoint(5, 4), true, true);
    ed.mapLeftRelease();
    check(ed.clipboard().width == 4 && ed.clipboard().height == 3, "drag-select copy 4x3");
    ed.beginPaste();
    check(ed.pasteMode(), "paste mode armed");
    g_boxes.clear();
    ed.mapLeftPress(QPoint(doc.widthTiles() - 2, 0));
    QApplication::processEvents();
    check(ed.pasteMode() && sawBox(QStringLiteral("Paste Error")), "paste past the edge refused, stays armed");
    ed.mapLeftPress(QPoint(30, 30));
    check(!ed.pasteMode(), "paste commits once");
    bool same = true;
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 4; ++x)
            same = same && doc.mappingIndexAt(30 + x, 30 + y) == doc.mappingIndexAt(2 + x, 2 + y);
    check(same, "pasted raw mapping indices");
    check(doc.undoText() == QStringLiteral("Paste 4x3 Tiles") || doc.undoText() == QStringLiteral("Paste 4x3 Tiles + Cell Types"),
          "paste undo text");
    doc.undo();

    // ---- Tile groups
    TileGroupData g;
    g.width = 2;
    g.height = 2;
    g.mappings = { 1, 2, 3, 4 };
    g.fileName = QStringLiteral("selftest");
    const QString gjson = QDir(out).filePath(QStringLiteral("selftest_group.json"));
    check(TileGroupIO::writeJson(gjson, 2, 2, g.mappings, QStringLiteral("well00"), QStringLiteral("selftest"), err),
          "write tile group JSON");
    TileGroupData g2;
    TileGroupIO::Header hdr;
    check(TileGroupIO::load(gjson, g2, hdr, err) && g2.mappings == g.mappings, "read tile group JSON back");
    g2.fileName = QStringLiteral("selftest");
    ed.setTileGroup(g2);
    ed.beginTileGroupPlacement(true);
    ed.mapLeftPress(QPoint(40, 40));
    check(doc.mappingIndexAt(41, 41) == 4 && doc.undoText().contains(QStringLiteral("Place Tile Group 'selftest'")),
          "tile group placed (raw indices)");
    doc.undo();
    TileGroupData bigIdx = g2;
    bigIdx.mappings = { 1, 99999, 3, 4 };
    ed.setTileGroup(bigIdx);
    ed.beginTileGroupPlacement(true);
    g_boxes.clear();
    ed.mapLeftPress(QPoint(40, 40));
    QApplication::processEvents();
    check(sawBox(QStringLiteral("Incompatible Tile Group")), "incompatible tile group refused");
    ed.exitTileGroupPlacement();

    // ---- Terrain brush
    const auto grounds = Op2Lib::paintableGrounds();
    check(!grounds.isEmpty(), "op2maplib profile has paintable grounds");
    if (!grounds.isEmpty()) {
        const auto &gr = grounds.first();
        ed.armTerrain(gr.index, gr.name, gr.color);
        check(ed.terrainToolActive(), "terrain brush armed");
        const int undo0 = doc.undoStack()->index();
        ed.mapLeftPress(QPoint(20, 20));
        ed.mapMouseMove(QPoint(21, 20), true, true);
        ed.mapMouseMove(QPoint(22, 21), true, true);
        ed.mapLeftRelease();
        const bool changed = doc.undoStack()->index() == undo0 + 1;
        check(changed, "terrain stroke is one undo step");
        if (changed)
            std::printf("      %s\n", qPrintable(doc.undoText()));
        QImage after(32 * 14, 32 * 12, QImage::Format_RGB32);
        QPainter p(&after);
        MapRenderer::drawTiles(p, doc, QRect(14, 14, 14, 12), 32, QPoint(-14 * 32, -14 * 32), MapRenderer::GameMap);
        p.end();
        after.save(QDir(out).filePath(QStringLiteral("terrain_brush.png")));
        doc.undo();
        ed.disarmTerrain();

        // Clean up transitions (auto-confirmed)
        g_boxes.clear();
        ed.cleanUpTransitions();
        QApplication::processEvents();
        std::printf("      clean up: %s\n", g_boxes.isEmpty() ? "(no box)" : qPrintable(g_boxes.last().left(90)));
        if (doc.undoText().startsWith(QStringLiteral("Clean Up")))
            doc.undo();

        // Cliff
        ed.armCliff();
        ed.mapLeftPress(QPoint(30, 12));
        ed.mapLeftPress(QPoint(30, 24));
        ed.mapLeftPress(QPoint(42, 24));
        check(ed.cliffPoints().size() == 3, "cliff points collected");
        g_boxes.clear();
        ed.placeCliff();
        QApplication::processEvents();
        check(doc.undoText().startsWith(QStringLiteral("Draw Cliff (")), "cliff placed as one undo step");
        QImage cliff(32 * 20, 32 * 18, QImage::Format_RGB32);
        QPainter cp(&cliff);
        MapRenderer::drawTiles(cp, doc, QRect(26, 9, 20, 18), 32, QPoint(-26 * 32, -9 * 32), MapRenderer::GameMap);
        cp.end();
        cliff.save(QDir(out).filePath(QStringLiteral("cliff.png")));
        doc.undo();
        ed.disarmCliff();
    }

    // ---- Enclosed areas
    QVector<QVector<QPoint>> regions;
    check(Op2Lib::findRegions(doc, regions, err) && !regions.isEmpty(), "passable regions found");
    std::printf("      %d region(s), main %d tiles\n", static_cast<int>(regions.size()),
                regions.isEmpty() ? 0 : static_cast<int>(regions[0].size()));

    // ---- MapView paint
    {
        MapView view(&ed);
        view.resize(800, 600);
        view.documentReplaced();
        ed.setShowGrid(true);
        ed.beginCopy();
        ed.mapLeftPress(QPoint(3, 3));
        ed.mapMouseMove(QPoint(8, 6), true, false);
        const QImage shot = view.grabView();
        check(!shot.isNull() && shot.width() == 800, "map view paints");
        shot.save(QDir(out).filePath(QStringLiteral("mapview_copy.png")));
        ed.exitCopyMode();
        ed.setShowGrid(false);
    }

    // ---- Save, verify, reload
    const QString saved = QDir(out).filePath(QStringLiteral("selftest_saved.map"));
    ed.pickCellType(21);
    ed.mapLeftPress(QPoint(7, 7));
    ed.mapLeftRelease();
    check(doc.save(saved, err), "safe save");
    check(!doc.isDirty(), "clean after save");
    MapDocument doc2;
    MapLoadInfo info2;
    check(doc2.load(saved, op2, err, info2) && doc2.cellTypeAt(7, 7) == 21, "saved file reloads with the edit");

    // New-map semantics
    doc.markNewUnsaved();
    check(doc.isDirty(), "new map is unsaved");

    // ---- JSON round trip
    const QString json = QDir(out).filePath(QStringLiteral("selftest.json"));
    check(MapJsonIO::exportToJson(doc2, json, QStringLiteral("selftest_saved.map"), QStringLiteral("selftest"),
                                  QString(), QString(), err), "JSON export");

    // ---- Whole main window + palettes (layout screenshots)
    {
        MainWindow win;
        win.resize(1280, 860);
        check(win.startup({ mapPath }), "main window startup with a map argument");
        QApplication::processEvents();
        QPushButton *editBtn = nullptr;
        for (QPushButton *b : win.findChildren<QPushButton *>())
            if (b->text() == QStringLiteral("Enable Edit Mode"))
                editBtn = b;
        check(editBtn && editBtn->isEnabled(), "Enable Edit Mode button enabled after load");
        if (editBtn)
            editBtn->click();
        QApplication::processEvents();
        win.grab().save(QDir(out).filePath(QStringLiteral("mainwindow.png")));
        int shown = 0;
        for (ToolPalette *p : ToolPalette::palettes()) {
            if (p->isVisible()) {
                ++shown;
                p->grab().save(QDir(out).filePath(QStringLiteral("palette_%1.png").arg(p->key())));
            }
        }
        check(shown == 6, "edit mode shows the six palettes");
        if (editBtn)
            editBtn->click();
        QApplication::processEvents();
        int still = 0;
        for (ToolPalette *p : ToolPalette::palettes())
            still += p->isVisible() ? 1 : 0;
        check(still == 0, "leaving edit mode hides the palettes");
        win.hide();
    }

    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
