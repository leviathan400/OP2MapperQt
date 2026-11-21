cmake_minimum_required(VERSION 3.16)

project(OP2Mapper VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

include(GNUInstallDirs)

set(OP2UTIL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../OP2Utility" CACHE PATH "Path to OP2Utility checkout")
# op2maplib lives in the OP2MappingLib repository
# (https://github.com/OutpostUniverse/OP2MappingLib), checked out next to
# this repository as "op2maplib" or "OP2MappingLib".
set(_op2maplib_default "${CMAKE_CURRENT_SOURCE_DIR}/../../op2maplib")
if(NOT EXISTS "${_op2maplib_default}/CMakeLists.txt" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../OP2MappingLib/CMakeLists.txt")
    set(_op2maplib_default "${CMAKE_CURRENT_SOURCE_DIR}/../../OP2MappingLib")
endif()
set(OP2MAPLIB_DIR "${_op2maplib_default}" CACHE PATH "Path to the op2maplib checkout (OP2MappingLib)")

# Everything but main.cpp, shared by the app and the self-test.
add_library(op2mapper_core STATIC
    src/AppInfo.h
    src/AppLog.h
    src/AppLog.cpp
    src/BlankMaps.h
    src/BlankMaps.cpp
    src/CellTypeInfo.h
    src/CellTypeInfo.cpp
    src/CellTypesWindow.h
    src/CellTypesWindow.cpp
    src/Editor.h
    src/Editor.cpp
    src/IniFile.h
    src/IniFile.cpp
    src/JsonWriter.h
    src/JsonWriter.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/MapDocument.h
    src/MapDocument.cpp
    src/MapJsonIO.h
    src/MapJsonIO.cpp
    src/MapRenderer.h
    src/MapRenderer.cpp
    src/MapView.h
    src/MapView.cpp
    src/MiniMap.h
    src/MiniMap.cpp
    src/NewMapDialog.h
    src/NewMapDialog.cpp
    src/OP2Launcher.h
    src/OP2Launcher.cpp
    src/Op2Lib.h
    src/Op2Lib.cpp
    src/SettingsDialog.h
    src/SettingsDialog.cpp
    src/Settings.h
    src/Settings.cpp
    src/Sidebar.h
    src/Sidebar.cpp
    src/TerrainWindow.h
    src/TerrainWindow.cpp
    src/TileGroupIO.h
    src/TileGroupIO.cpp
    src/TileGroupsWindow.h
    src/TileGroupsWindow.cpp
    src/TileImageCache.h
    src/TileImageCache.cpp
    src/TileSelectWindow.h
    src/TileSelectWindow.cpp
    src/TilesetGrid.h
    src/TilesetGrid.cpp
    src/TilesetWindow.h
    src/TilesetWindow.cpp
    src/ToolPalette.h
    src/ToolPalette.cpp
)
target_include_directories(op2mapper_core PUBLIC src)
target_link_libraries(op2mapper_core PUBLIC Qt6::Widgets Qt6::Network)
# The app version (src/AppInfo.h) is the project version above.
target_compile_definitions(op2mapper_core PUBLIC OP2MAPPERQT_VERSION="${PROJECT_VERSION}")
set_target_properties(op2mapper_core PROPERTIES AUTOMOC ON)
# Message texts contain non-ASCII characters (×, —, →); MSVC
# must read and store the sources as UTF-8.
if(MSVC)
    target_compile_options(op2mapper_core PUBLIC /utf-8)
endif()

qt_add_executable(appOP2Mapper
    main.cpp
    app.qrc
    $<$<PLATFORM_ID:Windows>:app.rc>
)


target_link_libraries(appOP2Mapper PRIVATE op2mapper_core)
if(WIN32)
    # winmm: PlaySound; dwmapi: palette snapping on the visible window frame
    target_link_libraries(op2mapper_core PUBLIC winmm dwmapi)
    # Keep <windows.h> from defining min/max macros (they break std::min/max).
    target_compile_definitions(op2mapper_core PUBLIC NOMINMAX)
endif()

# op2maplib: terrain blending, cliffs and enclosed-area regions (C API).
# Built as a static library so the app needs no extra DLL; its blend
# profile (profiles/op2.json) is embedded as :/op2maplib/op2.json.
if(NOT EXISTS "${OP2MAPLIB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "op2maplib not found at ${OP2MAPLIB_DIR}. Clone https://github.com/OutpostUniverse/OP2MappingLib next to this repository or set -DOP2MAPLIB_DIR=<path>.")
endif()
set(OP2MAPLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(OP2MAPLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${OP2MAPLIB_DIR}" "${CMAKE_BINARY_DIR}/op2maplib")
target_link_libraries(op2mapper_core PUBLIC op2maplib)
# op2maplib links the static MSVC runtime for its standalone DLL; as a
# static library inside this app it must use the same runtime as Qt.
if(MSVC)
    set_property(TARGET op2maplib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
set_source_files_properties("${OP2MAPLIB_DIR}/profiles/op2.json"
    PROPERTIES QT_RESOURCE_ALIAS op2.json)
qt_add_resources(op2mapper_core op2maplib_profile
    PREFIX "/op2maplib"
    FILES "${OP2MAPLIB_DIR}/profiles/op2.json")

# OP2Utility: map, tileset and archive I/O. Use its CMake project if the
# checkout has one; otherwise (a plain clone of the upstream repository,
# which ships no CMake files) build its sources here.
if(EXISTS "${OP2UTIL_DIR}/CMakeLists.txt")
    add_subdirectory("${OP2UTIL_DIR}" "${CMAKE_BINARY_DIR}/OP2Utility")
elseif(EXISTS "${OP2UTIL_DIR}/OP2Utility/CMakeLists.txt")
    add_subdirectory("${OP2UTIL_DIR}/OP2Utility" "${CMAKE_BINARY_DIR}/OP2Utility")
elseif(EXISTS "${OP2UTIL_DIR}/OP2Utility/src")
    file(GLOB_RECURSE OP2UTIL_SOURCES CONFIGURE_DEPENDS
        "${OP2UTIL_DIR}/OP2Utility/src/*.cpp"
        "${OP2UTIL_DIR}/OP2Utility/src/*.h")
    add_library(OP2Utility STATIC ${OP2UTIL_SOURCES})
    target_include_directories(OP2Utility PUBLIC
        "${OP2UTIL_DIR}/OP2Utility/include"
        "${OP2UTIL_DIR}/OP2Utility/src")
    target_compile_features(OP2Utility PUBLIC cxx_std_20)
else()
    message(FATAL_ERROR "OP2Utility not found at ${OP2UTIL_DIR}. Clone "
                        "https://github.com/OutpostUniverse/OP2Utility there or set -DOP2UTIL_DIR=<path>.")
endif()
message(STATUS "OP2Utility: ${OP2UTIL_DIR}")
target_link_libraries(op2mapper_core PUBLIC OP2Utility)
target_compile_definitions(op2mapper_core PUBLIC OP2MAPPER_HAVE_OP2UTILITY=1)

set_target_properties(appOP2Mapper PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME OP2Mapper
    MACOSX_BUNDLE_GUI_IDENTIFIER org.outpostuniverse.op2mapperqt
)

install(TARGETS appOP2Mapper
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Headless self-test of the editing core (needs an Outpost 2 install):
#   cmake -DOP2MAPPER_BUILD_SELFTEST=ON ...
#   QT_QPA_PLATFORM=offscreen op2mapper_selftest <op2 folder> <map> <out dir>
option(OP2MAPPER_BUILD_SELFTEST "Build the headless self-test" OFF)
if(OP2MAPPER_BUILD_SELFTEST)
    qt_add_executable(op2mapper_selftest tests/selftest.cpp app.qrc)
    target_link_libraries(op2mapper_selftest PRIVATE op2mapper_core)
endif()
