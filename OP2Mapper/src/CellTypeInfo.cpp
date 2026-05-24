#include "CellTypeInfo.h"

#include <OP2Utility.h>

namespace CellTypeInfo {

QString label(OP2Utility::CellType ct)
{
    using OP2Utility::CellType;
    switch (ct) {
        case CellType::FastPassible1:    return "Fast1";
        case CellType::FastPassible2:    return "Fast2";
        case CellType::MediumPassible1:  return "Med1";
        case CellType::MediumPassible2:  return "Med2";
        case CellType::SlowPassible1:    return "Slow1";
        case CellType::SlowPassible2:    return "Slow2";
        case CellType::Impassible1:      return "Imp1";
        case CellType::Impassible2:      return "Imp2";
        case CellType::NorthCliffs:      return "NCliff";
        case CellType::CliffsHighSide:   return "CliffH";
        case CellType::CliffsLowSide:    return "CliffL";
        case CellType::VentsAndFumaroles:return "Vent";
        case CellType::DozedArea:        return "Dozed";
        case CellType::Rubble:           return "Rubble";
        case CellType::NormalWall:       return "Wall";
        case CellType::MicrobeWall:      return "MWall";
        case CellType::LavaWall:         return "LWall";
        case CellType::Tube0:            return "Tube";
        case CellType::Tube1:            return "Tube1";
        case CellType::Tube2:            return "Tube2";
        case CellType::Tube3:            return "Tube3";
        case CellType::Tube4:            return "Tube4";
        case CellType::Tube5:            return "Tube5";
        default:                         return QString("?%1").arg(static_cast<int>(ct));
    }
}

QColor color(OP2Utility::CellType ct)
{
    using OP2Utility::CellType;
    constexpr int A = 140; // overlay alpha
    switch (ct) {
        case CellType::FastPassible1:    return QColor(  0, 200,   0, A); // green
        case CellType::FastPassible2:    return QColor( 80, 230,  80, A);
        case CellType::MediumPassible1:  return QColor(255, 200,   0, A); // amber
        case CellType::MediumPassible2:  return QColor(255, 160,   0, A);
        case CellType::SlowPassible1:    return QColor(220, 100,   0, A); // orange
        case CellType::SlowPassible2:    return QColor(200,  70,   0, A);
        case CellType::Impassible1:      return QColor(220,   0,   0, A); // red
        case CellType::Impassible2:      return QColor(160,   0,   0, A);
        case CellType::NorthCliffs:      return QColor(120, 120, 120, A);
        case CellType::CliffsHighSide:   return QColor(140, 140, 140, A);
        case CellType::CliffsLowSide:    return QColor(100, 100, 100, A);
        case CellType::VentsAndFumaroles:return QColor(255,   0, 255, A);
        case CellType::DozedArea:        return QColor(160, 130,  90, A);
        case CellType::Rubble:           return QColor(110,  80,  50, A);
        case CellType::NormalWall:       return QColor(  0, 120, 200, A);
        case CellType::MicrobeWall:      return QColor(  0, 180,  80, A);
        case CellType::LavaWall:         return QColor(200,  60,   0, A);
        case CellType::Tube0:
        case CellType::Tube1:
        case CellType::Tube2:
        case CellType::Tube3:
        case CellType::Tube4:
        case CellType::Tube5:            return QColor(  0, 200, 220, A); // cyan
        default:                         return QColor(255,   0, 255, A);
    }
}

QString name(OP2Utility::CellType ct)
{
    using OP2Utility::CellType;
    switch (ct) {
        case CellType::FastPassible1:    return "Fast Passible 1";
        case CellType::FastPassible2:    return "Fast Passible 2";
        case CellType::MediumPassible1:  return "Medium Passible 1";
        case CellType::MediumPassible2:  return "Medium Passible 2";
        case CellType::SlowPassible1:    return "Slow Passible 1";
        case CellType::SlowPassible2:    return "Slow Passible 2";
        case CellType::Impassible1:      return "Impassible 1";
        case CellType::Impassible2:      return "Impassible 2";
        case CellType::NorthCliffs:      return "North Cliffs";
        case CellType::CliffsHighSide:   return "Cliffs (High Side)";
        case CellType::CliffsLowSide:    return "Cliffs (Low Side)";
        case CellType::VentsAndFumaroles:return "Vents / Fumaroles";
        case CellType::DozedArea:        return "Dozed Area";
        case CellType::Rubble:           return "Rubble";
        case CellType::NormalWall:       return "Normal Wall";
        case CellType::MicrobeWall:      return "Microbe Wall";
        case CellType::LavaWall:         return "Lava Wall";
        case CellType::Tube0:            return "Tube";
        case CellType::Tube1:            return "Tube 1";
        case CellType::Tube2:            return "Tube 2";
        case CellType::Tube3:            return "Tube 3";
        case CellType::Tube4:            return "Tube 4";
        case CellType::Tube5:            return "Tube 5";
        default:                         return QString("Unknown (%1)").arg(static_cast<int>(ct));
    }
}

}
