#pragma once

#include <QColor>
#include <QString>

namespace OP2Utility { enum class CellType; }

namespace CellTypeInfo {
    // Short label (max ~5 chars) for overlay display, matching VB mapper style.
    QString label(OP2Utility::CellType ct);

    // Semi-transparent fill color for overlay; designed to layer over terrain.
    QColor color(OP2Utility::CellType ct);

    // Full descriptive name for the status bar / tooltips.
    QString name(OP2Utility::CellType ct);
}
