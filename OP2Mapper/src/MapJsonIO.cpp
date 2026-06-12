#include "MapJsonIO.h"

#include "BlankMaps.h"
#include "JsonWriter.h"
#include "MapDocument.h"

#include <OP2Utility.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

namespace {

// The default in-game visible area: x offset 32 for maps narrower than 512
// (OP2 pads narrow maps), and the bottom border row excluded. A clipRect
// matching this is omitted from the JSON, mirroring OP2MapJsonTools.
bool isDefaultClipRect(const OP2Utility::Rect &r, int width, int height)
{
    const int x = width < 512 ? 32 : 0;
    return r.x1 == x && r.y1 == 0
        && r.x2 == x + width - 1 && r.y2 == height - 2;
}

// ---- Hand-rolled serialization -----------------------------------------
//
// The export is emitted line-by-line rather than via QJsonDocument because
// the on-disk format must be byte-identical to OP2MapJsonTools' PerRowPadded
// output (see JsonWriter.h). That reference output is Newtonsoft.Json
// 2-space-indented JSON post-processed by ProcessJsonArraysWithPadding,
// which collapses "tiles" rows (pad 4) and "cellTypes" rows (pad 2) onto
// single lines. One quirk is replicated deliberately: the VB post-pass
// never exits its "cellTypes" scope (its section-end check compares against
// an unindented "],"), so by the time the tileset section streams past it
// ALSO collapses the bare inner arrays of wallTileMappingIndexes at pad 2.
// Community files in the wild have that shape, so we emit it too.

// Emits a fixed-size numeric array in Newtonsoft multiline form:
//   <indent>"key": [
//   <indent>  16,
//   ...
//   <indent>],
QStringList numberArrayLines(const QString &indent, const QString &key,
                             const QVector<int> &values, bool trailingComma)
{
    QStringList out;
    out << indent + QStringLiteral("\"%1\": [").arg(key);
    for (int i = 0; i < values.size(); ++i) {
        out << indent + QStringLiteral("  %1%2")
                   .arg(values[i])
                   .arg(i + 1 < values.size() ? QStringLiteral(",") : QString());
    }
    out << indent + QStringLiteral("]") + (trailingComma ? QStringLiteral(",") : QString());
    return out;
}

void emitHeader(QStringList &out, const MapDocument &doc,
                const QString &mapFileName, const QString &mapName,
                const QString &author, const QString &notes)
{
    using JsonWriter::escape;
    out << QStringLiteral("  \"header\": {");
    out << QStringLiteral("    \"width\": %1,").arg(doc.widthTiles());
    out << QStringLiteral("    \"height\": %1,").arg(doc.heightTiles());
    out << QStringLiteral("    \"map\": \"%1\",").arg(escape(mapFileName));
    out << QStringLiteral("    \"name\": \"%1\",").arg(escape(mapName));
    out << QStringLiteral("    \"author\": \"%1\",").arg(escape(author));
    out << QStringLiteral("    \"notes\": \"%1\"").arg(escape(notes));
    out << QStringLiteral("  },");
}

// "tiles" and "cellTypes" share a shape: one padded single-line row per map
// row. Only the value source and the minimum pad width differ (4 vs 2).
template <typename ValueAt>
void emitPaddedRowsSection(QStringList &out, const QString &key, int minPad,
                           int w, int h, ValueAt valueAt, bool trailingComma)
{
    out << QStringLiteral("  \"%1\": [").arg(key);
    for (int y = 0; y < h; ++y) {
        QVector<int> row;
        row.reserve(w);
        for (int x = 0; x < w; ++x)
            row.append(valueAt(x, y));
        const bool last = (y == h - 1);
        out << QStringLiteral("    ") + JsonWriter::paddedRow(row, minPad)
                 + (last ? QString() : QStringLiteral(","));
    }
    out << QStringLiteral("  ]") + (trailingComma ? QStringLiteral(",") : QString());
}

void emitTileset(QStringList &out, const OP2Utility::Map &map)
{
    using JsonWriter::escape;

    out << QStringLiteral("  \"tileset\": {");

    // 5.1 sources
    out << QStringLiteral("    \"sources\": [");
    for (std::size_t i = 0; i < map.tilesetSources.size(); ++i) {
        const auto &s = map.tilesetSources[i];
        out << QStringLiteral("      {");
        out << QStringLiteral("        \"filename\": \"%1\",")
                   .arg(escape(QString::fromStdString(s.tilesetFilename)));
        out << QStringLiteral("        \"numTiles\": %1").arg(s.numTiles);
        out << QStringLiteral("      }%1")
                   .arg(i + 1 < map.tilesetSources.size()
                            ? QStringLiteral(",") : QString());
    }
    out << QStringLiteral("    ],");

    // 5.2 tileMappings
    out << QStringLiteral("    \"tileMappings\": [");
    for (std::size_t i = 0; i < map.tileMappings.size(); ++i) {
        const auto &m = map.tileMappings[i];
        out << QStringLiteral("      {");
        out << QStringLiteral("        \"tilesetIndex\": %1,").arg(m.tilesetIndex);
        out << QStringLiteral("        \"tileGraphicIndex\": %1,").arg(m.tileGraphicIndex);
        out << QStringLiteral("        \"animationCount\": %1,").arg(m.animationCount);
        out << QStringLiteral("        \"animationDelay\": %1").arg(m.animationDelay);
        out << QStringLiteral("      }%1")
                   .arg(i + 1 < map.tileMappings.size()
                            ? QStringLiteral(",") : QString());
    }
    out << QStringLiteral("    ],");

    // 5.3 terrainTypes
    out << QStringLiteral("    \"terrainTypes\": [");
    for (std::size_t i = 0; i < map.terrainTypes.size(); ++i) {
        const auto &t = map.terrainTypes[i];
        out << QStringLiteral("      {");

        out << QStringLiteral("        \"tileRange\": {");
        out << QStringLiteral("          \"start\": %1,").arg(t.tileMappingRange.start);
        out << QStringLiteral("          \"end\": %1").arg(t.tileMappingRange.end);
        out << QStringLiteral("        },");

        out << QStringLiteral("        \"bulldozedTileMappingIndex\": %1,")
                   .arg(t.bulldozedTileMappingIndex);
        out << QStringLiteral("        \"rubbleTileMappingIndex\": %1,")
                   .arg(t.rubbleTileMappingIndex);

        QVector<int> tubes;
        for (auto v : t.tubeTileMappings) tubes.append(v);
        out << numberArrayLines(QStringLiteral("        "),
                                QStringLiteral("tubeTileMappings"), tubes, true);

        // Wall groups — single-line padded rows (min pad 2): the deliberate
        // replication of the VB post-pass quirk described above.
        out << QStringLiteral("        \"wallTileMappingIndexes\": [");
        for (int g = 0; g < 5; ++g) {
            QVector<int> group;
            group.reserve(16);
            for (int j = 0; j < 16; ++j)
                group.append(t.wallTileMappingIndexes[g][j]);
            out << QStringLiteral("          ") + JsonWriter::paddedRow(group, 2)
                     + (g < 4 ? QStringLiteral(",") : QString());
        }
        out << QStringLiteral("        ],");

        out << QStringLiteral("        \"lavaTileMappingIndex\": %1,")
                   .arg(t.lavaTileMappingIndex);
        out << QStringLiteral("        \"flat1\": %1,").arg(t.flat1);
        out << QStringLiteral("        \"flat2\": %1,").arg(t.flat2);
        out << QStringLiteral("        \"flat3\": %1,").arg(t.flat3);

        QVector<int> tubeIdx;
        for (auto v : t.tubeTileMappingIndexes) tubeIdx.append(v);
        out << numberArrayLines(QStringLiteral("        "),
                                QStringLiteral("tubeTileMappingIndexes"), tubeIdx, true);

        out << QStringLiteral("        \"scorchedTileMappingIndex\": %1,")
                   .arg(t.scorchedTileMappingIndex);

        out << QStringLiteral("        \"scorchedRange\": [");
        for (int r = 0; r < 3; ++r) {
            out << QStringLiteral("          {");
            out << QStringLiteral("            \"start\": %1,").arg(t.scorchedRange[r].start);
            out << QStringLiteral("            \"end\": %1").arg(t.scorchedRange[r].end);
            out << QStringLiteral("          }%1")
                       .arg(r < 2 ? QStringLiteral(",") : QString());
        }
        out << QStringLiteral("        ],");

        QVector<int> unknown;
        for (auto v : t.unknown) unknown.append(v);
        out << numberArrayLines(QStringLiteral("        "),
                                QStringLiteral("unknown"), unknown, false);

        out << QStringLiteral("      }%1")
                   .arg(i + 1 < map.terrainTypes.size()
                            ? QStringLiteral(",") : QString());
    }
    out << QStringLiteral("    ]");

    out << QStringLiteral("  }");
}

} // namespace

