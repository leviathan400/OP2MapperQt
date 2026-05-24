cmake_minimum_required(VERSION 3.16)

project(OP2Mapper VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(OP2UTIL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../OP2Utility" CACHE PATH "Path to OP2Utility checkout")

qt_add_executable(appOP2Mapper
    main.cpp
    app.qrc
    $<$<PLATFORM_ID:Windows>:app.rc>
    src/MainWindow.h
    src/MainWindow.cpp
    src/MapView.h
    src/MapView.cpp
    src/MapScene.h
    src/MapScene.cpp
    src/MapDocument.h
    src/MapDocument.cpp
    src/TileImageCache.h
    src/TileImageCache.cpp
    src/Settings.h
    src/Settings.cpp
    src/CellTypeInfo.h
    src/CellTypeInfo.cpp
)

target_include_directories(appOP2Mapper PRIVATE src)

target_link_libraries(appOP2Mapper PRIVATE Qt6::Widgets)

if(EXISTS "${OP2UTIL_DIR}/CMakeLists.txt")
    message(STATUS "OP2Utility found at ${OP2UTIL_DIR} — linking.")
    add_subdirectory("${OP2UTIL_DIR}" "${CMAKE_BINARY_DIR}/OP2Utility")
    target_link_libraries(appOP2Mapper PRIVATE OP2Utility)
    target_compile_definitions(appOP2Mapper PRIVATE OP2MAPPER_HAVE_OP2UTILITY=1)
elseif(EXISTS "${OP2UTIL_DIR}/OP2Utility/CMakeLists.txt")
    message(STATUS "OP2Utility found at ${OP2UTIL_DIR}/OP2Utility — linking.")
    add_subdirectory("${OP2UTIL_DIR}/OP2Utility" "${CMAKE_BINARY_DIR}/OP2Utility")
    target_link_libraries(appOP2Mapper PRIVATE OP2Utility)
    target_compile_definitions(appOP2Mapper PRIVATE OP2MAPPER_HAVE_OP2UTILITY=1)
else()
    message(WARNING "OP2Utility not found at ${OP2UTIL_DIR}. Building UI-only stub. "
                    "Clone https://github.com/OutpostUniverse/OP2Utility there to enable map loading.")
endif()

set_target_properties(appOP2Mapper PROPERTIES
    WIN32_EXECUTABLE TRUE
)

install(TARGETS appOP2Mapper
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
