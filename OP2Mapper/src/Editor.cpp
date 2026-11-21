#include "Editor.h"

#include "CellTypeInfo.h"
#include "MapDocument.h"
#include "Op2Lib.h"
#include "Settings.h"
#include "TileImageCache.h"

#include <QFileInfo>
#include <QFont>
#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace {
constexpr int T = TileImageCache::TileSize;

QPixmap labelledSwatch(const QColor &back, const QColor &fore, int w, int h)
{
    QPixmap pm(32, 32);
    pm.fill(back);
    QPainter p(&pm);
    p.setPen(QPen(fore, 2));
    p.drawRect(2, 2, 28, 28);
    QFont f(QStringLiteral("Arial"));
    f.setPixelSize(9);
    f.setBold(true);
    p.setFont(f);
    const QString text = QStringLiteral("%1x%2").arg(w).arg(h);
    if (p.fontMetrics().horizontalAdvance(text) < 28)
        p.drawText(QRect(0, 0, 32, 32), Qt::AlignCenter, text);
    return pm;
}

QPixmap solidSwatch(const QColor &c)
{
    QPixmap pm(32, 32);
    pm.fill(c);
    return pm;
}

// 70% opacity preview of a mapping block, `ts` px per tile, with a border.
QImage buildPreview(const MapDocument &doc, int w, int h, const std::vector<int> &mappings,
                    int ts, const QColor &border)
{
    QImage img(w * ts, h * ts, QImage::Format_ARGB32_Premultiplied);
    if (img.isNull())
        return img;
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setOpacity(0.7);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const QImage tile = MapRenderer::mappingImage(doc, mappings[static_cast<std::size_t>(y * w + x)]);
            if (!tile.isNull())
                p.drawImage(QRect(x * ts, y * ts, ts, ts), tile);
        }
    }
    p.setOpacity(1.0);
    p.setPen(QPen(border, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(1, 1, img.width() - 2, img.height() - 2));
    return img;
}
} // namespace

Editor::Editor(MapDocument *doc, QWidget *dialogParent, QObject *parent)
    : QObject(parent), m_doc(doc), m_dialogParent(dialogParent)
{
    m_copyTimer.setSingleShot(true);
    m_copyTimer.setInterval(2000);
    connect(&m_copyTimer, &QTimer::timeout, this, &Editor::onCopyTimer);

    m_enclosedTimer.setSingleShot(true);
    m_enclosedTimer.setInterval(400);
    connect(&m_enclosedTimer, &QTimer::timeout, this, [this] {
        if (m_highlightOn && hasMap()) {
            recomputeEnclosed();
            emit overlayChanged();
        }
    });
    connect(m_doc, &MapDocument::edited, this, [this] {
        if (m_highlightOn)
            m_enclosedTimer.start();
    });

    int size = Settings::windowInt(QStringLiteral("TerrainBrushSize"), 5);
    size = std::clamp(size, 3, 25);
    if (size % 2 == 0)
        size = std::min(25, size + 1);
    m_brushSize = size;
    m_cliffSide = std::clamp(Settings::windowInt(QStringLiteral("TerrainCliffSide"), 0), 0, 3);
    m_cliffHeight = std::clamp(Settings::windowInt(QStringLiteral("TerrainCliffHeight"), 3), 3, 6);
}

Editor::~Editor() = default;

bool Editor::hasMap() const { return m_doc && m_doc->isLoaded(); }

bool Editor::lockLastRow() const { return !Settings::allowEditLastRow(); }

void Editor::info(const QString &title, const QString &text)
{
    QMessageBox::information(m_dialogParent, title, text);
}

void Editor::warn(const QString &title, const QString &text)
{
    QMessageBox::warning(m_dialogParent, title, text);
}

bool Editor::refuseInRestrictedMode(const QString &action)
{
    if (!MapRenderer::isRestrictedMode(m_renderMode))
        return false;
    info(tr("Operation Not Available"),
         tr("Cannot %1 while in %2 Render Mode.").arg(action, MapRenderer::modeName(m_renderMode)));
    return true;
}

// ---- Map lifecycle -----------------------------------------------------------

void Editor::onMapLoaded()
{
    if (m_editMode)
        setEditMode(false);
    m_highlightOn = false;
    m_enclosed.clear();
    emit highlightChanged(false);
    m_renderMode = MapRenderer::GameMap;
    m_overlays.grid = m_overlays.largeGrid = m_overlays.coords = false;
    m_activeCellType = 0;
    m_cellToolActive = false;
    m_activeMapping = -1;
    m_selectedTileImage = QImage();
    m_selectedTilesetName.clear();
    m_selectedTilesetIndex = -1;
    m_selectedImageIndex = -1;
    m_currentTile = QPoint(-1, -1);
    m_statusOverride.clear();
    clearClipboard();
    clearTileGroup();
    emit renderModeChanged(m_renderMode);
    emit overlaysChanged();
    emit currentTileChanged();
    emit toolChanged();
    emit statusChanged();
}

void Editor::onMapClosed()
{
    if (m_editMode)
        setEditMode(false);
    clearClipboard();
    clearTileGroup();
    m_highlightOn = false;
    m_enclosed.clear();
    emit highlightChanged(false);
    m_currentTile = QPoint(-1, -1);
    m_statusOverride.clear();
    emit currentTileChanged();
    emit toolChanged();
    emit statusChanged();
    emit overlayChanged();
}

// ---- Edit mode ---------------------------------------------------------------

