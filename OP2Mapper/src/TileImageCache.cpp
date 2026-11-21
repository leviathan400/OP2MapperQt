// TileImageCache: tileset graphics for one map.

#include "TileImageCache.h"

#include "CellTypeInfo.h"

#include <OP2Utility.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPainter>

namespace {

// Converts an 8-bit tileset BMP to a top-down ARGB32 strip.
QImage bitmapToImage(const OP2Utility::BitmapFile &bmp)
{
    const int width = bmp.imageHeader.width;
    const int height = static_cast<int>(bmp.AbsoluteHeight());
    if (width <= 0 || height <= 0 || bmp.pixels.empty())
        return {};

    const int pitch = static_cast<int>(bmp.imageHeader.CalculatePitch());
    QImage indexed(reinterpret_cast<const uchar *>(bmp.pixels.data()),
                   width, height, pitch, QImage::Format_Indexed8);

    // OP2 palette entries are BGR: the field named `red` holds blue.
    // Entry 0 is the transparency key when white; OP2Mapper3 shows it grey.
    QList<QRgb> palette;
    palette.reserve(256);
    for (std::size_t i = 0; i < bmp.palette.size() && i < 256; ++i) {
        const auto &c = bmp.palette[i];
        palette.append(qRgb(c.blue, c.green, c.red));
    }
    if (!palette.isEmpty()) {
        const auto &c0 = bmp.palette[0];
        if (c0.red > 250 && c0.green > 250 && c0.blue > 250)
            palette[0] = qRgb(192, 192, 192);
    }
    // Indices outside the palette become transparent black.
    while (palette.size() < 256)
        palette.append(qRgba(0, 0, 0, 0));
    indexed.setColorTable(palette);

    QImage rgb = indexed.convertToFormat(QImage::Format_ARGB32);
    if (bmp.GetScanLineOrientation() == OP2Utility::ScanLineOrientation::BottomUp)
        rgb = rgb.flipped(Qt::Vertical);
    return rgb;
}

} // namespace

TileImageCache::TileImageCache(OP2Utility::ResourceManager *resources,
                               const QString &tilesetDir)
    : m_resources(resources), m_tilesetDir(tilesetDir)
{
}

// Loose file in the tileset folder first, then the ResourceManager (.vol).
// ".bmp" is appended unless the name has it. Sources with the same file name
// share one strip.
QStringList TileImageCache::load(const std::vector<OP2Utility::TilesetSource> &sources)
{
    m_tilesets.clear();
    m_tilesets.resize(sources.size());
    m_scaled.clear();
    m_nonEmptySources = 0;
    m_loadedSources = 0;

    QHash<QString, std::shared_ptr<QImage>> byName;
    QStringList missing;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        const auto &src = sources[i];
        if (src.IsEmpty())
            continue;
        ++m_nonEmptySources;

        QString name = QString::fromStdString(src.tilesetFilename);
        if (!name.endsWith(QLatin1String(".bmp"), Qt::CaseInsensitive))
            name += QStringLiteral(".bmp");
        const QString key = name.toLower();
        if (byName.contains(key)) {
            m_tilesets[i] = byName.value(key);
            if (m_tilesets[i])
                ++m_loadedSources;
            else
                missing.append(name);
            continue;
        }

        std::shared_ptr<QImage> strip;
        try {
            OP2Utility::BitmapFile bmp;
            bool have = false;
            if (!m_tilesetDir.isEmpty()) {
                QFile loose(QDir(m_tilesetDir).filePath(name));
                if (loose.open(QIODevice::ReadOnly)) {
                    const QByteArray bytes = loose.readAll();
                    OP2Utility::Stream::MemoryReader reader(bytes.constData(),
                        static_cast<std::size_t>(bytes.size()));
                    bmp = OP2Utility::Tileset::ReadTileset(reader);
                    have = true;
                }
            }
            if (!have && m_resources) {
                auto stream = m_resources->GetResourceStream(name.toStdString());
                if (stream) {
                    bmp = OP2Utility::Tileset::ReadTileset(*stream);
                    have = true;
                }
            }
            if (have) {
                QImage img = bitmapToImage(bmp);
                if (img.height() >= TileSize)
                    strip = std::make_shared<QImage>(std::move(img));
            }
        } catch (const std::exception &e) {
            qWarning() << "Failed to load tileset" << name << ":" << e.what();
        }
        byName.insert(key, strip);
        m_tilesets[i] = strip;
        if (strip)
            ++m_loadedSources;
        else
            missing.append(name);
    }
    return missing;
}

const QImage *TileImageCache::tileset(std::size_t tilesetIndex) const
{
    if (tilesetIndex >= m_tilesets.size())
        return nullptr;
    return m_tilesets[tilesetIndex].get();
}

int TileImageCache::tileCount(std::size_t tilesetIndex) const
{
    const QImage *s = tileset(tilesetIndex);
    return s ? s->height() / TileSize : 0;
}

QImage TileImageCache::tile(std::size_t tilesetIndex, std::size_t imageIndex) const
{
    const QImage *ts = tileset(tilesetIndex);
    if (!ts)
        return {};
    const int y = static_cast<int>(imageIndex) * TileSize;
    if (y < 0 || y + TileSize > ts->height())
        return {};
    return ts->copy(0, y, TileSize, TileSize);
}

// Scaled once per tile size and kept, so zoomed drawing is a plain blit.
const QPixmap *TileImageCache::scaledStrip(std::size_t tilesetIndex, int tilePx) const
{
    const QImage *ts = tileset(tilesetIndex);
    if (!ts || tilePx <= 0)
        return nullptr;
    const quint64 key = (static_cast<quint64>(reinterpret_cast<quintptr>(ts)) << 8)
                      ^ static_cast<quint64>(tilePx);
    auto it = m_scaled.constFind(key);
    if (it != m_scaled.constEnd())
        return &it.value();
    const int n = ts->height() / TileSize;
    QPixmap pm = tilePx == TileSize
        ? QPixmap::fromImage(*ts)
        : QPixmap::fromImage(ts->scaled(tilePx, n * tilePx, Qt::IgnoreAspectRatio,
                                        Qt::FastTransformation));
    return &m_scaled.insert(key, pm).value();
}

// OP2Mapper3's placeholder for a missing graphic: the Basic cell-type colour with a grey frame and X.
const QPixmap &TileImageCache::fallbackTile(int cellType, int tilePx) const
{
    const quint32 key = (static_cast<quint32>(cellType & 0xFF) << 16) | static_cast<quint32>(tilePx & 0xFFFF);
    auto it = m_fallback.constFind(key);
    if (it != m_fallback.constEnd())
        return it.value();
    QImage img(TileSize, TileSize, QImage::Format_ARGB32);
    img.fill(CellTypeInfo::basicColor(cellType));
    {
        QPainter p(&img);
        p.setPen(QColor(169, 169, 169));
        p.drawRect(0, 0, TileSize - 1, TileSize - 1);
        p.drawLine(0, 0, TileSize - 1, TileSize - 1);
        p.drawLine(0, TileSize - 1, TileSize - 1, 0);
    }
    QPixmap pm = QPixmap::fromImage(tilePx == TileSize ? img
        : img.scaled(tilePx, tilePx, Qt::IgnoreAspectRatio, Qt::FastTransformation));
    return m_fallback.insert(key, pm).value();
}
