#pragma once

#include <QString>
#include <QVector>

// Manages the bundled blank .map templates. Files are embedded as Qt
// resources under :/blankmaps/blank_WxH.map and extracted on first use
// to AppLocalDataLocation/BlankMaps/, since OP2Utility::Map::ReadMap
// needs a real filesystem path.
namespace BlankMaps {

struct Size {
    int width;
    int height;
};

// All available blank-map sizes in the order the New Map dialog should show.
QVector<Size> availableSizes();

// Path on disk for blank_WxH.map. Extracts the embedded resource on first
// call. Returns empty string if width/height aren't in availableSizes() or
// the extraction fails.
QString pathFor(int width, int height);

} // namespace BlankMaps
