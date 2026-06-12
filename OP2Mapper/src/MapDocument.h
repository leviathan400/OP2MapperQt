#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

namespace OP2Utility { class Map; class ResourceManager; enum class CellType; }

class TileClipboard;
class TileImageCache;
class QUndoStack;

// The document: owns the loaded OP2Utility::Map plus everything whose
// lifetime is tied to it (ResourceManager for .vol/.bmp access, the
// TileImageCache of decoded tileset graphics, the QUndoStack).
//
// Mutation model — three layers, strictly ordered:
//   1. set*/apply* methods write the map directly and emit tileChanged.
//      No undo recording. Only QUndoCommands and bulk importers call these.
//   2. push* methods wrap an edit in a QUndoCommand and put it on the
//      stack. All interactive editing goes through these.
//   3. would*Change queries are the no-op predicates for layer 2, exposed
//      so MapView can decide whether to open a stroke macro BEFORE any
//      command exists (see MapView's stroke-lifecycle comments).
//
// Dirty state is not a flag — it's derived from QUndoStack cleanliness
// (isDirty() == !stack->isClean()), so undoing back to the last save
// correctly clears the title-bar marker.
//
// Storage quirk inherited from the .map format: tiles are NOT row-major.
// Maps store tiles in 32-tile-wide column blocks; see tileIndex() in the
// .cpp for the index math. All public APIs take ordinary (x, y).
class MapDocument : public QObject
{
    Q_OBJECT
public:
    explicit MapDocument(QObject *parent = nullptr);
    ~MapDocument() override;

    // Loads the map and its tilesets. Returns true on success.
    // On success, missingTilesets contains tileset filenames that couldn't be resolved.
    bool load(const QString &mapPath, const QString &op2Folder,
              QString &errorOut, QStringList &missingTilesets);

    // Closes the currently-loaded map and clears the undo stack.
    // The QUndoStack itself is preserved so any QActions bound to it stay live.
    void reset();

    bool isLoaded() const { return m_map != nullptr; }
    const OP2Utility::Map *map() const { return m_map.get(); }
    const TileImageCache *cache() const { return m_cache.get(); }
    QUndoStack *undoStack() const { return m_undoStack; }

    int widthTiles() const;
    int heightTiles() const;

    // Reads — 0-based coords. Safe to call when not loaded.
    OP2Utility::CellType cellTypeAt(int x, int y) const;
    unsigned mappingIndexAt(int x, int y) const;

    // Low-level writes (no undo recording). Emit tileChanged. Used by commands.
    void setMappingIndexAt(int x, int y, unsigned mappingIndex);
    void setCellTypeAt(int x, int y, OP2Utility::CellType ct);

    // Auto-insert variant: finds-or-inserts a TileMapping for (tilesetIdx, imageIdx),
    // sets the tile to point at it, emits tileChanged. Returns true if anything
    // changed. Used by PaintTileCommand on redo.
    bool applyPaintTileGraphic(int x, int y, int tilesetIdx, int imageIdx);

    // Pre-flight checks: true if the corresponding push* call would actually
    // change the map (in bounds, not the protected bottom row, value differs).
    // MapView uses these to open its stroke macro lazily — a QUndoStack macro
    // that ends up empty still lands on the stack as a visible no-op undo
    // entry AND flips the clean flag (spurious "unsaved changes"), so the
    // macro must not be opened until a real command is guaranteed to follow.
    bool wouldPaintTileChange(int x, int y, int tilesetIdx, int imageIdx) const;
    bool wouldPaintCellTypeChange(int x, int y, OP2Utility::CellType ct) const;

    // Push undoable commands onto the stack. No-ops if nothing would change
    // (same predicate as the would*Change queries above).
    void pushPaintTile(int x, int y, int tilesetIdx, int imageIdx);
    void pushPaintCellType(int x, int y, OP2Utility::CellType ct);

    // Places the clipboard's tiles + cell types with their top-left at (x0, y0).
    // The whole placement is one undoable command. Clipped to map bounds.
    void pushPlaceTiles(int x0, int y0, const TileClipboard &clip);

    // Persistence
    bool save(const QString &path, QString &errorOut);
    QString currentPath() const { return m_currentPath; }
    bool isDirty() const;
    void setCurrentPath(const QString &path) { m_currentPath = path; }

signals:
    // (x, y) in 0-based tile coords. Emitted whenever the tile's mapping
    // index or cell type changes. Listeners: MapScene (invalidate),
    // MainWindow (title bar refresh), etc.
    void tileChanged(int x, int y);

    // Mirrors QUndoStack::cleanChanged inverted — emitted as a convenience
    // so views can update window-title dirty markers without reaching into
    // the stack themselves.
    void dirtyChanged(bool dirty);

private:
    std::unique_ptr<OP2Utility::ResourceManager> m_resources;
    std::unique_ptr<OP2Utility::Map> m_map;
    std::unique_ptr<TileImageCache> m_cache;
    QUndoStack *m_undoStack = nullptr;
    QString m_currentPath;
};
