// MapDocument: the loaded map, its tileset graphics and the undo history,
// plus the undoable commands. See MapDocument.h for the editing model.

#include "MapDocument.h"

#include "CellTypeInfo.h"
#include "TileImageCache.h"

#include <Stream/DynamicMemoryWriter.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QUndoStack>

#include <exception>
#include <stdexcept>

namespace {

// Replicates Map::GetTileIndex (private in OP2Utility): 32-tile-wide column
// blocks, storage index (upperX * height + y) * 32 + lowerX.
std::size_t tileIndex(const OP2Utility::Map &map, int x, int y)
{
    const std::size_t lowerX = static_cast<std::size_t>(x) & 0x1F;
    const std::size_t upperX = static_cast<std::size_t>(x) >> 5;
    return (upperX * map.HeightInTiles() + static_cast<std::size_t>(y)) * 32 + lowerX;
}

QString tr(const char *s) { return QCoreApplication::translate("MapDocument", s); }

} // namespace

// ---- MapCommand -------------------------------------------------------------

// Commands are applied by MapDocument::execute() before they are pushed, so
// QUndoStack's first redo() (on push) is skipped.
void MapCommand::redo()
{
    if (m_skipRedo) {
        m_skipRedo = false;
        return;
    }
    apply();
    m_doc->flushChanges();
}

void MapCommand::undo()
{
    revert();
    m_doc->flushChanges();
}

// ---- MapDocument ------------------------------------------------------------

// The undo limit has to be set while the stack is empty.
MapDocument::MapDocument(QObject *parent)
    : QObject(parent)
    , m_undoStack(new QUndoStack(this))
{
    m_undoStack->setUndoLimit(MaxUndoSteps);
    connect(m_undoStack, &QUndoStack::cleanChanged, this, [this] { emitDirtyIfChanged(); });
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this] {
        emitDirtyIfChanged();
        emit undoRedoChanged();
    });
}

MapDocument::~MapDocument()
{
    for (MapCommand *c : m_stroke)
        delete c;
}

QStringList MapDocument::tilesetSubfolders()
{
    return { QStringLiteral("OPU/base/tilesets"), QStringLiteral("base/tilesets"),
             QStringLiteral("tilesets") };
}

// First existing of OPU/base/tilesets, base/tilesets and tilesets under the Outpost 2 folder.
QString MapDocument::resolveTilesetDir(const QString &op2Folder)
{
    if (op2Folder.trimmed().isEmpty())
        return QString();
    for (const QString &sub : tilesetSubfolders()) {
        const QString path = QDir(op2Folder).filePath(sub);
        if (QDir(path).exists())
            return path;
    }
    return QString();
}

