// Op2Lib: C++ wrapper over op2maplib's C API.

#include "Op2Lib.h"

#include "AppLog.h"

#include <op2maplib/op2maplib.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <algorithm>

namespace {
om_profile *g_profile = nullptr;
bool g_tried = false;
QString g_error;
} // namespace

namespace Op2Lib {

QString libraryVersion() { return QString::fromUtf8(om_version()); }

// Loaded on first use. An op2maplib/op2.json next to the exe overrides the
// embedded profile (handy for testing a new profile without rebuilding).
const om_profile *profile()
{
    if (g_tried)
        return g_profile;
    g_tried = true;

    QByteArray json;
    const QString override = QDir(QCoreApplication::applicationDirPath())
                                 .filePath(QStringLiteral("op2maplib/op2.json"));
    for (const QString &path : { override, QStringLiteral(":/op2maplib/op2.json") }) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            json = f.readAll();
            break;
        }
    }
    if (json.isEmpty()) {
        g_error = QStringLiteral("the blend profile op2maplib/op2.json could not be read.");
        AppLog::write(QStringLiteral("op2maplib"), QStringLiteral("Not available: ") + g_error);
        return nullptr;
    }
    char err[512] = {};
    g_profile = om_profile_load(json.constData(), static_cast<size_t>(json.size()), err, sizeof err);
    if (!g_profile) {
        g_error = QStringLiteral("Could not load blend profile: %1").arg(QString::fromUtf8(err));
        AppLog::write(QStringLiteral("op2maplib"), QStringLiteral("Not available: ") + g_error);
        return nullptr;
    }
    AppLog::write(QStringLiteral("op2maplib"),
                  QStringLiteral("Loaded op2maplib %1, profile: %2")
                      .arg(libraryVersion(), QString::fromUtf8(om_profile_name(g_profile))));
    return g_profile;
}

QString profileError()
{
    profile();
    return g_error;
}

void shutdown()
{
    om_profile_free(g_profile);
    g_profile = nullptr;
}

// Swatch and brush-outline colours per ground id, as in OP2Mapper3's Terrain Brush window.
QColor groundColor(const QString &id)
{
    if (id == QLatin1String("mud")) return QColor(150, 120, 85);
    if (id == QLatin1String("roc")) return QColor(105, 105, 110);
    if (id == QLatin1String("san")) return QColor(214, 140, 50);
    if (id == QLatin1String("veg")) return QColor(85, 135, 60);
    if (id == QLatin1String("sno")) return QColor(235, 235, 240);
    if (id == QLatin1String("lav")) return QColor(200, 60, 30);
    return QColor(128, 128, 128);
}

// Grounds with no fill tiles can't be painted, so they get no button.
QVector<Ground> paintableGrounds()
{
    QVector<Ground> out;
    const om_profile *p = profile();
    if (!p)
        return out;
    const int n = om_profile_ground_count(p);
    for (int g = 0; g < n; ++g) {
        if (om_profile_ground_fill_count(p, g) == 0)
            continue;
        Ground gr;
        gr.index = g;
        gr.id = QString::fromUtf8(om_profile_ground_id(p, g));
        gr.name = QString::fromUtf8(om_profile_ground_name(p, g));
        gr.color = groundColor(gr.id);
        out.append(gr);
    }
    return out;
}

BlendMap::~BlendMap()
{
    om_blend_free(m_map);
}

