# Architecture Overview

How OP2MapperQt is put together. It is a Qt 6 / C++17 desktop map editor for *Outpost 2: Divided Destiny* that edits the tile graphics and cell types of `.map` files. It is a port of the VB.NET editor [OP2Mapper3](https://github.com/leviathan400/OP2Mapper) and follows its behaviour, message texts and settings file; most classes name the OP2Mapper3 form or module they correspond to.

Keep this document up to date when the structure changes.

## 1. Project structure

```
OP2MapperQt/
├── README.md, ARCHITECTURE.md
├── docs/OP2MapperQt.png        # README screenshot
├── .github/workflows/build.yml # CI: Windows, Linux and macOS builds
├── OP2Utility/                 # OP2Utility C++ library (map, tileset and archive I/O)
└── OP2Mapper/
    ├── CMakeLists.txt          # op2mapper_core library + the OP2MapperQt program (+ optional self-test)
    ├── main.cpp                # single instance, log, splash, main window
    ├── app.qrc, app.rc         # resources; Windows icon
    ├── resources/
    │   ├── blankmaps/          # blank_WxH.map templates for New Map
    │   ├── images/             # icons, Settings/About banner, New Map biome pictures
    │   ├── sounds/             # splash sound
    │   ├── op2launcher/        # bundled op2launcher.exe + Launcher.dll
    │   ├── OP2MapperQt.png     # 256 px icon (Linux AppImage)
    │   └── linux/              # .desktop entry for the AppImage
    ├── tests/selftest.cpp      # headless self-test of the editing core
    └── src/
        │   ── Application shell ──
        ├── MainWindow          # menus, file flows, exports, edit-mode palettes (fMain)
        ├── Sidebar             # right-hand info panel (fMain sidebar)
        ├── SettingsDialog      # File > Settings and Help > About (fSettings, fAbout)
        ├── NewMapDialog        # File > New Map (fNewMap)
        │   ── Editing ──
        ├── Editor              # tool/mode state machine (GlobalVariables + fMain partials)
        ├── MapView             # map panel: zoom, pan, overlays, mouse input (fMain.MapPanel)
        ├── MapDocument         # map + tilesets + undo stack + commands (Commands.vb, UndoRedo)
        │   ── Rendering ──
        ├── MapRenderer         # render modes, grids, coordinates (MapRenderer.vb)
        ├── TileImageCache      # tileset BMP loading and scaled tile strips
        ├── CellTypeInfo        # colours, labels and list texts per cell type
        │   ── Tool palettes ──
        ├── ToolPalette         # shared base: positions, snapping, layout, title menu
        ├── TilesetWindow, TilesetGrid   # Tile Set (fTileSet)
        ├── CellTypesWindow     # Cell Types (fCellTypes)
        ├── TileSelectWindow    # Tile Selection (fTileSelect)
        ├── TileGroupsWindow    # Tile Groups (fTileGroups)
        ├── MiniMap             # Mini Map (fMiniMap)
        ├── TerrainWindow       # Terrain Brush (fTerrain)
        │   ── Files and support ──
        ├── Op2Lib              # wrapper over op2maplib's C API
        ├── TileGroupIO         # tile-group .json / .bmp
        ├── MapJsonIO, JsonWriter  # full-map JSON export/import (OP2MapJsonTools format)
        ├── BlankMaps           # extraction of the blank templates
        ├── Settings, IniFile   # Mapper3.ini
        ├── AppLog              # OP2Mapper.log
        ├── OP2Launcher         # launch Outpost 2 / DLL picker; sound playback
        └── AppInfo.h           # name, version (from CMake), OP2Mapper3 release matched
```

External libraries:

- **OP2Utility** reads and writes `.map` files and tileset bitmaps. The app reads and writes map files through `QFile` and OP2Utility's memory streams, so paths with non-ASCII characters work.
- **op2maplib** (OP2MappingLib) does terrain blending, cliffs and passable-region analysis. It is built as a static library from its own CMake project; its blend profile `profiles/op2.json` is embedded as `:/op2maplib/op2.json`.

## 2. How the parts fit

```
   .map file ──► MapDocument::load ──► OP2Utility::Map + TileImageCache + QUndoStack
                         │  regionChanged / edited / dirtyChanged
                         ▼
 Tool palettes ──► Editor (modes, tools, clipboard, tile group, terrain, cliff) ◄── MapView (mouse)
                         │  MapCommand objects                      │ paints with
                         ▼                                          ▼
                   MapDocument::execute ─────────────────────► MapRenderer
                                                                    ▲
 MainWindow: menus, sidebar, status bar, file flows, exports ───────┘
```

- **Load:** `MainWindow::loadMapFile` asks `MapDocument::load` to build the map and its tileset cache in locals; they are swapped in only on success, so a failed load leaves the open map and its history untouched. `Editor::onMapLoaded` then resets tools and view state, as OP2Mapper3 does on every load.
- **Edit:** `MapView` turns a left click into `Editor::mapLeftPress` etc. The Editor decides what the click means for the active mode and creates `MapCommand`s, which `MapDocument::execute` applies and records. Everything painted between mouse down and up is one undo step (`beginStroke` / `endStroke`).
- **Render:** `MapView::paintEvent` draws the visible tiles through `MapRenderer`, then the overlays (enclosed areas, paste and tile-group previews, copy rectangle, brush outline, cliff preview, selected tile). Zoomed out, the whole map layer is cached as one image and patched per edited tile.
- **Save:** `MapDocument::writeMapFileSafely` serialises to memory, checks the bytes read back as the same map, then writes through `QSaveFile` (temp file plus atomic rename).
- **Palettes** are top-level `Qt::Tool` windows created once by `MainWindow` and shown or hidden with Edit Mode. They talk to the `Editor` (tools, clipboard, tile group) and listen to its signals.

## 3. Core components

### 3.1 Editor (`Editor.h/.cpp`)

All editing state lives here: edit mode, Single Tile Edit Mode, render mode and overlays, the selected tile, the tile and cell-type tools, copy mode (stages 0 to 2), the clipboard (raw mapping indices + cell types), paste mode, the loaded tile group and placement mode, the terrain brush and stroke blend map, Draw Cliff points, and the enclosed-area highlight.

The mode rules follow OP2Mapper3:

- Entering a mode leaves the ones it conflicts with (copy leaves paste and tile-group placement, the brush leaves all three, ...).
- Mouse dispatch order: paste, tile-group placement, then (edit mode only) copy selection, cliff, terrain brush, tile or cell-type painting.
- Esc cancels, in this order: cliff points, tile-group placement, copy selection, paste.
- Restricted render modes (CellTypeBasic / CellTypeAll) refuse tile-related actions.
- No tile mappings are ever added: a tileset tile without a mapping can't be placed, and pastes and tile groups write raw mapping indices.

`toolLabels()` builds the sidebar's "Editing" labels and icon from the state.

### 3.2 MapDocument and commands (`MapDocument.h/.cpp`)

Owns the `OP2Utility::Map`, its `TileImageCache` and a `QUndoStack` capped at 500 steps. Coordinates are 0-based (x, y); the `.map` 32-column block layout is hidden behind `mappingIndexAt` / `cellTypeAt`.

Commands (`MapCommand` subclasses) are applied before they are recorded:

| Command | Used for |
|---|---|
| `ChangeTileMappingCommand` | tile tool, one tile |
| `ChangeCellTypeCommand` | cell-type tool, one tile |
| `PasteTilesCommand` | paste (cell types optional, never on the bottom row) |
| `PlaceTileGroupCommand` | tile-group placement (tiles only) |
| `TileChangesCommand` | terrain brush, Clean Up Transitions, Draw Cliff |
| `ChangeCellTypesBulkCommand` | Find & Fix Enclosed Areas |
| `CompositeCommand` | a drag stroke with several commands |

Dirty state comes from the undo stack's clean index. `markNewUnsaved()` makes the clean state unreachable, so a new or imported map counts as unsaved even after undoing everything.

### 3.3 MapView and MapRenderer

`MapView` keeps a zoom level and the pixel offset of the map origin, like OP2Mapper3's map panel. Tiles are drawn at a whole number of pixels (`round(32 * zoom)`), which avoids seams. The wheel steps through 10, 25, 50, 75, 100, 125 and 150% around the cursor, right-drag pans (a right click that moved less than 5 px opens the context menu), and Ctrl+wheel resizes the terrain brush.

`MapRenderer` draws the five render modes, the grids and coordinate labels, and whole-map images for the mini map, exports and the overview. Colours and labels come from `CellTypeInfo`, copied from OP2Mapper3.

### 3.4 Tool palettes

`ToolPalette` gives every palette:

- **Positions:** `<Key>Left/Top` in Mapper3.ini, restored only if the title bar lands on a screen.
- **Default layout:** palettes without a usable position get OP2Mapper3's smart layout, on another monitor if there is one.
- **Snapping (Windows):** to screen edges and other palettes while moving.
- **Title-bar menu:** right-click the caption or press Alt+Space for Always On Top choices plus the palette's own extras.

### 3.5 op2maplib (`Op2Lib.h/.cpp`)

The app builds op2maplib 0.3.4. Its map generator (`om_mapgen_*`) is compiled in but not used by the app yet.

- **Profile:** loaded once, on first use.
- **Blend maps:** a fresh `BlendMap` is built from the current map for each brush stroke, each Clean Up Transitions and each cliff, so it never needs syncing with other edits.
- **Changes:** results come back as `om_change` records and become one `TileChangesCommand`. Suggested cell types are applied except on the locked bottom row.
- **Regions:** `findRegions` runs the passable-region analysis for the enclosed-area tools.

## 4. Data stores

| File | Where | Contents |
|---|---|---|
| `.map` | anywhere | the document (OP2Utility format) |
| `Mapper3.ini` | next to the exe | `[Paths]` OP2Path, WorkingPath, Tilegroups; `[Window]` recent files, window and palette positions, always-on-top flags, paste / Tile Selection / terrain settings. Same rules as OP2Mapper3, so both apps can share it. |
| `OP2Mapper.log` | next to the exe | start / stop, maps opened and saved, launcher installs, op2maplib status; rolled to `OP2Mapper.old.log` past 1 MB |
| Blank templates | `<exe>/BlankMaps` or `%LOCALAPPDATA%/OP2Mapper/BlankMaps` | extracted from the resources for New Map |
| Tile groups | `<Tilegroups>/<folder>/<name>.json` (+ `.bmp`) | reusable stamps of mapping indices; exports go to `Exported/` |
| Exported images | next to the map | `<map>_<RenderMode>[_suffix].jpg`, `<map>_overview.jpg`; screenshots in WorkingPath |

## 5. Differences from OP2Mapper3

- JSON map export and import are extra (OP2Mapper3 has them disabled).
- The auto-updater is not ported.
- New Map is a modal dialog.
- Main-window edge snapping is not ported. Palette snapping and the title-bar menu are Windows-only.

## 6. Testing

There is no unit-test framework. `tests/selftest.cpp` (CMake option `OP2MAPPER_BUILD_SELFTEST`) drives the real `Editor`, `MapDocument`, `MapView` and `MainWindow` headlessly against an Outpost 2 install. It keeps its own `Mapper3.ini` in the output folder (via the `OP2MAPPER_INI` environment variable, which overrides the INI location), writes the Outpost 2 folder into it, answers message boxes automatically, and writes rendered images to check by eye. Run it with `QT_QPA_PLATFORM=offscreen` (and `QT_QPA_FONTDIR` pointing at a font folder so text renders).

### Continuous integration and releases

`.github/workflows/build.yml` builds the app on Windows (MSVC), Linux (GCC) and macOS (Clang) with Qt 6.10 on every push and pull request. It checks out OP2Utility into `OP2Utility/` and OP2MappingLib beside the repository, both at pinned commits (`OP2UTILITY_REF`, `OP2MAPPINGLIB_REF`), and uploads ready-to-run packages:

- Windows: `OP2MapperQt-<ver>-Windows-x64.zip`, made with windeployqt plus the MSVC runtime DLLs
- macOS: `OP2MapperQt-<ver>-macOS-<arch>.zip`, an app bundle made with macdeployqt (not signed)
- Linux: `OP2MapperQt-<ver>-Linux-x86_64.AppImage`, made with linuxdeploy and its Qt plugin on Ubuntu 22.04

Pushing a tag `v<version>` runs the same builds and creates a draft GitHub release with the packages attached; its notes are edited and the release published by hand. The tag should match `project(... VERSION ...)`.

Portability notes the build relies on:

- Windows-only code (sounds, palette snapping and title-bar menu, hidden folders) is behind `Q_OS_WIN`; Windows builds define `NOMINMAX`.
- MSVC compiles with `/utf-8` (message texts contain non-ASCII characters) and links op2maplib against the same C runtime as Qt.
- Qt 6.8 still works locally (`QImage::mirrored` is used before 6.9), but the current macOS SDK needs Qt 6.9 or later.

## 7. Glossary

- **Tile mapping:** an entry in the map's `tileMappings` table (tileset index + graphic index). Each tile stores a mapping index, not an image.
- **Cell type:** a tile's passability / terrain class (0-31), separate from its graphic.
- **Bottom row:** the map's last row, not shown in game; its cell types stay Impassable.
- **Tile group:** a saved rectangle of mapping indices, placed as a stamp.
- **Edit Mode:** the state in which the tool palettes are open and edits are allowed.
- **Render mode:** GameMap, CellTypeOverlay, CellTypeOverlayAll, CellTypeBasic or CellTypeAll.
- **Enclosed area:** a passable region units can't reach from the main play area.
