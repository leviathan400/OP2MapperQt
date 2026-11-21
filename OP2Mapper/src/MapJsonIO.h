#pragma once

#include <QString>
#include <QStringList>

class MapDocument;
struct MapLoadInfo;

// Full-map JSON I/O. Port of OP2MapJsonTools (.NET shared library).
//
// Schema: { header, tiles[][], cellTypes[][], clipRect?, tileset:
// { sources, tileMappings, terrainTypes } }. The writer is hand-rolled to
// be byte-identical to OP2MapJsonTools' PerRowPadded output (see
// JsonWriter.h and the serialization notes in the .cpp); the reader is a
// normal structural JSON parse and accepts any formatting.
//
// Import asymmetry, inherited from the VB library: a .map can't be built
// from scratch (OP2 needs internal state the JSON doesn't carry), so import
// loads the bundled blank template of matching dimensions and overlays the
// JSON's data onto it. Consequence: only the 10 template sizes are
// importable, and the template's tileMappings/terrainTypes are kept in
// preference to the JSON's copies.
namespace MapJsonIO {

// Writes the document's map as a single .json file. The optional metadata
// strings populate the header object — empty values are accepted.
// Returns true on success; errorOut filled on failure.
bool exportToJson(const MapDocument &doc,
                  const QString &outPath,
                  const QString &mapFileName,  // source .map filename (header.map)
                  const QString &mapName,
                  const QString &author,
                  const QString &notes,
                  QString &errorOut);

// Imports a JSON map into the document, using a blank-map template of the
// matching dimensions as the base (the VB ExportMapFile approach). Tile
// graphics, cell types, clip rect, and tileset sources are overlaid; the
// blank template's tileMappings/terrainTypes are kept.
//
// The whole map is built in memory first, so a failed import leaves the
// open map untouched. Tile values that aren't in the template's mapping
// table and out-of-range cell types are skipped (those tiles keep the
// template's values) and reported through errorOut as a warning while the
// function still returns true. `op2Folder` locates the tilesets; `info`
// receives the tileset loading results.
bool importFromJson(const QString &jsonPath,
                    MapDocument &doc,
                    const QString &op2Folder,
                    MapLoadInfo &info,
                    QString &errorOut); // on success errorOut may hold a warning

} // namespace MapJsonIO
