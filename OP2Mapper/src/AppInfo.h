#pragma once

// Application identity. The version comes from project(... VERSION ...) in
// CMakeLists.txt (OP2MAPPERQT_VERSION). Name, description and links follow
// OP2Mapper3 (GlobalVariables.vb); the window title and Mapper3.ini /
// OP2Mapper.log names keep "OP2Mapper" so both apps share them.
#ifndef OP2MAPPERQT_VERSION
#define OP2MAPPERQT_VERSION "0.0.0"
#endif

namespace AppInfo {
inline constexpr const char *Name = "OP2Mapper";
inline constexpr const char *DisplayName = "OP2MapperQt"; // splash and About
inline constexpr const char *Version = OP2MAPPERQT_VERSION;
// The OP2Mapper3 release this port matches feature for feature.
inline constexpr const char *PortedFrom = "OP2Mapper3 0.7.0 (build 0152)";
inline constexpr const char *Description =
    "A powerful map editor for Outpost 2: Divided Destiny.";
inline constexpr const char *Website = "https://github.com/leviathan400/OP2MapperQt";
inline constexpr const char *Copyright = "Outpost Universe © 2025-2026";
} // namespace AppInfo