namespace MapJsonIO {

bool exportToJson(const MapDocument &doc,
                  const QString &outPath,
                  const QString &mapFileName,
                  const QString &mapName,
                  const QString &author,
                  const QString &notes,
                  QString &errorOut)
{
    if (!doc.isLoaded() || !doc.map()) {
        errorOut = QStringLiteral("No map loaded");
        return false;
    }
    const auto *map = doc.map();
    const int w = doc.widthTiles();
    const int h = doc.heightTiles();

    // Section order is fixed by the reference format: header, tiles,
    // cellTypes, clipRect (only when non-default), tileset.
    QStringList out;
    out << QStringLiteral("{");

    emitHeader(out, doc, mapFileName, mapName, author, notes);

    emitPaddedRowsSection(out, QStringLiteral("tiles"), 4, w, h,
        [&doc](int x, int y) { return static_cast<int>(doc.mappingIndexAt(x, y)); },
        /*trailingComma=*/true);

    emitPaddedRowsSection(out, QStringLiteral("cellTypes"), 2, w, h,
        [&doc](int x, int y) { return static_cast<int>(doc.cellTypeAt(x, y)); },
        /*trailingComma=*/true);

    if (!isDefaultClipRect(map->clipRect, w, h)) {
        out << QStringLiteral("  \"clipRect\": {");
        out << QStringLiteral("    \"x1\": %1,").arg(map->clipRect.x1);
        out << QStringLiteral("    \"y1\": %1,").arg(map->clipRect.y1);
        out << QStringLiteral("    \"x2\": %1,").arg(map->clipRect.x2);
        out << QStringLiteral("    \"y2\": %1").arg(map->clipRect.y2);
        out << QStringLiteral("  },");
    }

    emitTileset(out, *map);

    out << QStringLiteral("}");

    QFile f(outPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorOut = QStringLiteral("Cannot open %1: %2")
                       .arg(outPath, f.errorString());
        return false;
    }
    f.write(JsonWriter::joinCrlf(out).toUtf8());
    f.close();
    return true;
}

bool importFromJson(const QString &jsonPath,
                    MapDocument &doc,
                    const QString &op2Folder,
                    QStringList &missingTilesets,
                    QString &errorOut)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        errorOut = QStringLiteral("Cannot open %1: %2").arg(jsonPath, f.errorString());
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError perr;
    QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &perr);
    if (jdoc.isNull() || !jdoc.isObject()) {
        errorOut = QStringLiteral("JSON parse error: %1").arg(perr.errorString());
        return false;
    }
    const QJsonObject root = jdoc.object();
    if (!root.contains(QStringLiteral("header"))) {
        errorOut = QStringLiteral("Missing 'header'");
        return false;
    }
    const QJsonObject header = root.value(QStringLiteral("header")).toObject();
    const int w = header.value(QStringLiteral("width")).toInt();
    const int h = header.value(QStringLiteral("height")).toInt();

    const QString blankPath = BlankMaps::pathFor(w, h);
    if (blankPath.isEmpty()) {
        errorOut = QStringLiteral("No blank-map template for %1×%2").arg(w).arg(h);
        return false;
    }

    if (!doc.load(blankPath, op2Folder, errorOut, missingTilesets))
        return false;

    // Note: we don't capture undo entries for import — it's a fresh load.
    // Switching to direct manipulation via the OP2Utility map.
    auto *map = const_cast<OP2Utility::Map *>(doc.map());
    if (!map) {
        errorOut = QStringLiteral("Blank map didn't load");
        return false;
    }

    // Overlay tiles
    if (root.contains(QStringLiteral("tiles"))) {
        const QJsonArray rows = root.value(QStringLiteral("tiles")).toArray();
        for (int y = 0; y < rows.size() && y < h; ++y) {
            const QJsonArray row = rows.at(y).toArray();
            for (int x = 0; x < row.size() && x < w; ++x) {
                doc.setMappingIndexAt(x, y,
                    static_cast<unsigned>(row.at(x).toInt()));
            }
        }
    }

    // Overlay cell types
    if (root.contains(QStringLiteral("cellTypes"))) {
        const QJsonArray rows = root.value(QStringLiteral("cellTypes")).toArray();
        for (int y = 0; y < rows.size() && y < h; ++y) {
            const QJsonArray row = rows.at(y).toArray();
            for (int x = 0; x < row.size() && x < w; ++x) {
                doc.setCellTypeAt(x, y,
                    static_cast<OP2Utility::CellType>(row.at(x).toInt()));
            }
        }
    }

    // Clip rect
    if (root.contains(QStringLiteral("clipRect"))) {
        const QJsonObject c = root.value(QStringLiteral("clipRect")).toObject();
        map->clipRect.x1 = c.value(QStringLiteral("x1")).toInt();
        map->clipRect.y1 = c.value(QStringLiteral("y1")).toInt();
        map->clipRect.x2 = c.value(QStringLiteral("x2")).toInt();
        map->clipRect.y2 = c.value(QStringLiteral("y2")).toInt();
    }

    // Tileset sources only — keep template's tileMappings/terrainTypes.
    if (root.contains(QStringLiteral("tileset"))) {
        const QJsonObject ts = root.value(QStringLiteral("tileset")).toObject();
        if (ts.contains(QStringLiteral("sources"))) {
            const QJsonArray sources = ts.value(QStringLiteral("sources")).toArray();
            map->tilesetSources.clear();
            for (const auto &v : sources) {
                const QJsonObject o = v.toObject();
                OP2Utility::TilesetSource src;
                src.tilesetFilename = o.value(QStringLiteral("filename")).toString().toStdString();
                src.numTiles = static_cast<uint32_t>(
                    o.value(QStringLiteral("numTiles")).toInt());
                map->tilesetSources.push_back(src);
            }
        }
    }

    return true;
}

} // namespace MapJsonIO