// The library gets the map's mapping table (tileset file name + image per
// entry) and every cell's mapping index, row by row. It never sees the .map
// column-block layout. Its results only use indices already in the table.
bool BlendMap::create(const MapDocument &doc, QString &error, bool &profileUnavailable)
{
    om_blend_free(m_map);
    m_map = nullptr;
    profileUnavailable = false;
    const om_profile *p = profile();
    if (!p) {
        profileUnavailable = true;
        error = profileError();
        return false;
    }
    const auto *map = doc.map();
    if (!map) {
        error = QStringLiteral("No map loaded.");
        return false;
    }
    const int w = doc.widthTiles();
    const int h = doc.heightTiles();

    std::vector<std::string> names;
    names.reserve(map->tileMappings.size());
    std::vector<om_mapping> mappings(map->tileMappings.size());
    for (std::size_t i = 0; i < map->tileMappings.size(); ++i) {
        const auto &m = map->tileMappings[i];
        names.push_back(m.tilesetIndex < map->tilesetSources.size()
                            ? map->tilesetSources[m.tilesetIndex].tilesetFilename
                            : std::string());
    }
    for (std::size_t i = 0; i < mappings.size(); ++i) {
        mappings[i].tileset = names[i].c_str();
        mappings[i].image = map->tileMappings[i].tileGraphicIndex;
    }
    std::vector<uint16_t> cells(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            cells[static_cast<std::size_t>(y) * w + x] = static_cast<uint16_t>(std::max(0, doc.mappingIndexAt(x, y)));

    char err[512] = {};
    m_map = om_blend_create(p, w, h, mappings.data(), static_cast<int32_t>(mappings.size()),
                            cells.data(), err, sizeof err);
    if (!m_map) {
        error = QStringLiteral("Could not create blend map: %1").arg(QString::fromUtf8(err));
        return false;
    }
    return true;
}

// xy holds (x, y) pairs. Returns the number of cells painted.
int BlendMap::paint(int ground, const std::vector<int32_t> &xy)
{
    return om_blend_paint(m_map, ground, xy.data(), static_cast<int32_t>(xy.size() / 2));
}

int BlendMap::resolve(uint64_t seed) { return om_blend_resolve(m_map, nullptr, seed); }
int BlendMap::resolveAll(uint64_t seed) { return om_blend_resolve_all(m_map, seed); }

int BlendMap::drawCliff(const std::vector<int32_t> &xy, int highSide, double wallHeight, uint64_t seed)
{
    return om_cliff_draw(m_map, xy.data(), static_cast<int32_t>(xy.size() / 2), highSide, wallHeight, seed);
}

int BlendMap::lastCliffStage() const { return om_cliff_last_stage(m_map); }

// Changes from the last resolve or cliff draw.
std::vector<BlendMap::Change> BlendMap::changes(int count) const
{
    std::vector<Change> out;
    if (count <= 0 || !m_map)
        return out;
    std::vector<om_change> raw(static_cast<std::size_t>(count));
    const int n = om_blend_get_changes(m_map, raw.data(), count);
    out.reserve(static_cast<std::size_t>(std::max(0, n)));
    for (int i = 0; i < n; ++i) {
        const auto &c = raw[static_cast<std::size_t>(i)];
        out.push_back({ c.x, c.y, c.old_mapping, c.new_mapping, c.cell_type });
    }
    return out;
}

QPoint snapCliffPoint(const QPoint &a, const QPoint &b)
{
    int32_t ox = b.x(), oy = b.y();
    om_cliff_snap_point(a.x(), a.y(), b.x(), b.y(), &ox, &oy);
    return QPoint(ox, oy);
}

std::vector<TileChange> toTileChanges(const MapDocument &doc,
                                      const std::vector<BlendMap::Change> &changes,
                                      bool lockLastRow)
{
    std::vector<TileChange> out;
    out.reserve(changes.size());
    const int last = doc.lastRow();
    for (const auto &c : changes) {
        TileChange t;
        t.x = c.x;
        t.y = c.y;
        t.oldMapping = c.oldMapping;
        t.newMapping = c.newMapping;
        t.oldCellType = doc.cellTypeAt(c.x, c.y);
        t.newCellType = t.oldCellType;
        if (c.cellType >= 0 && c.cellType < 32 && !(lockLastRow && c.y == last))
            t.newCellType = c.cellType;
        out.push_back(t);
    }
    return out;
}

// Passable regions with Outpost 2's rules (8-connected, bottom row skipped,
// edges joined only on 512x256 world maps), largest first.
bool findRegions(const MapDocument &doc, QVector<QVector<QPoint>> &regions, QString &error)
{
    regions.clear();
    const int w = doc.widthTiles();
    const int h = doc.heightTiles();
    std::vector<uint8_t> ct(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            ct[static_cast<std::size_t>(y) * w + x] = static_cast<uint8_t>(doc.cellTypeAt(x, y) & 0x1F);
    om_regions *r = om_regions_find(w, h, ct.data(), 0, om_regions_op2_flags(w, h));
    if (!r) {
        error = QStringLiteral("Could not analyse the map's regions.");
        return false;
    }
    const int n = om_regions_count(r);
    for (int i = 0; i < n; ++i) {
        const int size = om_regions_size(r, i);
        std::vector<int32_t> xy(static_cast<std::size_t>(size) * 2);
        const int got = om_regions_cells(r, i, xy.data(), size);
        QVector<QPoint> cells;
        cells.reserve(got);
        for (int k = 0; k < got; ++k)
            cells.append(QPoint(xy[2 * k], xy[2 * k + 1]));
        regions.append(cells);
    }
    om_regions_free(r);
    std::stable_sort(regions.begin(), regions.end(),
                     [](const QVector<QPoint> &a, const QVector<QPoint> &b) { return a.size() > b.size(); });
    return true;
}

} // namespace Op2Lib