void Editor::setEditMode(bool on)
{
    if (on == m_editMode)
        return;
    if (on) {
        if (!hasMap())
            return;
        m_editMode = true;
        m_activeCellType = 0;
        m_cellToolActive = false;
        m_activeMapping = -1;
        m_selectedTileImage = QImage();
        m_selectedTilesetName.clear();
        m_selectedTilesetIndex = -1;
        m_selectedImageIndex = -1;
        m_copyMode = false;
        m_copyStage = 0;
        m_groupMode = false;
    } else {
        if (m_copyMode)
            exitCopyMode();
        if (m_pasteMode)
            exitPasteMode();
        if (m_groupMode)
            exitTileGroupPlacement();
        m_editMode = false;
        m_pasteMode = false;
        m_groupMode = false;
        m_terrainActive = false;
        m_cliffActive = false;
        m_cliffPoints.clear();
        endTerrainStroke();
        m_copyMode = false;
        m_copyStage = 0;
    }
    m_statusOverride.clear();
    emit editModeChanged(m_editMode);
    emit pasteAvailabilityChanged();
    emit toolChanged();
    emit terrainStateChanged();
    emit statusChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::setSingleTileMode(bool on)
{
    m_singleTile = on;
    emit statusChanged();
}

QString Editor::statusName() const
{
    if (!m_statusOverride.isEmpty())
        return m_statusOverride;
    if (!hasMap())
        return QString();
    QString name = QFileInfo(m_doc->currentPath()).fileName();
    if (m_editMode)
        name += m_singleTile ? tr(" [Single Tile]") : tr(" [Drag Edit]");
    return name;
}

void Editor::setStatusOverride(const QString &text)
{
    m_statusOverride = text;
    emit statusChanged();
}

// ---- View state --------------------------------------------------------------

void Editor::setRenderMode(int mode)
{
    if (!hasMap() || mode == m_renderMode)
        return;
    const bool wasRestricted = MapRenderer::isRestrictedMode(m_renderMode);
    m_renderMode = mode;
    if (MapRenderer::isRestrictedMode(mode) && !wasRestricted) {
        // ResetToolsForRestrictedMode: no copy/paste, no tile tool.
        if (m_copyMode)
            exitCopyMode();
        if (m_pasteMode)
            exitPasteMode();
        m_activeMapping = -1;
        m_selectedTileImage = QImage();
        m_selectedTilesetName.clear();
        m_selectedTilesetIndex = -1;
        m_selectedImageIndex = -1;
        emit tilesetSelectionCleared();
        emit toolChanged();
    }
    emit renderModeChanged(mode);
}

void Editor::setShowGrid(bool on) { m_overlays.grid = on; emit overlaysChanged(); }
void Editor::setShowLargeGrid(bool on) { m_overlays.largeGrid = on; emit overlaysChanged(); }
void Editor::setShowCoords(bool on) { m_overlays.coords = on; emit overlaysChanged(); }

void Editor::setGridSizes(int largeGridSize, int coordSpacing)
{
    m_overlays.largeGridSize = largeGridSize;
    m_overlays.coordSpacing = coordSpacing;
    emit overlaysChanged();
}

void Editor::setCurrentTile(const QPoint &t)
{
    if (t == m_currentTile)
        return;
    m_currentTile = t;
    emit currentTileChanged();
    emit overlayChanged();
}

// ---- Tools -------------------------------------------------------------------

bool Editor::pickTilesetTile(int tilesetIndex, int imageIndex)
{
    if (!hasMap())
        return false;
    if (m_copyMode)
        return false;
    if (refuseInRestrictedMode(tr("place tiles")))
        return false;
    const int mapping = m_doc->findMapping(tilesetIndex, imageIndex);
    if (mapping < 0)
        return true; // highlight moves, tool unchanged (no mapping inserted)
    m_activeCellType = 0;
    m_cellToolActive = false;
    m_activeMapping = mapping;
    QString name = m_doc->tilesetNameOfMapping(mapping);
    if (name.endsWith(QLatin1String(".bmp"), Qt::CaseInsensitive))
        name.chop(4);
    m_selectedTilesetName = name;
    m_selectedTilesetIndex = tilesetIndex;
    m_selectedImageIndex = imageIndex;
    m_selectedTileImage = MapRenderer::mappingImage(*m_doc, mapping);
    if (m_selectedTileImage.isNull()) {
        m_selectedTileImage = QImage(T, T, QImage::Format_ARGB32);
        m_selectedTileImage.fill(Qt::black);
    }
    emit cellTypeSelectionCleared();
    emit toolChanged();
    emit terrainStateChanged();
    emit cursorChanged();
    emit overlayChanged();
    return true;
}

void Editor::pickCellType(int cellType)
{
    m_selectedTileImage = QImage();
    m_selectedTilesetName.clear();
    m_selectedTilesetIndex = -1;
    m_selectedImageIndex = -1;
    m_activeMapping = -1;
    m_activeCellType = cellType;
    m_cellToolActive = true;
    emit tilesetSelectionCleared();
    emit toolChanged();
    emit terrainStateChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::selectTileUnderCursor(const QPoint &tile)
{
    if (!m_doc->inBounds(tile.x(), tile.y()))
        return;
    const int mapping = m_doc->mappingIndexAt(tile.x(), tile.y());
    const int ts = m_doc->tilesetIndexOfMapping(mapping);
    const int img = m_doc->imageIndexOfMapping(mapping);
    if (ts < 0 || img < 0) {
        info(tr("No Tile"), tr("No valid tile found at the cursor position."));
        return;
    }
    m_activeMapping = mapping;
    QString name = m_doc->tilesetNameOfMapping(mapping);
    if (name.endsWith(QLatin1String(".bmp"), Qt::CaseInsensitive))
        name.chop(4);
    m_selectedTilesetName = name;
    m_selectedTilesetIndex = ts;
    m_selectedImageIndex = img;
    m_selectedTileImage = MapRenderer::rawTileImage(*m_doc, tile.x(), tile.y());
    m_activeCellType = 0;
    m_cellToolActive = false;
    emit cellTypeSelectionCleared();
    setCurrentTile(tile);
    emit toolChanged();
    emit terrainStateChanged();
    emit cursorChanged();
}

void Editor::clearCurrentTool()
{
    if (!m_editMode)
        return;
    stopTerrainTools();
    m_activeMapping = -1;
    m_selectedTileImage = QImage();
    m_selectedTilesetName.clear();
    m_selectedTilesetIndex = -1;
    m_selectedImageIndex = -1;
    m_cellToolActive = false;
    m_activeCellType = 0;
    if (m_pasteMode)
        exitPasteMode();
    emit tilesetSelectionCleared();
    emit cellTypeSelectionCleared();
    if (m_copyStage < 2 && m_copyMode)
        exitCopyMode();
    m_statusOverride.clear();
    emit toolChanged();
    emit statusChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::resetToolUi(bool keepCopyData)
{
    m_activeMapping = -1;
    m_selectedTileImage = QImage();
    m_selectedTilesetName.clear();
    m_selectedTilesetIndex = -1;
    m_selectedImageIndex = -1;
    m_cellToolActive = false;
    m_activeCellType = 0;
    m_copyMode = false;
    m_copyStage = 0;
    if (!keepCopyData)
        clearClipboard();
    emit tilesetSelectionCleared();
    emit cellTypeSelectionCleared();
    emit toolChanged();
}

// ---- Copy --------------------------------------------------------------------

QRect Editor::copyRect() const
{
    QPoint end = m_copyStage == 2 ? m_copyEnd : m_copyHover;
    if (m_copyStart.x() < 0 || end.x() < 0)
        return QRect();
    return QRect(QPoint(std::min(m_copyStart.x(), end.x()), std::min(m_copyStart.y(), end.y())),
                 QPoint(std::max(m_copyStart.x(), end.x()), std::max(m_copyStart.y(), end.y())));
}

void Editor::beginCopy()
{
    if (!hasMap() || !m_editMode)
        return;
    if (refuseInRestrictedMode(tr("copy tiles")))
        return;
    if (m_pasteMode)
        exitPasteMode();
    if (m_groupMode)
        exitTileGroupPlacement();
    m_copyTimer.stop();
    stopTerrainTools();
    m_copyMode = true;
    m_copyStage = 0;
    m_copyStart = m_copyEnd = m_copyHover = QPoint(-1, -1);
    m_copyDragging = false;
    m_statusOverride.clear();
    emit toolChanged();
    emit statusChanged();
    emit overlayChanged();
}

void Editor::exitCopyMode()
{
    m_copyTimer.stop();
    m_copyMode = false;
    m_copyStage = 0;
    m_copyStart = m_copyEnd = m_copyHover = QPoint(-1, -1);
    m_copyDragging = false;
    m_statusOverride.clear();
    emit pasteAvailabilityChanged();
    emit toolChanged();
    emit statusChanged();
    emit overlayChanged();
}

void Editor::finishCopySelection(const QPoint &end)
{
    m_copyEnd = end;
    m_copyStage = 2;
    m_copyHover = QPoint(-1, -1);
    m_copyDragging = false;
    completeCopy();
    emit toolChanged();
    emit overlayChanged();
}

void Editor::completeCopy()
{
    const QRect r = copyRect();
    if (r.isEmpty())
        return;
    TileClipboardData c;
    c.width = r.width();
    c.height = r.height();
    c.mappings.resize(static_cast<std::size_t>(c.width * c.height));
    c.cellTypes.resize(c.mappings.size());
    for (int y = 0; y < c.height; ++y) {
        for (int x = 0; x < c.width; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * c.width + x);
            c.mappings[i] = m_doc->mappingIndexAt(r.x() + x, r.y() + y);
            c.cellTypes[i] = m_doc->cellTypeAt(r.x() + x, r.y() + y);
        }
    }
    c.generation = m_nextGeneration++;
    m_clipboard = std::move(c);
    emit clipboardChanged();
    emit pasteAvailabilityChanged();
    m_copyTimer.start();
}

void Editor::onCopyTimer()
{
    m_copyMode = false;
    m_copyStage = 0;
    m_copyStart = m_copyEnd = m_copyHover = QPoint(-1, -1);
    m_activeCellType = 0;
    m_cellToolActive = false;
    m_activeMapping = -1;
    m_selectedTileImage = QImage();
    emit tilesetSelectionCleared();
    emit toolChanged();
    emit overlayChanged();
}

void Editor::clearClipboard()
{
    m_clipboard = TileClipboardData();
    emit clipboardChanged();
    emit pasteAvailabilityChanged();
}

// ---- Paste -------------------------------------------------------------------

bool Editor::pasteCellTypes() const { return Settings::pasteCellTypes(); }
void Editor::setPasteCellTypes(bool on) { Settings::setPasteCellTypes(on); }

void Editor::beginPaste()
{
    if (!hasMap()) {
        info(tr("Cannot Paste"), tr("No map loaded. Please open a map first."));
        return;
    }
    if (refuseInRestrictedMode(tr("paste tiles")))
        return;
    m_copyTimer.stop();
    if (m_copyMode) {
        m_copyMode = false;
        m_copyStage = 0;
        m_copyStart = m_copyEnd = m_copyHover = QPoint(-1, -1);
    }
    if (m_clipboard.isEmpty()) {
        info(tr("Cannot Paste"), tr("No tiles copied. Please copy tiles first."));
        return;
    }
    if (m_groupMode)
        exitTileGroupPlacement();
    stopTerrainTools();
    m_pasteMode = true;
    m_pastePreview = QPoint(-1, -1);
    rebuildPastePreview();
    m_statusOverride = tr("Paste Mode: Click to paste tiles");
    emit toolChanged();
    emit statusChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::rebuildPastePreview()
{
    const int w = m_clipboard.width, h = m_clipboard.height;
    const double fit = std::sqrt(4096.0 * 4096.0 / (static_cast<double>(w) * h));
    const int ts = std::clamp(static_cast<int>(std::floor(fit)), 1, 32);
    m_pastePreviewImage = buildPreview(*m_doc, w, h, m_clipboard.mappings, ts, Qt::white);
}

void Editor::exitPasteMode()
{
    m_pasteMode = false;
    m_pastePreviewImage = QImage();
    m_pastePreview = QPoint(-1, -1);
    m_currentTile = QPoint(-1, -1);
    resetToolUi(true);
    m_statusOverride.clear();
    emit currentTileChanged();
    emit statusChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::executePaste(const QPoint &tile)
{
    const int w = m_clipboard.width, h = m_clipboard.height;
    if (tile.x() + w > m_doc->widthTiles() || tile.y() + h > m_doc->heightTiles()) {
        warn(tr("Paste Error"),
             tr("Cannot paste at this location. The paste region would extend beyond the map boundary."));
        return;
    }
    std::vector<int> cells;
    if (pasteCellTypes())
        cells = m_clipboard.cellTypes;
    m_doc->execute(new PasteTilesCommand(m_doc, tile.x(), tile.y(), w, h, m_clipboard.mappings, std::move(cells)));
    exitPasteMode();
}

// ---- Tile groups -------------------------------------------------------------

void Editor::setTileGroup(const TileGroupData &g)
{
    m_tileGroup = g;
    emit tileGroupChanged();
}

void Editor::clearTileGroup()
{
    m_tileGroup = TileGroupData();
    if (m_groupMode)
        exitTileGroupPlacement();
    emit tileGroupChanged();
}

void Editor::beginTileGroupPlacement(bool fromButton)
{
    if (!hasMap())
        return;
    if (!m_tileGroup.isLoaded()) {
        if (fromButton)
            info(tr("Cannot Place"), tr("No tile group selected. Please select a tile group first."));
        return;
    }
    stopTerrainTools();
    if (m_pasteMode)
        exitPasteMode();
    if (m_copyMode)
        exitCopyMode();
    m_groupMode = true;
    m_groupPreview = QPoint(-1, -1);
    rebuildGroupPreview();
    emit toolChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::rebuildGroupPreview()
{
    m_groupPreviewImage = buildPreview(*m_doc, m_tileGroup.width, m_tileGroup.height,
                                       m_tileGroup.mappings, T, QColor(255, 0, 255));
}

void Editor::exitTileGroupPlacement()
{
    m_groupMode = false;
    m_groupPreview = QPoint(-1, -1);
    m_groupPreviewImage = QImage();
    m_currentTile = QPoint(-1, -1);
    resetToolUi(true);
    emit currentTileChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::executeTileGroupPlacement(const QPoint &tile)
{
    const int w = m_tileGroup.width, h = m_tileGroup.height;
    if (tile.x() + w > m_doc->widthTiles() || tile.y() + h > m_doc->heightTiles()) {
        warn(tr("Placement Error"),
             tr("Cannot place tile group at this location. It would extend beyond the map boundary."));
        return;
    }
    const int count = m_doc->mappingCount();
    int bad = 0, first = 0;
    for (int m : m_tileGroup.mappings) {
        if (m < 0 || m >= count) {
            if (bad++ == 0)
                first = m;
        }
    }
    if (bad > 0) {
        warn(tr("Incompatible Tile Group"),
             tr("This tile group can't be placed on this map: %1 of its tiles use mapping indices "
                "this map doesn't have (e.g. %2; this map has %3 mappings, 0-%4).\n\n"
                "The group was probably made for a map with a different tileset setup.")
                 .arg(bad).arg(first).arg(count).arg(count - 1));
        return;
    }
    m_doc->execute(new PlaceTileGroupCommand(m_doc, tile.x(), tile.y(), w, h,
                                             m_tileGroup.mappings, m_tileGroup.fileName));
    exitTileGroupPlacement();
}

// ---- Painting ----------------------------------------------------------------

void Editor::handleTilePlacement(const QPoint &tile)
{
    if (m_activeMapping < 0 || m_activeMapping >= m_doc->mappingCount())
        return;
    const int old = m_doc->mappingIndexAt(tile.x(), tile.y());
    if (old == m_activeMapping)
        return;
    m_doc->execute(new ChangeTileMappingCommand(m_doc, tile.x(), tile.y(), old, m_activeMapping));
}

void Editor::handleCellTypeClick(const QPoint &tile)
{
    if (tile.y() == m_doc->lastRow() && lockLastRow()) {
        endEditDrag(); // before the modal box, or the release is lost
        warn(tr("Cannot Edit Last Row"),
             tr("Cant edit bottom row. This row is not visible in game and all cell types should be Impassable."));
        return;
    }
    if (m_renderMode != MapRenderer::CellTypeOverlayAll && m_renderMode != MapRenderer::CellTypeBasic
        && m_renderMode != MapRenderer::CellTypeAll) {
        m_renderMode = MapRenderer::CellTypeOverlayAll;
        emit renderModeChanged(m_renderMode);
    }
    const int old = m_doc->cellTypeAt(tile.x(), tile.y());
    if (old == m_activeCellType)
        return;
    m_doc->execute(new ChangeCellTypeCommand(m_doc, tile.x(), tile.y(), old, m_activeCellType));
}

void Editor::endEditDrag()
{
    m_editDragging = false;
    m_lastEditTile = QPoint(-1, -1);
    m_doc->endStroke();
    endTerrainStroke();
}

// ---- Mouse -------------------------------------------------------------------

void Editor::mapLeftPress(const QPoint &tile)
{
    if (!hasMap() || !m_doc->inBounds(tile.x(), tile.y()))
        return;
    if (m_pasteMode) {
        executePaste(tile);
        return;
    }
    if (m_groupMode) {
        executeTileGroupPlacement(tile);
        return;
    }
    if (m_editMode) {
        if (m_copyMode && m_copyStage == 0) {
            m_copyStart = tile;
            m_copyStage = 1;
            m_copyDragging = true;
            m_copyHover = tile;
            emit toolChanged();
            emit overlayChanged();
            return;
        }
        if (m_copyMode && m_copyStage == 1) {
            finishCopySelection(tile);
            return;
        }
        if (cliffToolActive()) {
            cliffClick(tile);
        } else if (terrainToolActive()) {
            beginTerrainStroke(tile);
        } else if (tileToolActive() || m_cellToolActive) {
            m_editDragging = true;
            m_doc->beginStroke();
            m_lastEditTile = tile;
            if (tileToolActive())
                handleTilePlacement(tile);
            else
                handleCellTypeClick(tile);
        }
    }
    setCurrentTile(tile);
}

void Editor::mapMouseMove(const QPoint &tileIn, bool inside, bool leftHeld)
{
    if (!hasMap())
        return;

    if (m_editMode && m_copyMode && m_copyStage == 1) {
        const QPoint t(std::clamp(tileIn.x(), 0, m_doc->widthTiles() - 1),
                       std::clamp(tileIn.y(), 0, m_doc->heightTiles() - 1));
        if (t != m_copyHover) {
            m_copyHover = t;
            emit toolChanged();
            emit overlayChanged();
        }
    }
    if (!inside)
        return;
    const QPoint tile = tileIn;

    if (m_editMode) {
        if (m_pasteMode) {
            if (tile != m_pastePreview) {
                m_pastePreview = tile;
                emit toolChanged();
                emit overlayChanged();
            }
            return;
        }
        if (m_groupMode) {
            if (tile != m_groupPreview) {
                m_groupPreview = tile;
                emit toolChanged();
                emit overlayChanged();
            }
            return;
        }
    }

    if (m_editDragging && !leftHeld)
        endEditDrag();

    if (cliffToolActive() && !m_cliffPoints.isEmpty()) {
        const QPoint snapped = Op2Lib::snapCliffPoint(m_cliffPoints.last(), tile);
        if (snapped != m_cliffCursor) {
            m_cliffCursor = snapped;
            emit overlayChanged();
        }
    }
    if (terrainToolActive()) {
        if (tile != m_brushHover) {
            m_brushHover = tile;
            emit overlayChanged();
        }
        if (m_editDragging && m_strokeBlend && tile != m_lastDab) {
            m_lastDab = tile;
            terrainDab(tile);
        }
        return;
    }

    if (m_editDragging && m_editMode && !m_singleTile && tile != m_lastEditTile) {
        m_lastEditTile = tile;
        if (tileToolActive())
            handleTilePlacement(tile);
        else if (m_cellToolActive)
            handleCellTypeClick(tile);
        if (m_editDragging)
            setCurrentTile(tile);
    }
}

void Editor::mapLeftRelease()
{
    endEditDrag();
    if (m_copyDragging) {
        m_copyDragging = false;
        if (m_copyMode && m_copyStage == 1 && m_copyHover.x() >= 0 && m_copyHover != m_copyStart)
            finishCopySelection(m_copyHover);
    }
}

bool Editor::handleEscape()
{
    if (!hasMap())
        return false;
    if (cliffToolActive() && handleCliffKey(Qt::Key_Escape))
        return true;
    if (m_groupMode) {
        exitTileGroupPlacement();
        return true;
    }
    if (m_copyMode && m_copyStage < 2) {
        exitCopyMode();
        return true;
    }
    if (m_pasteMode) {
        exitPasteMode();
        return true;
    }
    return false;
}

// ---- Terrain brush -----------------------------------------------------------

bool Editor::terrainToolActive() const
{
    return m_editMode && m_terrainActive && !m_cliffActive && m_ground >= 0
        && m_activeMapping < 0 && !m_cellToolActive;
}

void Editor::setBrushSize(int size)
{
    size = std::clamp(size, 3, 25);
    if (size % 2 == 0)
        size = std::min(25, size + 1);
    if (size == m_brushSize)
        return;
    m_brushSize = size;
    Settings::setWindowInt(QStringLiteral("TerrainBrushSize"), size);
    emit terrainStateChanged();
    emit toolChanged();
    emit overlayChanged();
}

void Editor::stepBrushSize(int delta) { setBrushSize(m_brushSize + delta); }

void Editor::armTerrain(int ground, const QString &name, const QColor &color)
{
    if (m_pasteMode)
        exitPasteMode();
    if (m_groupMode)
        exitTileGroupPlacement();
    if (m_copyMode && m_copyStage < 2)
        exitCopyMode();
    m_activeMapping = -1;
    m_selectedTileImage = QImage();
    m_cellToolActive = false;
    emit tilesetSelectionCleared();
    emit cellTypeSelectionCleared();
    m_cliffActive = false;
    m_cliffPoints.clear();
    m_cliffCursor = QPoint(-1, -1);
    m_ground = ground;
    m_groundName = name;
    m_groundColor = color;
    m_terrainActive = true;
    emit toolChanged();
    emit terrainStateChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::disarmTerrain()
{
    m_terrainActive = false;
    m_brushHover = QPoint(-1, -1);
    endTerrainStroke();
    emit toolChanged();
    emit terrainStateChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::stopTerrainTools()
{
    m_cliffActive = false;
    m_cliffPoints.clear();
    m_cliffCursor = QPoint(-1, -1);
    m_terrainActive = false;
    m_brushHover = QPoint(-1, -1);
    endTerrainStroke();
    emit terrainStateChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::beginTerrainStroke(const QPoint &tile)
{
    if (refuseInRestrictedMode(tr("paint terrain")))
        return;
    endTerrainStroke();
    auto blend = std::make_unique<Op2Lib::BlendMap>();
    QString err;
    bool unavailable = false;
    if (!blend->create(*m_doc, err, unavailable)) {
        if (!unavailable)
            warn(tr("Terrain Brush"), tr("The terrain brush couldn't read this map: %1").arg(err));
        return;
    }
    m_strokeBlend = std::move(blend);
    m_strokeSeed = QRandomGenerator::global()->generate64();
    m_editDragging = true;
    m_doc->beginStroke(tr("Terrain Brush: %1").arg(m_groundName));
    m_lastEditTile = tile;
    m_lastDab = tile;
    terrainDab(tile);
}

void Editor::terrainDab(const QPoint &tile)
{
    if (!m_strokeBlend)
        return;
    const int half = m_brushSize / 2;
    const double limit = (half + 0.5) * (half + 0.5);
    const bool lock = lockLastRow();
    const int last = m_doc->lastRow();
    std::vector<int32_t> xy;
    for (int dy = -half; dy <= half; ++dy) {
        for (int dx = -half; dx <= half; ++dx) {
            if (dx * dx + dy * dy > limit)
                continue;
            const int x = tile.x() + dx, y = tile.y() + dy;
            if (!m_doc->inBounds(x, y) || (lock && y == last))
                continue;
            xy.push_back(x);
            xy.push_back(y);
        }
    }
    if (xy.empty() || m_strokeBlend->paint(m_ground, xy) <= 0)
        return;
    const int n = m_strokeBlend->resolve(m_strokeSeed);
    if (n <= 0)
        return;
    applyBlendChanges(Op2Lib::toTileChanges(*m_doc, m_strokeBlend->changes(n), lock),
                      tr("Terrain Brush: %1").arg(m_groundName));
}

void Editor::endTerrainStroke()
{
    m_strokeBlend.reset();
    m_lastDab = QPoint(-1, -1);
}

void Editor::applyBlendChanges(const std::vector<TileChange> &changes, const QString &text)
{
    if (changes.empty())
        return;
    m_doc->execute(new TileChangesCommand(m_doc, changes, text));
    if (m_currentTile.x() >= 0)
        emit currentTileChanged();
}

void Editor::cleanUpTransitions()
{
    if (!hasMap()) {
        info(tr("Clean Up Transitions"), tr("No map loaded."));
        return;
    }
    if (!m_editMode)
        return;
    if (refuseInRestrictedMode(tr("change tiles")))
        return;
    Op2Lib::BlendMap blend;
    QString err;
    bool unavailable = false;
    if (!blend.create(*m_doc, err, unavailable)) {
        warn(tr("Clean Up Transitions"), unavailable
            ? tr("The terrain brush is unavailable: %1").arg(err)
            : tr("Clean Up Transitions failed: %1").arg(err));
        return;
    }
    const int n = blend.resolveAll(QRandomGenerator::global()->generate64());
    if (n <= 0) {
        info(tr("Clean Up Transitions"), tr("All transitions already fit; nothing to change."));
        return;
    }
    const auto changes = Op2Lib::toTileChanges(*m_doc, blend.changes(n), lockLastRow());
    const auto answer = QMessageBox::question(m_dialogParent, tr("Clean Up Transitions"),
        tr("Clean Up Transitions will change %1 tile(s) so the borders between mud, rock and sand "
           "use matching transition tiles.\n\nThis can be undone. Continue?").arg(n),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    m_doc->endStroke();
    applyBlendChanges(changes, tr("Clean Up Transitions (%1 tiles)").arg(n));
}

// ---- Draw cliff --------------------------------------------------------------

bool Editor::cliffToolActive() const
{
    return m_editMode && m_cliffActive && m_activeMapping < 0 && !m_cellToolActive;
}

void Editor::armCliff()
{
    if (m_pasteMode)
        exitPasteMode();
    if (m_groupMode)
        exitTileGroupPlacement();
    if (m_copyMode && m_copyStage < 2)
        exitCopyMode();
    m_activeMapping = -1;
    m_selectedTileImage = QImage();
    m_cellToolActive = false;
    emit tilesetSelectionCleared();
    emit cellTypeSelectionCleared();
    m_terrainActive = false;
    m_brushHover = QPoint(-1, -1);
    endTerrainStroke();
    m_cliffActive = true;
    m_cliffPoints.clear();
    m_cliffCursor = QPoint(-1, -1);
    emit toolChanged();
    emit terrainStateChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::disarmCliff()
{
    m_cliffActive = false;
    m_cliffPoints.clear();
    m_cliffCursor = QPoint(-1, -1);
    emit toolChanged();
    emit terrainStateChanged();
    emit cursorChanged();
    emit overlayChanged();
}

void Editor::setCliffOptions(int side, int height)
{
    m_cliffSide = std::clamp(side, 0, 3);
    m_cliffHeight = std::clamp(height, 3, 6);
    Settings::setWindowInt(QStringLiteral("TerrainCliffSide"), m_cliffSide);
    Settings::setWindowInt(QStringLiteral("TerrainCliffHeight"), m_cliffHeight);
    emit toolChanged();
}

void Editor::cliffClick(const QPoint &tile)
{
    if (refuseInRestrictedMode(tr("draw cliffs")))
        return;
    const QPoint p = m_cliffPoints.isEmpty() ? tile : Op2Lib::snapCliffPoint(m_cliffPoints.last(), tile);
    if (!m_doc->inBounds(p.x(), p.y()))
        return;
    if (!m_cliffPoints.isEmpty() && p == m_cliffPoints.last())
        return;
    m_cliffPoints.append(p);
    emit overlayChanged();
    if (m_cliffPoints.size() > 3 && p == m_cliffPoints.first()) {
        placeCliff();
        return;
    }
    emit toolChanged();
}

void Editor::placeCliff()
{
    if (!hasMap() || !cliffToolActive())
        return;
    if (m_cliffPoints.size() < 2) {
        info(tr("Draw Cliff"), tr("Click at least two points on the map first."));
        return;
    }
    Op2Lib::BlendMap blend;
    QString err;
    bool unavailable = false;
    if (!blend.create(*m_doc, err, unavailable)) {
        warn(tr("Draw Cliff"), unavailable ? tr("Cliffs are unavailable: %1").arg(err)
                                           : tr("Draw Cliff failed: %1").arg(err));
        return;
    }
    std::vector<int32_t> xy;
    for (const QPoint &p : m_cliffPoints) {
        xy.push_back(p.x());
        xy.push_back(p.y());
    }
    const int n = blend.drawCliff(xy, m_cliffSide, m_cliffHeight,
                                  QRandomGenerator::global()->generate64());
    if (n < 0) {
        warn(tr("Draw Cliff"), tr("Draw Cliff failed: %1").arg(
            tr("Invalid cliff: points outside the map, fewer than 2 points, or a bad side.")));
        return;
    }
    const int stage = blend.lastCliffStage();
    if (n == 0) {
        info(tr("Draw Cliff"), tr("No cliff could be built along these points."));
        return;
    }
    const auto changes = Op2Lib::toTileChanges(*m_doc, blend.changes(n), lockLastRow());
    m_doc->endStroke();
    applyBlendChanges(changes, tr("Draw Cliff (%1 tiles)").arg(n));
    m_cliffPoints.clear();
    m_cliffCursor = QPoint(-1, -1);
    m_statusOverride = tr("Cliff placed: %1 tiles").arg(n)
        + (stage < 0 ? tr(" (rough joins: no exact fit found)") : QString());
    emit statusChanged();
    emit toolChanged();
    emit overlayChanged();
}

bool Editor::handleCliffKey(int key)
{
    if (!cliffToolActive())
        return false;
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        placeCliff();
        return true;
    case Qt::Key_Backspace:
        if (!m_cliffPoints.isEmpty()) {
            m_cliffPoints.removeLast();
            if (m_cliffPoints.isEmpty())
                m_cliffCursor = QPoint(-1, -1);
            emit toolChanged();
            emit overlayChanged();
        }
        return true;
    case Qt::Key_Escape:
        if (!m_cliffPoints.isEmpty()) {
            m_cliffPoints.clear();
            m_cliffCursor = QPoint(-1, -1);
            emit toolChanged();
            emit overlayChanged();
        } else {
            disarmCliff();
        }
        return true;
    default:
        return false;
    }
}

// ---- Enclosed areas ----------------------------------------------------------

void Editor::recomputeEnclosed()
{
    m_enclosed.clear();
    QVector<QVector<QPoint>> regions;
    QString err;
    if (!Op2Lib::findRegions(*m_doc, regions, err))
        return;
    for (int i = 1; i < regions.size(); ++i)
        m_enclosed += regions[i];
}

void Editor::setHighlightEnclosed(bool on)
{
    if (on) {
        if (!hasMap()) {
            m_highlightOn = false;
            emit highlightChanged(false);
            return;
        }
        recomputeEnclosed();
        if (m_enclosed.isEmpty()) {
            m_highlightOn = false;
            emit highlightChanged(false);
            info(tr("Highlight Enclosed Areas"), tr("No enclosed areas found to highlight."));
            emit overlayChanged();
            return;
        }
        m_highlightOn = true;
    } else {
        m_highlightOn = false;
        m_enclosed.clear();
    }
    emit highlightChanged(m_highlightOn);
    emit overlayChanged();
}

void Editor::findAndFixEnclosedAreas()
{
    const QString title = tr("Find & Fix Enclosed Areas");
    if (!hasMap()) {
        info(tr("Find Enclosed Areas"), tr("No map loaded."));
        return;
    }
    QVector<QVector<QPoint>> regions;
    QString err;
    if (!Op2Lib::findRegions(*m_doc, regions, err)) {
        warn(tr("Enclosed Areas"), tr("Enclosed-area analysis failed: %1").arg(err));
        return;
    }
    if (regions.isEmpty()) {
        info(tr("Find Enclosed Areas"), tr("No passable tiles found in this map."));
        return;
    }
    if (regions.size() == 1) {
        info(tr("Find Enclosed Areas"),
             tr("No enclosed areas found.\n\nThe map has a single connected passable region (%1 tiles).")
                 .arg(regions[0].size()));
        return;
    }
    QStringList sizes;
    int total = 0;
    for (int i = 1; i < regions.size(); ++i) {
        sizes << tr("%1 tiles").arg(regions[i].size());
        total += regions[i].size();
    }
    const int k = regions.size() - 1;
    const auto answer = QMessageBox::question(m_dialogParent, title,
        tr("Found %1 enclosed area(s) — sizes: %2.\n\nMain play area: %3 tiles.\n\n"
           "Convert all %4 enclosed tile(s) to Impassable 1 (I1)?\n\n"
           "Tiles already at the target cell type are not changed.\n"
           "This action can be undone with Ctrl+Z.")
            .arg(k).arg(sizes.join(QStringLiteral(", "))).arg(regions[0].size()).arg(total),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    constexpr int kImpassible1 = 6;
    std::vector<CellTypeChange> changes;
    for (int i = 1; i < regions.size(); ++i) {
        for (const QPoint &p : regions[i]) {
            const int old = m_doc->cellTypeAt(p.x(), p.y());
            if (old != kImpassible1)
                changes.push_back({ p.x(), p.y(), old, kImpassible1 });
        }
    }
    if (changes.empty()) {
        info(title, tr("All enclosed tiles are already Impassable 1 (I1) — nothing to change."));
        return;
    }
    const int c = static_cast<int>(changes.size());
    m_doc->endStroke();
    m_doc->execute(new ChangeCellTypesBulkCommand(m_doc, std::move(changes),
        tr("Fix %1 enclosed area(s) (%2 tiles)").arg(k).arg(c)));
    info(title, tr("Enclosed area fix complete.\n\nAreas edited: %1\nTiles changed: %2").arg(k).arg(c));
    setHighlightEnclosed(false);
}

// ---- Labels ------------------------------------------------------------------

ToolLabels Editor::toolLabels() const
{
    ToolLabels l;
    const QColor gray = QColor(109, 109, 109);
    l.tool = tr("No Tool Selected");
    l.placement = tr("No Placement Selected");
    l.toolColor = l.placementColor = gray;
    l.swatchBack = Qt::black;
    if (!m_editMode)
        return l;

    const QColor saddle(139, 69, 19);
    if (m_groupMode) {
        l.tool = tr("Current Tool: Place Tile Group");
        l.toolColor = QColor(139, 0, 139);
        l.placement = m_groupPreview.x() >= 0
            ? tr("Click to place %1x%2 tile group at %3").arg(m_tileGroup.width).arg(m_tileGroup.height).arg(formatTile(m_groupPreview))
            : tr("Click location to place %1x%2 tile group").arg(m_tileGroup.width).arg(m_tileGroup.height);
        l.placementColor = QColor(255, 0, 255);
        l.swatch = labelledSwatch(QColor(230, 230, 250), QColor(139, 0, 139), m_tileGroup.width, m_tileGroup.height);
        return l;
    }
    if (m_pasteMode) {
        l.tool = tr("Current Tool: Paste Tiles");
        l.toolColor = QColor(0, 0, 139);
        l.placement = m_pastePreview.x() >= 0
            ? tr("Click to paste %1x%2 tiles at %3").arg(m_clipboard.width).arg(m_clipboard.height).arg(formatTile(m_pastePreview))
            : tr("Click location to paste %1x%2 tiles").arg(m_clipboard.width).arg(m_clipboard.height)
                  + (pasteCellTypes() ? tr(" + cell types") : QString());
        l.placementColor = QColor(0, 0, 255);
        l.swatch = labelledSwatch(Qt::white, QColor(0, 0, 255), m_clipboard.width, m_clipboard.height);
        return l;
    }
    if (!m_copyMode && cliffToolActive()) {
        static const char *sides[] = { "left", "right", "north", "south" };
        l.tool = tr("Current Tool: Draw Cliff");
        l.placement = m_cliffPoints.isEmpty()
            ? tr("Click points (high %1, height %2)").arg(QString::fromLatin1(sides[m_cliffSide])).arg(m_cliffHeight)
            : tr("%1 point(s): Enter places the cliff").arg(m_cliffPoints.size());
        l.toolColor = l.placementColor = saddle;
        l.swatch = solidSwatch(QColor(255, 210, 90));
        return l;
    }
    if (!m_copyMode && terrainToolActive()) {
        l.tool = tr("Current Tool: Terrain Brush");
        l.placement = tr("Painting: %1 (size %2)").arg(m_groundName).arg(m_brushSize);
        l.toolColor = l.placementColor = saddle;
        l.swatch = solidSwatch(m_groundColor);
        return l;
    }
    if (m_copyMode) {
        l.tool = tr("Current Tool: Tile Copy/Paste");
        l.toolColor = QColor(0, 100, 0);
        switch (m_copyStage) {
        case 0:
            l.placement = tr("Selecting start copy tile");
            l.placementColor = QColor(0, 128, 0);
            l.swatch = solidSwatch(QColor(0, 128, 0));
            break;
        case 1: {
            const QRect r = copyRect();
            l.placement = r.isEmpty() ? tr("Select end copy tile")
                                      : tr("Selecting %1x%2 tiles to copy").arg(r.width()).arg(r.height());
            l.placementColor = QColor(0, 100, 0);
            l.swatch = solidSwatch(QColor(0, 100, 0));
            break;
        }
        default:
            l.placement = tr("Region Copied: %1x%2 tiles").arg(m_clipboard.width).arg(m_clipboard.height);
            l.placementColor = QColor(0, 0, 139);
            l.swatch = solidSwatch(QColor(0, 0, 139));
            break;
        }
        return l;
    }
    if (tileToolActive()) {
        l.tool = tr("Current Tool: Tile Placement");
        l.toolColor = QColor(0, 0, 255);
        l.placement = tr("Placing: Tile: %1").arg(m_activeMapping);
        l.placementColor = QColor(0, 0, 139);
        l.swatch = QPixmap::fromImage(m_selectedTileImage);
        return l;
    }
    if (m_cellToolActive) {
        l.tool = tr("Current Tool: Cell Type");
        l.toolColor = QColor(139, 0, 0);
        l.placement = tr("Placing: %1").arg(CellTypeInfo::enumName(m_activeCellType));
        l.placementColor = CellTypeInfo::categoryColor(m_activeCellType);
        l.swatch = solidSwatch(CellTypeInfo::categoryColor(m_activeCellType));
        return l;
    }
    return l;
}
