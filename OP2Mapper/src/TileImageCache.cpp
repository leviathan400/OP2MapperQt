#include "TileImageCache.h"

#include <OP2Utility.h>

#include <QDebug>

namespace {

QImage bitmapToImage(const OP2Utility::BitmapFile &bmp)
{
    const int width = bmp.imageHeader.width;
    const int height = static_cast<int>(bmp.AbsoluteHeight());
    if (width <= 0 || height <= 0 || bmp.pixels.empty())
        return {};

    const int pitch = static_cast<int>(bmp.imageHeader.CalculatePitch());

    // Build 8-bit indexed image referencing the source bytes, then convert+copy
    // to ARGB32 so we own the data and the painter is happy.
    QImage indexed(reinterpret_cast<const uchar *>(bmp.pixels.data()),
                   width, height, pitch, QImage::Format_Indexed8);

    QList<QRgb> palette;
    palette.reserve(static_cast<int>(bmp.palette.size()));
    for (const auto &c : bmp.palette)
        palette.append(qRgb(c.red, c.green, c.blue));
    indexed.setColorTable(palette);

    QImage rgb = indexed.convertToFormat(QImage::Format_ARGB32);

    // BMP scan order: bottom-up needs flipping for natural top-left origin.
    if (bmp.GetScanLineOrientation() == OP2Utility::ScanLineOrientation::BottomUp)
        rgb = rgb.flipped(Qt::Vertical);

    return rgb;
}

}

TileImageCache::TileImageCache(OP2Utility::ResourceManager &resources)
    : m_resources(resources)
{
}

QStringList TileImageCache::load(const std::vector<OP2Utility::TilesetSource> &sources)
{
    m_tilesets.clear();
    m_tilesets.resize(sources.size());

    QStringList missing;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        const auto &src = sources[i];
        if (src.IsEmpty())
            continue;

        const std::string filename = src.tilesetFilename + ".bmp";
        auto stream = m_resources.GetResourceStream(filename);
        if (!stream) {
            missing.append(QString::fromStdString(filename));
            continue;
        }

        try {
            OP2Utility::BitmapFile bmp = OP2Utility::Tileset::ReadTileset(*stream);
            QImage img = bitmapToImage(bmp);
            if (img.isNull()) {
                missing.append(QString::fromStdString(filename));
                continue;
            }
            m_tilesets[i] = std::make_unique<QImage>(std::move(img));
        } catch (const std::exception &e) {
            qWarning() << "Failed to load tileset" << QString::fromStdString(filename)
                       << ":" << e.what();
            missing.append(QString::fromStdString(filename));
        }
    }
    return missing;
}

const QImage *TileImageCache::tileset(std::size_t tilesetIndex) const
{
    if (tilesetIndex >= m_tilesets.size())
        return nullptr;
    return m_tilesets[tilesetIndex].get();
}

QImage TileImageCache::tile(std::size_t tilesetIndex, std::size_t imageIndex) const
{
    const QImage *ts = tileset(tilesetIndex);
    if (!ts)
        return {};
    const int y = static_cast<int>(imageIndex) * TileSize;
    if (y + TileSize > ts->height())
        return {};
    return ts->copy(0, y, TileSize, TileSize);
}
