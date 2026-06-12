#include "TileGroupIO.h"

#include "JsonWriter.h"
#include "MapDocument.h"
#include "TileClipboard.h"
#include "TileImageCache.h"

#include <OP2Utility.h>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

namespace {

// Emits the tile-group JSON by hand so the bytes match what OP2Mapper3's
// fTileSelect.vb ExportTileGroupToJson produces (PerRowPadded): exact key
// order, 2-space indent, CRLF, no BOM, no trailing newline. See JsonWriter.h
// for why QJsonDocument can't be used (it alphabetizes keys).
bool writeJson(const TileClipboard &clip, const QString &name,
               const QString &path, QString &errorOut)
{
    using JsonWriter::escape;

    QStringList out;
    out << QStringLiteral("{");

    // Header — key order is the VB writer's insertion order, not alphabetical.
    out << QStringLiteral("  \"header\": {");
    out << QStringLiteral("    \"width\": %1,").arg(clip.width());
    out << QStringLiteral("    \"height\": %1,").arg(clip.height());
    out << QStringLiteral("    \"tileset\": \"%1\",").arg(escape(clip.sourceTilesetName()));
    out << QStringLiteral("    \"name\": \"%1\",").arg(escape(name));
    out << QStringLiteral("    \"bmp\": \"%1.bmp\",").arg(escape(name));
    out << QStringLiteral("    \"notes\": \"Exported from OP2Mapper\"");
    out << QStringLiteral("  },");

    // Tiles — one padded row per line, raw tileMappingIndex values.
    // minPad 4 matches VB PadNumbersInRow's hardcoded default.
    out << QStringLiteral("  \"tiles\": [");
    for (int y = 0; y < clip.height(); ++y) {
        QVector<int> row;
        row.reserve(clip.width());
        for (int x = 0; x < clip.width(); ++x)
            row.append(static_cast<int>(clip.at(x, y).originalMappingIdx));
        const bool last = (y == clip.height() - 1);
        out << QStringLiteral("    ") + JsonWriter::paddedRow(row, 4)
                 + (last ? QString() : QStringLiteral(","));
    }
    out << QStringLiteral("  ]");
    out << QStringLiteral("}");

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorOut = QStringLiteral("Failed to open %1: %2").arg(path, f.errorString());
        return false;
    }
    f.write(JsonWriter::joinCrlf(out).toUtf8());
    f.close();
    return true;
}

bool writeBmp(const TileClipboard &clip, const MapDocument &doc,
              const QString &path, QString &errorOut)
{
    constexpr int T = TileImageCache::TileSize;
    QImage img(clip.width() * T, clip.height() * T, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    QPainter p(&img);
    for (int y = 0; y < clip.height(); ++y) {
        for (int x = 0; x < clip.width(); ++x) {
            const auto &c = clip.at(x, y);
            if (c.tilesetIdx < 0 || c.imageIdx < 0)
                continue;
            const QImage *strip = doc.cache()
                ? doc.cache()->tileset(static_cast<std::size_t>(c.tilesetIdx))
                : nullptr;
            if (!strip)
                continue;
            const int srcY = c.imageIdx * T;
            if (srcY + T > strip->height())
                continue;
            p.drawImage(QPoint(x * T, y * T), *strip, QRect(0, srcY, T, T));
        }
    }
    p.end();

    if (!img.save(path, "BMP")) {
        errorOut = QStringLiteral("Failed to write BMP: %1").arg(path);
        return false;
    }
    return true;
}

} // namespace

namespace TileGroupIO {

bool writeExport(const TileClipboard &clip,
                 const MapDocument &doc,
                 const QString &outDir,
                 const QString &name,
                 QString &errorOut)
{
    if (clip.isEmpty()) {
        errorOut = QStringLiteral("Clipboard is empty");
        return false;
    }
    if (name.isEmpty()) {
        errorOut = QStringLiteral("Tile group name is required");
        return false;
    }

    QDir().mkpath(outDir);
    const QString jsonPath = QDir(outDir).filePath(name + QStringLiteral(".json"));
    const QString bmpPath = QDir(outDir).filePath(name + QStringLiteral(".bmp"));

    if (!writeJson(clip, name, jsonPath, errorOut))
        return false;
    if (!writeBmp(clip, doc, bmpPath, errorOut))
        return false;
    return true;
}

bool loadIntoClipboard(const QString &jsonPath,
                       const MapDocument &doc,
                       TileClipboard &clipOut,
                       QString &errorOut)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        errorOut = QStringLiteral("Cannot open: %1").arg(f.errorString());
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

    if (!root.contains(QStringLiteral("header"))
        || !root.contains(QStringLiteral("tiles"))) {
        errorOut = QStringLiteral("Missing 'header' or 'tiles'");
        return false;
    }
    const QJsonObject header = root.value(QStringLiteral("header")).toObject();
    const QJsonArray rows = root.value(QStringLiteral("tiles")).toArray();
    if (rows.isEmpty() || !rows.first().isArray()) {
        errorOut = QStringLiteral("'tiles' is not a 2D array");
        return false;
    }

    const int h = rows.size();
    const int w = rows.first().toArray().size();
    if (w <= 0 || h <= 0) {
        errorOut = QStringLiteral("Empty tile-group region");
        return false;
    }

    const QString tilesetName = header.value(QStringLiteral("tileset"))
                                    .toString(QStringLiteral("well00"));
    clipOut.resetForRegion(w, h, tilesetName);

    const auto *map = doc.map();
    for (int y = 0; y < h; ++y) {
        const QJsonArray row = rows.at(y).toArray();
        for (int x = 0; x < w && x < row.size(); ++x) {
            const unsigned mapIdx = static_cast<unsigned>(row.at(x).toInt());
            auto &cell = clipOut.mutableCell(x, y);
            cell.originalMappingIdx = mapIdx;
            if (map && mapIdx < map->tileMappings.size()) {
                const auto &m = map->tileMappings[mapIdx];
                cell.tilesetIdx = static_cast<int>(m.tilesetIndex);
                cell.imageIdx = static_cast<int>(m.tileGraphicIndex);
            } else {
                cell.tilesetIdx = -1; // unresolved → skip on paste
                cell.imageIdx = -1;
            }
            cell.cellType = 0; // tile groups don't carry cell types
        }
    }
    return true;
}

} // namespace TileGroupIO
