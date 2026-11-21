#pragma once

#include <QColor>
#include <QString>

// Per-cell-type styles, copied from OP2Mapper3 MapRenderer.CellTypeStyles.vb
// and the Cell Types list (fCellTypes.vb / GlobalVariables.CellTypeMap).
// Cell types are the raw OP2 values 0..31 (OP2Utility::CellType order).
namespace CellTypeInfo {

constexpr int Count = 32;
constexpr int OverlayAlpha = 115;

// Enum name, e.g. "FastPassible1", "zPad12", "Tube0".
QString enumName(int cellType);

// Mode 2 (CellTypeOverlayAll) label and its colour.
QString label(int cellType);
QColor labelColor(int cellType);

// Opaque palettes: Basic (mode 3, fallback tiles), Detailed (mode 4),
// Overlay base colour (drawn at OverlayAlpha in modes 1/2 and Tile Selection).
QColor basicColor(int cellType);
QColor detailedColor(int cellType);
QColor overlayColor(int cellType);

// Colour used for the "Placing: <type>" label and tool swatch.
QColor categoryColor(int cellType);

// Cell Types palette rows (list order differs from the enum order).
QString listText(int row);
int listRowToCellType(int row);
int cellTypeToListRow(int cellType);

} // namespace CellTypeInfo
