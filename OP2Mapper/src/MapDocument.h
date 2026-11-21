#pragma once

#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QUndoCommand>
#include <memory>
#include <vector>

#include <OP2Utility.h>

class MapDocument;
class TileImageCache;
class QUndoStack;

// Base for every map edit. Commands are applied by MapDocument::execute()
// BEFORE they are recorded (as in OP2Mapper3's ExecuteCommand), so the
// first redo() QUndoStack makes on push is skipped.
class MapCommand : public QUndoCommand
{
public:
    explicit MapCommand(MapDocument *doc) : m_doc(doc) {}
    virtual void apply() = 0;
    virtual void revert() = 0;
    void redo() final;
    void undo() final;
    void markApplied() { m_skipRedo = true; }

protected:
    MapDocument *m_doc;

private:
    bool m_skipRedo = false;
};

// Result of loading a map, for the tileset warnings OP2Mapper3 shows.
struct MapLoadInfo {
    bool tilesetDirFound = false;
    QString tilesetDir;
    int tilesetsTotal = 0;   // non-empty sources
    int tilesetsLoaded = 0;
    QStringList missingTilesets;
};

// One terrain/cliff/blend change (op2maplib om_change applied by the editor).
struct TileChange {
    int x = 0, y = 0;
    int oldMapping = 0, newMapping = 0;
    int oldCellType = 0, newCellType = 0;
};

// The document: the loaded OP2Utility::Map, its tileset graphics and the
// undo stack. All coordinates are 0-based (x, y); the .map column-block
// storage is hidden behind mappingIndexAt / cellTypeAt.
//
// Edits:
//   - execute(cmd) applies a command and records it (or adds it to the
//     open stroke). beginStroke/endStroke group a drag into one undo step:
//     0 commands -> nothing, 1 -> the command itself, more -> a composite.
//   - Undo history is capped at 500 steps.
//   - Dirty = undo stack not at its clean index, or an open stroke with
//     edits. markNewUnsaved() makes the clean state unreachable (New Map).
class MapDocument : public QObject
{
    Q_OBJECT
public:
    static constexpr int MaxUndoSteps = 500;

    explicit MapDocument(QObject *parent = nullptr);
    ~MapDocument() override;

    // Builds the new map and tilesets into locals and swaps them in only on
    // success; on failure the current map, history and path are untouched.
    bool load(const QString &mapPath, const QString &op2Folder,
              QString &errorOut, MapLoadInfo &info);
    // Same, for a map built in memory (JSON import). `path` may be empty.
    bool loadMap(OP2Utility::Map &&map, const QString &path, const QString &op2Folder,
                 QString &errorOut, MapLoadInfo &info);
    void reset();

    bool isLoaded() const { return m_map != nullptr; }
    const OP2Utility::Map *map() const { return m_map.get(); }
    OP2Utility::Map *mutableMap() { return m_map.get(); }
    const TileImageCache *cache() const { return m_cache.get(); }
    QUndoStack *undoStack() const { return m_undoStack; }

    int widthTiles() const;
    int heightTiles() const;
    int lastRow() const { return heightTiles() - 1; }
    int mappingCount() const;
    bool inBounds(int x, int y) const;

    // Reads; safe when not loaded (0 / -1).
    int cellTypeAt(int x, int y) const;
    int mappingIndexAt(int x, int y) const;
    int tilesetIndexOfMapping(int mapping) const;   // -1 if invalid
    int imageIndexOfMapping(int mapping) const;     // -1 if invalid
    QString tilesetNameOfMapping(int mapping) const; // "" if invalid
    // First mapping with this (tileset, image), or -1.
    int findMapping(int tilesetIdx, int imageIdx) const;

    // Low-level writes, no undo (commands and bulk initialisers use these).
    // Changed tiles are accumulated and announced by flushChanges().
    void setMappingIndexAt(int x, int y, int mapping);
    void setCellTypeAt(int x, int y, int cellType);
    void flushChanges();

    // Undoable editing.
    void execute(MapCommand *cmd);
    void beginStroke(const QString &description = QString());
    void endStroke();
    bool strokeOpen() const { return m_strokeOpen; }
    void undo();
    void redo();
    QString undoText() const;
    QString redoText() const;
    bool canUndo() const;
    bool canRedo() const;

