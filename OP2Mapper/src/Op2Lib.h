#pragma once

#include "MapDocument.h"

#include <QColor>
#include <QPair>
#include <QPoint>
#include <QString>
#include <QVector>
#include <cstdint>
#include <vector>

struct om_profile;
struct om_blend_map;

// Thin C++ layer over op2maplib's C API (terrain blending, cliffs, passable
// regions), following how OP2Mapper3 uses its .NET binding.
namespace Op2Lib {

// The blend profile, loaded once from the embedded :/op2maplib/op2.json
// (or op2maplib/op2.json next to the exe, if present). A failure is
// remembered and not retried; profileError() says why.
const om_profile *profile();
QString profileError();
void shutdown();

struct Ground {
    int index = -1;
    QString id;
    QString name;
    QColor color;
};
// Grounds that have fill tiles (OP2: lava and snow are hidden).
QVector<Ground> paintableGrounds();
QColor groundColor(const QString &id);

// A blend state built from the current map. VB builds a fresh one per
// brush stroke / Clean Up / Place Cliff, so it never needs syncing.
class BlendMap
{
public:
    BlendMap() = default;
    ~BlendMap();
    BlendMap(const BlendMap &) = delete;
    BlendMap &operator=(const BlendMap &) = delete;

    // Returns false with an error if the profile is unavailable or the map
    // can't be read; profileUnavailable tells the two apart.
    bool create(const MapDocument &doc, QString &error, bool &profileUnavailable);
    bool isValid() const { return m_map != nullptr; }

    int paint(int ground, const std::vector<int32_t> &xy);
    int resolve(uint64_t seed);
    int resolveAll(uint64_t seed);
    // Returns the change count, or -1 for invalid input.
    int drawCliff(const std::vector<int32_t> &xy, int highSide, double wallHeight, uint64_t seed);
    int lastCliffStage() const;

    struct Change { int x, y, oldMapping, newMapping, cellType; };
    std::vector<Change> changes(int count) const;

private:
    om_blend_map *m_map = nullptr;
};

QPoint snapCliffPoint(const QPoint &a, const QPoint &b);

// Converts library changes to undoable tile changes. A suggested cell type
// (0..31) is applied unless the tile is on the locked bottom row.
std::vector<TileChange> toTileChanges(const MapDocument &doc,
                                      const std::vector<BlendMap::Change> &changes,
                                      bool lockLastRow);

// Passable regions with OP2 rules; regions sorted largest first. Returns
// false with an error message if the library refused the input.
bool findRegions(const MapDocument &doc, QVector<QVector<QPoint>> &regions, QString &error);

QString libraryVersion();

// ---- Map generator (om_mapgen_*) ---------------------------------------------

// One terrain feature for the generator.
struct MapGenFeature {
    enum Kind { Valley = 0, Mountain, Plateau, Clearing, Choke };
    int kind = Valley;
    QString where; // sector "nw", "n", "no", "w", "c", "o", "sw", "s", "so"; "" = random
    QString to;    // valley / choke end sector; "" = opposite of `where`
    double size = 0.5; // 0..1
    bool lava = false; // valley only
};

// Everything the generator needs (OP2Mapper3 fGenerateMap's request).
struct MapGenRequest {
    QString templateName = QStringLiteral("plains");
    int width = 128;
    int height = 128;
    quint64 seed = 1234;
    QVector<QPair<QString, double>> mix; // empty = the template's own mix
    double roughness = 0.5;
    double minBiome = 0.006;
    int symmetry = 0;      // om_mapgen_symmetry
    double cliffs = 0;
    double details = 0.5;
    int spawns = 2;
    int volcanoes = 0;
    QVector<MapGenFeature> features;
};

// A generated map. Tiles are indices into the profile's standard mapping
// table; tiles and cell types are row-major.
struct GeneratedMap {
    int width = 0;
    int height = 0;
    std::vector<uint16_t> tiles;
    std::vector<uint8_t> cellTypes;
    QVector<QPoint> spawns;
    QVector<QPoint> volcanoes;
    QVector<QVector<QPoint>> lavaZones;
    double passableShare = 0;
    double plateauShare = 0;
    bool connected = false;
    QVector<double> groundShares; // indexed by profile ground
};

// A template's default for a numeric setting (e.g. "roughness", "volcanoes").
double mapGenTemplateDefault(const QString &templateName, const QString &key);

// Number of entries in the profile's standard mapping table (0 if none).
int standardMappingCount();

QString groundName(int ground);

// Runs the generator. Safe to call from a worker thread (it only reads the
// shared profile). Returns false with an error message on failure.
bool generateMap(const MapGenRequest &request, GeneratedMap &out, QString &error);

// Writes a generated map into a document holding the blank template of the
// same size (whose mapping table is the profile's standard table): tiles,
// cell types and lava-possible zones, bottom row Impassable. Written
// directly, not as an undo step (it is the new map's starting state).
// Returns false with an error if the sizes or mapping tables don't match.
bool applyGeneratedMap(MapDocument &doc, const GeneratedMap &gen, QString &error);

} // namespace Op2Lib
