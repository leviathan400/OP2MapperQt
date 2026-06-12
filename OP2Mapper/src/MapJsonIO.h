#pragma once

#include <QString>
#include <QStringList>

class MapDocument;

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
// `op2Folder` is needed because loading the blank requires a ResourceManager.
// `missingTilesets` reports any tileset names the destination map can't load.
bool importFromJson(const QString &jsonPath,
                    MapDocument &doc,
                    const QString &op2Folder,
                    QStringList &missingTilesets,
                    QString &errorOut);

} // namespace MapJsonIO
