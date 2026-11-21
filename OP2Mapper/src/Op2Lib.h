#pragma once

#include "MapDocument.h"

#include <QColor>
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

} // namespace Op2Lib
