// CellTypeInfo: per-cell-type tables copied from OP2Mapper3.

#include "CellTypeInfo.h"

namespace {

struct Style {
    const char *name;
    const char *label;
    QRgb labelColor;
    QRgb basic;
    QRgb detailed;
    QRgb overlay;
};

// Indexed by raw cell type value.
const Style kStyles[CellTypeInfo::Count] = {
    { "FastPassible1",     "Fast1",  0xFF7CFC00, 0xFF90EE90, 0xFF00FF00, 0xFF00FF00 },
    { "Impassible2",       "Imp2",   0xFFFF0000, 0xFF8B0000, 0xFFFF0000, 0xFFC00000 },
    { "SlowPassible1",     "Slow1",  0xFFFFA500, 0xFFFFFF00, 0xFFFF8000, 0xFFFF8000 },
    { "SlowPassible2",     "Slow2",  0xFFFFA500, 0xFFFFFF00, 0xFFFF8000, 0xFFFF8000 },
    { "MediumPassible1",   "Med1",   0xFFFFFF00, 0xFF808000, 0xFFFFFF00, 0xFFFFFF00 },
    { "MediumPassible2",   "Med2",   0xFFFFFF00, 0xFF808000, 0xFFFFFF00, 0xFFFFFF00 },
    { "Impassible1",       "Imp1",   0xFFFF0000, 0xFF8B0000, 0xFFFF0000, 0xFFC00000 },
    { "FastPassible2",     "Fast2",  0xFF7CFC00, 0xFF90EE90, 0xFF00FF00, 0xFF00FF00 },
    { "NorthCliffs",       "N Clif", 0xFFFF0000, 0xFFA0522D, 0xFF8B4513, 0xFFC00000 },
    { "CliffsHighSide",    "Clif H", 0xFFFF0000, 0xFFA0522D, 0xFF8B4513, 0xFFC00000 },
    { "CliffsLowSide",     "Clif L", 0xFFFF0000, 0xFFA0522D, 0xFF8B4513, 0xFFC00000 },
    { "VentsAndFumaroles", "Vent",   0xFFFFFFFF, 0xFFFFA500, 0xFF800080, 0xFFFF8000 },
    { "zPad12",            "zPad1",  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { "zPad13",            "zPad1",  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { "zPad14",            "zPad1",  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { "zPad15",            "zPad1",  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { "zPad16",            "zPad1",  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { "zPad17",            "zPad1",  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { "zPad18",            "zPad1",  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { "zPad19",            "zPad1",  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { "zPad20",            "zPad1",  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { "DozedArea",         "Dozed",  0xFF90EE90, 0xFFD3D3D3, 0xFFC0C0C0, 0xFF00FF00 },
    { "Rubble",            "Rubble", 0xFFFF0000, 0xFFA9A9A9, 0xFF404040, 0xFFC00000 },
    { "NormalWall",        "Wall",   0xFFFF0000, 0xFF808080, 0xFF808080, 0xFFC00000 },
    { "MicrobeWall",       "MWall",  0xFFFF0000, 0xFF800080, 0xFF8000FF, 0xFFC00000 },
    { "LavaWall",          "LWall",  0xFFFF0000, 0xFFFF4500, 0xFFFF4500, 0xFFC00000 },
    { "Tube0",             "Tube0",  0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF },
    { "Tube1",             "Tube1",  0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF },
    { "Tube2",             "Tube2",  0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF },
    { "Tube3",             "Tube3",  0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF },
    { "Tube4",             "Tube4",  0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF },
    { "Tube5",             "Tube5",  0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF },
};

struct Row {
    const char *text;
    int cellType;
};

const Row kRows[CellTypeInfo::Count] = {
    { "Fast Passable 1 (F1)", 0 },     { "Fast Passable 2 (F2)", 7 },
    { "Medium Passable 1 (M1)", 4 },   { "Medium Passable 2 (M2)", 5 },
    { "Slow Passable 1 (S1)", 2 },     { "Slow Passable 2 (S2)", 3 },
    { "Impassable 1 (I1)", 6 },        { "Impassable 2 (I2)", 1 },
    { "North Cliffs (NC)", 8 },        { "Cliffs - High Side (CHS)", 9 },
    { "Cliffs - Low Side (CLS)", 10 }, { "Vents and Fumaroles (V)", 11 },
    { "zPad12 (Z12)", 12 },            { "zPad13 (Z13)", 13 },
    { "zPad14 (Z14)", 14 },            { "zPad15 (Z15)", 15 },
    { "zPad16 (Z16)", 16 },            { "zPad17 (Z17)", 17 },
    { "zPad18 (Z18)", 18 },            { "zPad19 (Z19)", 19 },
    { "zPad20 (Z20)", 20 },            { "Bulldozed (D)", 21 },
    { "Rubble (R)", 22 },              { "Normal Wall (NW)", 23 },
    { "Microbe Wall (MW)", 24 },       { "Lava Wall (LW)", 25 },
    { "Tube 0 (T0)", 26 },             { "Tube 1 (T1)", 27 },
    { "Tube 2 (T2)", 28 },             { "Tube 3 (T3)", 29 },
    { "Tube 4 (T4)", 30 },             { "Tube 5 (T5)", 31 },
};

// Out-of-range values use a zPad entry (white).
const Style &style(int ct)
{
    return kStyles[(ct >= 0 && ct < CellTypeInfo::Count) ? ct : 12];
}

} // namespace

namespace CellTypeInfo {

QString enumName(int ct)
{
    if (ct < 0 || ct >= Count)
        return QString::number(ct);
    return QString::fromLatin1(kStyles[ct].name);
}

QString label(int ct) { return QString::fromLatin1(style(ct).label); }
QColor labelColor(int ct) { return QColor::fromRgba(style(ct).labelColor); }
QColor basicColor(int ct) { return QColor::fromRgba(style(ct).basic); }
QColor detailedColor(int ct) { return QColor::fromRgba(style(ct).detailed); }
QColor overlayColor(int ct) { return QColor::fromRgba(style(ct).overlay); }

// OP2Mapper3 GetCellTypeColor: colour by passability category.
QColor categoryColor(int ct)
{
    switch (ct) {
    case 0: case 7: return QColor(0, 128, 0);              // Fast*: Green
    case 4: case 5: return QColor(128, 128, 0);            // Medium*: Olive
    case 2: case 3: return QColor(255, 140, 0);            // Slow*: DarkOrange
    case 1: case 6: return QColor(255, 0, 0);              // Impassible*: Red
    case 21: case 22: return QColor(139, 69, 19);          // Dozed/Rubble: SaddleBrown
    case 23: case 24: case 25: return QColor(0, 0, 139);   // walls: DarkBlue
    case 26: case 27: case 28: case 29: case 30: case 31:
        return QColor(128, 0, 128);                        // tubes: Purple
    case 8: case 9: case 10: return QColor(165, 42, 42);   // cliffs: Brown
    case 11: return QColor(178, 34, 34);                   // vents: Firebrick
    default: return QColor(169, 169, 169);                 // zPad: DarkGray
    }
}

QString listText(int row)
{
    if (row < 0 || row >= Count)
        return QString();
    return QString::fromLatin1(kRows[row].text);
}

int listRowToCellType(int row)
{
    if (row < 0 || row >= Count)
        return -1;
    return kRows[row].cellType;
}

int cellTypeToListRow(int ct)
{
    for (int i = 0; i < Count; ++i) {
        if (kRows[i].cellType == ct)
            return i;
    }
    return -1;
}

} // namespace CellTypeInfo
