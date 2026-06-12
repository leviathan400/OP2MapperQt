#pragma once

#include <QString>

class MapDocument;
class TileClipboard;

// Read/write tile-group files. A tile group is a paired (.json + .bmp)
// representing a small rectangular region of a map plus its rendered preview.
//
// The JSON is shared community currency — OP2Mapper3, OP2TileGroupTools
// (https://github.com/leviathan400/OP2TileGroupTools — the format's
// reference documentation), and this app all read/write the same files —
// so the writer emits bytes identical to the VB tools' PerRowPadded
// output (key order, indent, CRLF; see JsonWriter.h). The reader only
// needs structural JSON, so any tool's output parses fine regardless of
// formatting. Note OP2TileGroupTools exports groups embedded in .map
// files and writes "bmp": "" with no preview image — the .bmp half of
// the pair is optional on read.
//
// Tiles are stored as RAW tileMappingIndex values from the source map.
// They're only meaningful against a map with a compatible tileMappings
// table (in practice: the standard well00XX tilesets) — the loader
// re-resolves them against the destination map and skips any that don't
// exist there.
namespace TileGroupIO {

// Writes <outDir>/<name>.json + <outDir>/<name>.bmp. Returns true on success.
// On failure, errorOut is populated and any partially-written file is left in
// place — caller is responsible for retry/cleanup if desired.
bool writeExport(const TileClipboard &clip,
                 const MapDocument &doc,
                 const QString &outDir,
                 const QString &name,
                 QString &errorOut);

// Reads jsonPath, resolves each raw tileMappingIndex through the current
// document's tileMappings table to fill (tilesetIdx, imageIdx) pairs, and
// populates the clipboard. Cell types stay untouched on paste — sets
// clip.hasCellTypes(false). Cells whose mappingIndex is invalid in the
// destination map become "skip" cells. Returns false on parse error.
bool loadIntoClipboard(const QString &jsonPath,
                       const MapDocument &doc,
                       TileClipboard &clipOut,
                       QString &errorOut);

} // namespace TileGroupIO
