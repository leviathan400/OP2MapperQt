#pragma once

#include <QString>
#include <QVector>

// Bundled blank .map templates (OP2Mapper3 GlobalVariables blank-map
// extraction). Embedded as :/blankmaps/blank_WxH.map and extracted to
// <exe>/BlankMaps (if it already has templates or the exe folder is
// writable) or else %LOCALAPPDATA%/OP2Mapper/BlankMaps. A file that is
// missing or has the wrong size is (re)written via a temp file.
namespace BlankMaps {

struct Size {
    int width;
    int height;
};

QVector<Size> availableSizes();
QString folder();

// Extracts every template. Returns false and an error message on failure.
bool ensureExtracted(QString &error);

// Path of blank_WxH.map on disk, or empty if that size doesn't exist.
QString pathFor(int width, int height);

} // namespace BlankMaps