    // Persistence
    bool save(const QString &path, QString &errorOut);
    QString currentPath() const { return m_currentPath; }
    void setCurrentPath(const QString &path) { m_currentPath = path; }
    bool isDirty() const;
    void markSavePoint();
    void markNewUnsaved();

    // .map file helpers through QFile (Unicode paths). The safe writer
    // serialises in memory, verifies a read-back, and replaces the target
    // atomically. Both throw std::exception on failure.
    static OP2Utility::Map readMapFile(const QString &path);
    static void writeMapFileSafely(const OP2Utility::Map &map, const QString &path);

    // OP2Mapper3's tileset folder search under the OP2 folder; empty if none.
    static QString resolveTilesetDir(const QString &op2Folder);
    static QStringList tilesetSubfolders();

signals:
    // Tiles whose mapping or cell type changed (tile coords, inclusive rect).
    void regionChanged(const QRect &tiles);
    // After every execute / undo / redo (for debounced refreshes).
    void edited();
    void dirtyChanged(bool dirty);
    void undoRedoChanged();
    void documentReplaced();

private:
    void noteChanged(int x, int y);
    void emitDirtyIfChanged();

    std::unique_ptr<OP2Utility::ResourceManager> m_resources;
    std::unique_ptr<OP2Utility::Map> m_map;
    std::unique_ptr<TileImageCache> m_cache;
    QUndoStack *m_undoStack = nullptr;
    QString m_currentPath;

    bool m_strokeOpen = false;
    QString m_strokeDescription;
    std::vector<MapCommand *> m_stroke; // owned until pushed
    QRect m_pending;
    bool m_lastDirty = false;
};

// ---- Commands (descriptions match OP2Mapper3) --------------------------------

class ChangeTileMappingCommand : public MapCommand
{
public:
    ChangeTileMappingCommand(MapDocument *doc, int x, int y, int oldMapping, int newMapping);
    void apply() override;
    void revert() override;
    int x() const { return m_x; }
    int y() const { return m_y; }
private:
    int m_x, m_y, m_old, m_new;
};

class ChangeCellTypeCommand : public MapCommand
{
public:
    ChangeCellTypeCommand(MapDocument *doc, int x, int y, int oldType, int newType);
    void apply() override;
    void revert() override;
private:
    int m_x, m_y, m_old, m_new;
};

// Raw mapping indices (+ optional cell types) for a w*h block at (x0, y0),
// row-major. Old values are snapshotted at construction. Cell types are
// never written to the map's bottom row.
class PasteTilesCommand : public MapCommand
{
public:
    PasteTilesCommand(MapDocument *doc, int x0, int y0, int w, int h,
                      std::vector<int> mappings, std::vector<int> cellTypes);
    void apply() override;
    void revert() override;
private:
    int m_x0, m_y0, m_w, m_h;
    std::vector<int> m_new, m_newCells, m_old, m_oldCells;
};

class PlaceTileGroupCommand : public MapCommand
{
public:
    PlaceTileGroupCommand(MapDocument *doc, int x0, int y0, int w, int h,
                          std::vector<int> mappings, const QString &name);
    void apply() override;
    void revert() override;
private:
    int m_x0, m_y0, m_w, m_h;
    std::vector<int> m_new, m_old;
};

class CompositeCommand : public MapCommand
{
public:
    CompositeCommand(MapDocument *doc, std::vector<MapCommand *> commands, const QString &text);
    ~CompositeCommand() override;
    void apply() override;
    void revert() override;
private:
    std::vector<MapCommand *> m_commands;
};

// Terrain brush / cliff / clean-up: mapping + cell type per tile.
class TileChangesCommand : public MapCommand
{
public:
    TileChangesCommand(MapDocument *doc, std::vector<TileChange> changes, const QString &text);
    void apply() override;
    void revert() override;
private:
    std::vector<TileChange> m_changes;
};

// Find & Fix Enclosed Areas: cell types only.
struct CellTypeChange { int x, y, oldType, newType; };
class ChangeCellTypesBulkCommand : public MapCommand
{
public:
    ChangeCellTypesBulkCommand(MapDocument *doc, std::vector<CellTypeChange> changes, const QString &text);
    void apply() override;
    void revert() override;
private:
    std::vector<CellTypeChange> m_changes;
};
