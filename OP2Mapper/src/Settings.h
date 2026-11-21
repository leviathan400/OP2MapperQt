#pragma once

#include <QString>
#include <QStringList>

// Typed settings stored in Mapper3.ini next to the exe (see IniFile.h),
// with the same sections and keys as OP2Mapper3:
//   [Paths]  OP2Path, WorkingPath, Tilegroups
//   [Window] RecentFile1..5, PasteCellTypes, ToolWindowsAlwaysOnTop,
//            AlwaysOnTop_<palette class>, palette/main-window positions,
//            TileSelect*, Terrain* and AllowEditLastRow.
namespace Settings {

// One-time move of the old Qt-port registry settings into Mapper3.ini.
void migrateLegacySettings();

QString op2Path();
void setOp2Path(const QString &path);
QString workingPath();
void setWorkingPath(const QString &path);
QString tileGroupsPath();
void setTileGroupsPath(const QString &path);

// <exe dir>/Tilegroups
QString defaultTileGroupsPath();

// Recent files: most recent first, max 5, non-existent entries dropped on read.
QStringList recentFiles();
void addRecentFile(const QString &path);
void removeRecentFile(const QString &path);

bool pasteCellTypes();
void setPasteCellTypes(bool on);

// Hidden power-user override (VB My.Settings.AllowEditLastRow), default false.
bool allowEditLastRow();

// Generic [Window] access for palettes.
QString windowValue(const QString &key, const QString &defaultValue = QString());
void setWindowValue(const QString &key, const QString &value);
bool windowBool(const QString &key, bool defaultValue);
void setWindowBool(const QString &key, bool v);
int windowInt(const QString &key, int defaultValue);
void setWindowInt(const QString &key, int v);
bool hasWindowValue(const QString &key);

// A valid OP2 1.4.1 install: folder exists, Outpost2.exe in it and
// OPU/OPULauncher.exe below it.
bool isValidOp2Folder(const QString &path);

} // namespace Settings
