#pragma once

#include <QString>
#include <QStringList>

// Thin typed facade over QSettings (org "OutpostUniverse", app "OP2Mapper",
// set in main.cpp). Each accessor reads/writes one key; getters with
// computed defaults (tileGroupsPath, workingPath) resolve the fallback at
// call time, so a value is only persisted once the user explicitly sets it.
// Tool-palette window geometry is NOT here — each palette saves its own
// QSettings key (e.g. "TilesetWindow/geometry") in its closeEvent.
namespace Settings {
    QString op2Folder();
    void setOp2Folder(const QString &path);
    QString lastMapDir();
    void setLastMapDir(const QString &path);

    // Tile groups root folder. Default: <app dir>/Tilegroups.
    // Exports go to <tileGroupsPath>/Exported/.
    QString tileGroupsPath();
    void setTileGroupsPath(const QString &path);

    // Working directory (defaults to lastMapDir or app dir).
    QString workingPath();
    void setWorkingPath(const QString &path);

    // Recent file list (most-recent-first, capped to 5). Stored as a
    // newline-joined string in QSettings.
    QStringList recentFiles();
    void addRecentFile(const QString &path);
    void removeRecentFile(const QString &path);
}