bool MapDocument::load(const QString &mapPath, const QString &op2Folder,
                       QString &errorOut, MapLoadInfo &info)
{
    try {
        return loadMap(readMapFile(mapPath), mapPath, op2Folder, errorOut, info);
    } catch (const std::exception &e) {
        errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool MapDocument::loadMap(OP2Utility::Map &&mapIn, const QString &path, const QString &op2Folder,
                          QString &errorOut, MapLoadInfo &info)
{
    try {
        auto map = std::make_unique<OP2Utility::Map>(std::move(mapIn));
        const QString mapPath = path;
        const QString tilesetDir = resolveTilesetDir(op2Folder);
        std::unique_ptr<OP2Utility::ResourceManager> resources;
        if (QDir(op2Folder).exists()) {
            try {
                resources = std::make_unique<OP2Utility::ResourceManager>(
                    QDir::toNativeSeparators(op2Folder).toStdString());
            } catch (const std::exception &) {
                resources.reset(); // archives are only a fallback
            }
        }
        auto cache = std::make_unique<TileImageCache>(resources.get(), tilesetDir);
        MapLoadInfo li;
        li.missingTilesets = cache->load(map->tilesetSources);
        li.tilesetDir = tilesetDir;
        li.tilesetDirFound = !tilesetDir.isEmpty();
        li.tilesetsTotal = cache->sourceCount();
        li.tilesetsLoaded = cache->loadedCount();

        // Commit.
        for (MapCommand *c : m_stroke)
            delete c;
        m_stroke.clear();
        m_strokeOpen = false;
        m_undoStack->clear();
        m_cache = std::move(cache);
        m_map = std::move(map);
        m_resources = std::move(resources);
        m_currentPath = mapPath;
        m_pending = QRect();
        m_undoStack->setClean();
        info = li;
        m_lastDirty = false;
        emit documentReplaced();
        emit dirtyChanged(false);
        emit undoRedoChanged();
        return true;
    } catch (const std::exception &e) {
        errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

// Close Map. The undo stack object stays, so menu actions bound to it never dangle.
void MapDocument::reset()
{
    for (MapCommand *c : m_stroke)
        delete c;
    m_stroke.clear();
    m_strokeOpen = false;
    m_undoStack->clear();
    m_undoStack->setClean();
    m_cache.reset();
    m_map.reset();
    m_resources.reset();
    m_currentPath.clear();
    m_pending = QRect();
    m_lastDirty = false;
    emit documentReplaced();
    emit dirtyChanged(false);
    emit undoRedoChanged();
}

int MapDocument::widthTiles() const { return m_map ? static_cast<int>(m_map->WidthInTiles()) : 0; }
int MapDocument::heightTiles() const { return m_map ? static_cast<int>(m_map->HeightInTiles()) : 0; }
int MapDocument::mappingCount() const { return m_map ? static_cast<int>(m_map->tileMappings.size()) : 0; }

bool MapDocument::inBounds(int x, int y) const
{
    return m_map && x >= 0 && y >= 0 && x < widthTiles() && y < heightTiles();
}

int MapDocument::cellTypeAt(int x, int y) const
{
    if (!inBounds(x, y))
        return 0;
    const std::size_t idx = tileIndex(*m_map, x, y);
    if (idx >= m_map->tiles.size())
        return 0;
    return static_cast<int>(m_map->tiles[idx].cellType) & 0x1F;
}

int MapDocument::mappingIndexAt(int x, int y) const
{
    if (!inBounds(x, y))
        return -1;
    const std::size_t idx = tileIndex(*m_map, x, y);
    if (idx >= m_map->tiles.size())
        return -1;
    return static_cast<int>(m_map->tiles[idx].tileMappingIndex);
}

int MapDocument::tilesetIndexOfMapping(int mapping) const
{
    if (!m_map || mapping < 0 || mapping >= mappingCount())
        return -1;
    return m_map->tileMappings[static_cast<std::size_t>(mapping)].tilesetIndex;
}

int MapDocument::imageIndexOfMapping(int mapping) const
{
    if (!m_map || mapping < 0 || mapping >= mappingCount())
        return -1;
    return m_map->tileMappings[static_cast<std::size_t>(mapping)].tileGraphicIndex;
}

QString MapDocument::tilesetNameOfMapping(int mapping) const
{
    const int ts = tilesetIndexOfMapping(mapping);
    if (ts < 0 || static_cast<std::size_t>(ts) >= m_map->tilesetSources.size())
        return QString();
    return QString::fromStdString(m_map->tilesetSources[static_cast<std::size_t>(ts)].tilesetFilename);
}

int MapDocument::findMapping(int tilesetIdx, int imageIdx) const
{
    if (!m_map)
        return -1;
    for (std::size_t i = 0; i < m_map->tileMappings.size(); ++i) {
        const auto &m = m_map->tileMappings[i];
        if (m.tilesetIndex == tilesetIdx && m.tileGraphicIndex == imageIdx)
            return static_cast<int>(i);
    }
    return -1;
}

// Changed tiles are collected and announced once by flushChanges().
void MapDocument::noteChanged(int x, int y)
{
    m_pending = m_pending.isNull() ? QRect(x, y, 1, 1) : m_pending.united(QRect(x, y, 1, 1));
}

// Raw write, no undo. The tile field is 11 bits wide, so 0..2047.
void MapDocument::setMappingIndexAt(int x, int y, int mapping)
{
    if (!inBounds(x, y) || mapping < 0 || mapping >= 2048)
        return;
    const std::size_t idx = tileIndex(*m_map, x, y);
    if (idx >= m_map->tiles.size())
        return;
    if (static_cast<int>(m_map->tiles[idx].tileMappingIndex) == mapping)
        return;
    m_map->tiles[idx].tileMappingIndex = static_cast<unsigned>(mapping);
    noteChanged(x, y);
}

// Raw write, no undo. Cell types are 0..31.
void MapDocument::setCellTypeAt(int x, int y, int cellType)
{
    if (!inBounds(x, y) || cellType < 0 || cellType >= CellTypeInfo::Count)
        return;
    const std::size_t idx = tileIndex(*m_map, x, y);
    if (idx >= m_map->tiles.size())
        return;
    if ((static_cast<int>(m_map->tiles[idx].cellType) & 0x1F) == cellType)
        return;
    m_map->tiles[idx].cellType = static_cast<OP2Utility::CellType>(cellType);
    noteChanged(x, y);
}

void MapDocument::flushChanges()
{
    if (m_pending.isNull())
        return;
    const QRect r = m_pending;
    m_pending = QRect();
    emit regionChanged(r);
}

// Apply first, record after: a command that throws leaves the history and
// the dirty state as they were. Inside a stroke the command waits in the
// stroke list until endStroke().
void MapDocument::execute(MapCommand *cmd)
{
    if (!m_map) {
        delete cmd;
        return;
    }
    try {
        cmd->apply();
    } catch (...) {
        flushChanges();
        delete cmd;
        throw;
    }
    flushChanges();
    if (m_strokeOpen) {
        m_stroke.push_back(cmd);
        emitDirtyIfChanged();
    } else {
        cmd->markApplied();
        m_undoStack->push(cmd);
    }
    emit edited();
}

void MapDocument::beginStroke(const QString &description)
{
    endStroke();
    m_strokeOpen = true;
    m_strokeDescription = description;
}

// An empty stroke records nothing, one command is pushed as it is, more become one composite step.
void MapDocument::endStroke()
{
    if (!m_strokeOpen)
        return;
    m_strokeOpen = false;
    std::vector<MapCommand *> cmds;
    cmds.swap(m_stroke);
    if (cmds.empty())
        return;
    MapCommand *top = nullptr;
    if (cmds.size() == 1) {
        top = cmds.front();
    } else {
        const QString text = m_strokeDescription.isEmpty()
            ? tr("Paint (%1 tiles)").arg(cmds.size())
            : m_strokeDescription;
        top = new CompositeCommand(this, std::move(cmds), text);
    }
    top->markApplied();
    m_undoStack->push(top);
}

// Undo / redo in the middle of a drag commit the stroke first (as in VB).
void MapDocument::undo()
{
    endStroke();
    if (!m_undoStack->canUndo())
        return;
    m_undoStack->undo();
    emit edited();
}

void MapDocument::redo()
{
    endStroke();
    if (!m_undoStack->canRedo())
        return;
    m_undoStack->redo();
    emit edited();
}

QString MapDocument::undoText() const { return m_undoStack->undoText(); }
QString MapDocument::redoText() const { return m_undoStack->redoText(); }
bool MapDocument::canUndo() const { return m_undoStack->canUndo(); }
bool MapDocument::canRedo() const { return m_undoStack->canRedo(); }

// Unsaved = not at the save point, or an open stroke that already changed tiles.
bool MapDocument::isDirty() const
{
    if (!m_map)
        return false;
    return !m_undoStack->isClean() || !m_stroke.empty();
}

void MapDocument::emitDirtyIfChanged()
{
    const bool d = isDirty();
    if (d != m_lastDirty) {
        m_lastDirty = d;
        emit dirtyChanged(d);
    }
}

void MapDocument::markSavePoint()
{
    endStroke();
    m_undoStack->setClean();
    emitDirtyIfChanged();
}

// New Map / JSON import: the clean state becomes unreachable, so even undoing everything still counts as unsaved.
void MapDocument::markNewUnsaved()
{
    m_undoStack->resetClean();
    emitDirtyIfChanged();
}

bool MapDocument::save(const QString &path, QString &errorOut)
{
    if (!m_map) {
        errorOut = tr("No map loaded");
        return false;
    }
    endStroke();
    try {
        writeMapFileSafely(*m_map, path);
    } catch (const std::exception &e) {
        errorOut = QString::fromUtf8(e.what());
        return false;
    }
    m_currentPath = path;
    markSavePoint();
    return true;
}

// Reads through QFile so non-ASCII paths work on Windows (OP2Utility's own file streams take a narrow path).
OP2Utility::Map MapDocument::readMapFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        throw std::runtime_error(QStringLiteral("Cannot open %1: %2")
                                     .arg(QDir::toNativeSeparators(path), f.errorString())
                                     .toStdString());
    const QByteArray bytes = f.readAll();
    OP2Utility::Stream::MemoryReader reader(bytes.constData(),
                                            static_cast<std::size_t>(bytes.size()));
    return OP2Utility::Map::ReadMap(reader);
}

// Serialise in memory, check the bytes read back as the same map, then write
// through QSaveFile (temp file + atomic rename). A failed save never damages
// the existing file.
void MapDocument::writeMapFileSafely(const OP2Utility::Map &map, const QString &path)
{
    OP2Utility::Stream::DynamicMemoryWriter writer;
    map.Write(writer);
    auto reader = writer.GetReader();
    const auto length = reader.Length();
    QByteArray bytes(static_cast<qsizetype>(length), Qt::Uninitialized);
    reader.Read(bytes.data(), static_cast<std::size_t>(length));

    {
        OP2Utility::Stream::MemoryReader check(bytes.constData(),
                                               static_cast<std::size_t>(bytes.size()));
        const OP2Utility::Map verify = OP2Utility::Map::ReadMap(check);
        if (verify.WidthInTiles() != map.WidthInTiles()
            || verify.HeightInTiles() != map.HeightInTiles()
            || verify.tiles.size() != map.tiles.size()
            || verify.tileMappings.size() != map.tileMappings.size())
            throw std::runtime_error("The written map did not read back correctly; the file was not replaced.");
    }

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly))
        throw std::runtime_error(QStringLiteral("Cannot write %1: %2")
                                     .arg(QDir::toNativeSeparators(path), out.errorString())
                                     .toStdString());
    if (out.write(bytes) != bytes.size() || !out.commit())
        throw std::runtime_error(QStringLiteral("Failed to write %1: %2")
                                     .arg(QDir::toNativeSeparators(path), out.errorString())
                                     .toStdString());
}

// ---- Commands ---------------------------------------------------------------

ChangeTileMappingCommand::ChangeTileMappingCommand(MapDocument *doc, int x, int y,
                                                   int oldMapping, int newMapping)
    : MapCommand(doc), m_x(x), m_y(y), m_old(oldMapping), m_new(newMapping)
{
    setText(tr("Change tile at (%1,%2) from mapping %3 to %4").arg(x).arg(y).arg(oldMapping).arg(newMapping));
}
void ChangeTileMappingCommand::apply() { m_doc->setMappingIndexAt(m_x, m_y, m_new); }
void ChangeTileMappingCommand::revert() { m_doc->setMappingIndexAt(m_x, m_y, m_old); }

ChangeCellTypeCommand::ChangeCellTypeCommand(MapDocument *doc, int x, int y, int oldType, int newType)
    : MapCommand(doc), m_x(x), m_y(y), m_old(oldType), m_new(newType)
{
    setText(tr("Change cell type at (%1,%2) from %3 to %4")
                .arg(x).arg(y).arg(CellTypeInfo::enumName(oldType), CellTypeInfo::enumName(newType)));
}
void ChangeCellTypeCommand::apply() { m_doc->setCellTypeAt(m_x, m_y, m_new); }
void ChangeCellTypeCommand::revert() { m_doc->setCellTypeAt(m_x, m_y, m_old); }

PasteTilesCommand::PasteTilesCommand(MapDocument *doc, int x0, int y0, int w, int h,
                                     std::vector<int> mappings, std::vector<int> cellTypes)
    : MapCommand(doc), m_x0(x0), m_y0(y0), m_w(w), m_h(h),
      m_new(std::move(mappings)), m_newCells(std::move(cellTypes))
{
    m_old.resize(static_cast<std::size_t>(w * h));
    if (!m_newCells.empty())
        m_oldCells.resize(static_cast<std::size_t>(w * h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * w + x);
            m_old[i] = doc->mappingIndexAt(x0 + x, y0 + y);
            if (!m_oldCells.empty())
                m_oldCells[i] = doc->cellTypeAt(x0 + x, y0 + y);
        }
    }
    setText(m_newCells.empty() ? tr("Paste %1x%2 Tiles").arg(w).arg(h)
                               : tr("Paste %1x%2 Tiles + Cell Types").arg(w).arg(h));
}

