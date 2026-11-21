// TileGroupIO: tile-group .json (+ .bmp) files, compatible with OP2Mapper3 and OP2TileGroupTools.

#include "TileGroupIO.h"

#include "Editor.h"
#include "JsonWriter.h"
#include "MapDocument.h"
#include "MapRenderer.h"
#include "TileImageCache.h"

#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>

#include <cmath>

namespace {

// Header fields are optional; non-string values are shown as text, like Newtonsoft's ToString.
QString headerString(const QJsonObject &h, const char *key)
{
    const QJsonValue v = h.value(QLatin1String(key));
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return QString::number(v.toDouble());
    if (v.isBool())
        return v.toBool() ? QStringLiteral("True") : QStringLiteral("False");
    return QString();
}

// Accepts whole numbers (and numeric strings); anything else makes the group invalid.
bool toInt(const QJsonValue &v, int &out)
{
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (std::floor(d) != d)
            return false;
        out = static_cast<int>(d);
        return true;
    }
    if (v.isString()) {
        bool ok = false;
        out = v.toString().trimmed().toInt(&ok);
        return ok;
    }
    return false;
}

} // namespace

namespace TileGroupIO {

bool load(const QString &jsonPath, TileGroupData &out, Header &header, QString &error)
{
    out = TileGroupData();
    header = Header();
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        error = f.errorString();
        return false;
    }
    QJsonParseError perr;
    const QJsonDocument jdoc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (jdoc.isNull() || !jdoc.isObject()) {
        error = perr.errorString();
        return false;
    }
    const QJsonObject root = jdoc.object();
    const QJsonValue hv = root.value(QStringLiteral("header"));
    if (hv.isObject()) {
        const QJsonObject h = hv.toObject();
        header.tileset = headerString(h, "tileset");
        header.name = headerString(h, "name");
        header.bmp = headerString(h, "bmp");
        header.notes = headerString(h, "notes");
    }
    const QJsonValue tv = root.value(QStringLiteral("tiles"));
    if (!tv.isArray()) {
        error = QStringLiteral("'tiles' is missing");
        return false;
    }
    const QJsonArray rows = tv.toArray();
    if (rows.isEmpty() || !rows.first().isArray()) {
        error = QStringLiteral("'tiles' is not in the expected per-row format");
        return false;
    }
    const int h = rows.size();
    const int w = rows.first().toArray().size();
    if (w <= 0) {
        error = QStringLiteral("'tiles' has an empty row");
        return false;
    }
    std::vector<int> mappings(static_cast<std::size_t>(w * h));
    for (int y = 0; y < h; ++y) {
        if (!rows.at(y).isArray()) {
            error = QStringLiteral("row %1 is not an array").arg(y);
            return false;
        }
        const QJsonArray row = rows.at(y).toArray();
        if (row.size() != w) {
            error = QStringLiteral("row %1 has %2 values, expected %3").arg(y).arg(row.size()).arg(w);
            return false;
        }
        for (int x = 0; x < w; ++x) {
            int v = 0;
            if (!toInt(row.at(x), v)) {
                error = QStringLiteral("non-numeric value in row %1").arg(y);
                return false;
            }
            mappings[static_cast<std::size_t>(y * w + x)] = v;
        }
    }
    out.width = w;
    out.height = h;
    out.mappings = std::move(mappings);
    return true;
}

// Byte-for-byte OP2Mapper3 output; see JsonWriter.h for why it is written by hand.
bool writeJson(const QString &path, int w, int h, const std::vector<int> &mappings,
               const QString &tilesetName, const QString &name, QString &error)
{
    using JsonWriter::escape;
    QStringList out;
    out << QStringLiteral("{");
    out << QStringLiteral("  \"header\": {");
    out << QStringLiteral("    \"width\": %1,").arg(w);
    out << QStringLiteral("    \"height\": %1,").arg(h);
    out << QStringLiteral("    \"tileset\": \"%1\",").arg(escape(tilesetName));
    out << QStringLiteral("    \"name\": \"%1\",").arg(escape(name));
    out << QStringLiteral("    \"bmp\": \"%1.bmp\",").arg(escape(name));
    out << QStringLiteral("    \"notes\": \"Exported from OP2Mapper\"");
    out << QStringLiteral("  },");
    out << QStringLiteral("  \"tiles\": [");
    for (int y = 0; y < h; ++y) {
        QVector<int> row;
        row.reserve(w);
        for (int x = 0; x < w; ++x)
            row.append(mappings[static_cast<std::size_t>(y * w + x)]);
        out << QStringLiteral("    ") + JsonWriter::paddedRow(row, 4)
                 + (y == h - 1 ? QString() : QStringLiteral(","));
    }
    out << QStringLiteral("  ]");
    out << QStringLiteral("}");

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        error = f.errorString();
        return false;
    }
    f.write(JsonWriter::joinCrlf(out).toUtf8());
    if (!f.commit()) {
        error = f.errorString();
        return false;
    }
    return true;
}

// Preview image beside the JSON: always 32 px tiles, whatever zoom the Tile Selection window uses.
bool writeBmp(const QString &path, const MapDocument &doc, int w, int h,
              const std::vector<int> &mappings, QString &error)
{
    constexpr int T = TileImageCache::TileSize;
    QImage img(w * T, h * T, QImage::Format_ARGB32);
    if (img.isNull()) {
        error = QStringLiteral("The image is too large.");
        return false;
    }
    img.fill(Qt::transparent);
    QPainter p(&img);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const QImage tile = MapRenderer::mappingImage(doc, mappings[static_cast<std::size_t>(y * w + x)]);
            if (!tile.isNull())
                p.drawImage(x * T, y * T, tile);
        }
    }
    p.end();
    QImageWriter writer(path, "bmp");
    if (!writer.write(img)) {
        error = writer.errorString();
        return false;
    }
    return true;
}

QString tilesetPrefix(const MapDocument &doc, const std::vector<int> &mappings)
{
    const auto *map = doc.map();
    if (!map)
        return QStringLiteral("well00");
    for (int m : mappings) {
        const int ts = doc.tilesetIndexOfMapping(m);
        if (ts < 0 || static_cast<std::size_t>(ts) >= map->tilesetSources.size())
            continue;
        const QString fn = QString::fromStdString(map->tilesetSources[static_cast<std::size_t>(ts)].tilesetFilename);
        if (fn.size() >= 6)
            return fn.left(6);
    }
    return QStringLiteral("well00");
}

} // namespace TileGroupIO
