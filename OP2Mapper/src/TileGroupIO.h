#pragma once

#include <QString>
#include <vector>

class MapDocument;
struct TileGroupData;

// Tile-group files (.json + optional .bmp preview), the format shared with
// OP2Mapper3 and OP2TileGroupTools. The writer emits the VB PerRowPadded
// bytes (key order, 2-space indent, CRLF, no BOM, no trailing newline).
namespace TileGroupIO {

struct Header {
    QString tileset;
    QString name;
    QString bmp;
    QString notes;
};

// OP2Mapper3 rules: only "tiles" is required; it must be a non-empty array
// of rows that are all arrays of the same non-zero length, every value an
// integer. Header fields are optional (empty when missing).
bool load(const QString &jsonPath, TileGroupData &out, Header &header, QString &error);

bool writeJson(const QString &path, int w, int h, const std::vector<int> &mappings,
               const QString &tilesetName, const QString &name, QString &error);

// 32 px per tile, transparent background, tiles only.
bool writeBmp(const QString &path, const MapDocument &doc, int w, int h,
              const std::vector<int> &mappings, QString &error);

// First 6 chars of the first usable tileset filename (>= 6 chars), else "well00".
QString tilesetPrefix(const MapDocument &doc, const std::vector<int> &mappings);

} // namespace TileGroupIO