// The map's bottom row keeps its own cell type: it must stay impassable.
void PasteTilesCommand::apply()
{
    const int lastRow = m_doc->lastRow();
    for (int y = 0; y < m_h; ++y) {
        for (int x = 0; x < m_w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * m_w + x);
            m_doc->setMappingIndexAt(m_x0 + x, m_y0 + y, m_new[i]);
            if (!m_newCells.empty() && m_y0 + y != lastRow)
                m_doc->setCellTypeAt(m_x0 + x, m_y0 + y, m_newCells[i]);
        }
    }
}

void PasteTilesCommand::revert()
{
    for (int y = 0; y < m_h; ++y) {
        for (int x = 0; x < m_w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * m_w + x);
            m_doc->setMappingIndexAt(m_x0 + x, m_y0 + y, m_old[i]);
            if (!m_oldCells.empty())
                m_doc->setCellTypeAt(m_x0 + x, m_y0 + y, m_oldCells[i]);
        }
    }
}

PlaceTileGroupCommand::PlaceTileGroupCommand(MapDocument *doc, int x0, int y0, int w, int h,
                                             std::vector<int> mappings, const QString &name)
    : MapCommand(doc), m_x0(x0), m_y0(y0), m_w(w), m_h(h), m_new(std::move(mappings))
{
    m_old.resize(static_cast<std::size_t>(w * h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            m_old[static_cast<std::size_t>(y * w + x)] = doc->mappingIndexAt(x0 + x, y0 + y);
    setText(name.isEmpty()
        ? tr("Place Tile Group (%1×%2 tiles)").arg(w).arg(h)
        : tr("Place Tile Group '%1' (%2×%3 tiles)").arg(name).arg(w).arg(h));
}

void PlaceTileGroupCommand::apply()
{
    for (int y = 0; y < m_h; ++y)
        for (int x = 0; x < m_w; ++x)
            m_doc->setMappingIndexAt(m_x0 + x, m_y0 + y, m_new[static_cast<std::size_t>(y * m_w + x)]);
}

void PlaceTileGroupCommand::revert()
{
    for (int y = 0; y < m_h; ++y)
        for (int x = 0; x < m_w; ++x)
            m_doc->setMappingIndexAt(m_x0 + x, m_y0 + y, m_old[static_cast<std::size_t>(y * m_w + x)]);
}

CompositeCommand::CompositeCommand(MapDocument *doc, std::vector<MapCommand *> commands,
                                   const QString &text)
    : MapCommand(doc), m_commands(std::move(commands))
{
    setText(text);
}

CompositeCommand::~CompositeCommand()
{
    for (MapCommand *c : m_commands)
        delete c;
}

void CompositeCommand::apply()
{
    for (MapCommand *c : m_commands)
        c->apply();
}

// Reverse order, so a tile painted twice in one stroke is restored correctly.
void CompositeCommand::revert()
{
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it)
        (*it)->revert();
}

TileChangesCommand::TileChangesCommand(MapDocument *doc, std::vector<TileChange> changes,
                                       const QString &text)
    : MapCommand(doc), m_changes(std::move(changes))
{
    setText(text);
}

void TileChangesCommand::apply()
{
    for (const auto &c : m_changes) {
        m_doc->setMappingIndexAt(c.x, c.y, c.newMapping);
        m_doc->setCellTypeAt(c.x, c.y, c.newCellType);
    }
}

// Reverse order, like CompositeCommand.
void TileChangesCommand::revert()
{
    for (auto it = m_changes.rbegin(); it != m_changes.rend(); ++it) {
        m_doc->setMappingIndexAt(it->x, it->y, it->oldMapping);
        m_doc->setCellTypeAt(it->x, it->y, it->oldCellType);
    }
}

ChangeCellTypesBulkCommand::ChangeCellTypesBulkCommand(MapDocument *doc,
                                                       std::vector<CellTypeChange> changes,
                                                       const QString &text)
    : MapCommand(doc), m_changes(std::move(changes))
{
    setText(text);
}

void ChangeCellTypesBulkCommand::apply()
{
    for (const auto &c : m_changes)
        m_doc->setCellTypeAt(c.x, c.y, c.newType);
}

void ChangeCellTypesBulkCommand::revert()
{
    for (const auto &c : m_changes)
        m_doc->setCellTypeAt(c.x, c.y, c.oldType);
}
