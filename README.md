# OP2MapperQt

![Screenshot](https://images.outpostuniverse.org/OP2MapperQt.png)

## What is it?

OP2MapperQt is a cross-platform map editor for Outpost 2: Divided Destiny. Create new maps, edit tile graphics and cell types, work with tile groups, and export maps as images or JSON — a Qt/C++ port of [OP2Mapper3](https://github.com/leviathan400/OP2Mapper3).

It requires an Outpost 2 (1.4.1) folder. Tile graphics are loaded from the game's well BMP files (loose files in `OPU\base\tilesets`, or from .vol archives).

## Features

OP2MapperQt follows [OP2Mapper3](https://github.com/leviathan400/OP2Mapper) 0.7.0: same tools, menus, messages and settings file.

### Terrain tools (via [op2maplib](https://github.com/OutpostUniverse/OP2MappingLib))
- **Terrain Brush** — paint mud, rock, sand or vegetation; borders blend with the game's own transition tiles. Brush size 3–25 (Ctrl+wheel). **Clean Up Transitions** redoes them across the whole map.
- **Draw Cliff** — click points (they snap to cliff slopes), choose the high side and a wall height of 3–6, press Enter. Click the first point again for a closed plateau.
- **Find & Fix Enclosed Areas** — find passable areas units can never reach, highlight them, and make them impassable in one undoable step.

### Editing
- **Edit Mode** opens the tool windows: Tile Set, Cell Types, Tile Selection, Tile Groups, Mini Map and Terrain Brush
- **Tile and cell-type painting** (all 32 cell types); Single Tile Edit Mode or drag painting
- **Copy and paste** — drag or two-click select; pastes bring cell types along (Edit → Paste Includes Cell Types)
- **Tile groups** — browse, preview and place (Ctrl+B); export a copy as a tile group (`.json` + `.bmp`), shared with OP2Mapper3 and [OP2TileGroupTools](https://github.com/leviathan400/OP2TileGroupTools)
- **Undo / redo** for every edit (500 steps); a whole drag or cliff is one step
- The bottom row keeps its Impassable cell type

### Viewing
- **Five render modes**, grid / large grid / coordinate overlays, configurable grid size
- **Zoom** 10%–150% with the mouse wheel (Ctrl+E locks it), right-drag panning
- **Sidebar** with map details, the map's tilesets and the selected tile
- **Mini map** with click-to-jump and drag-to-scroll

### Tool windows
They snap to each other and to screen edges, remember their positions across monitors, and can each be set to stay on top (right-click the title bar).

### Export and launch
- **Image export** at full size, 1/8 or 1/16, with or without grids; a combined **overview** of all render modes; **screenshot** of the view (F12)
- **Map JSON** export/import, compatible with [OP2MapJsonTools](https://github.com/leviathan400/OP2MapJsonToolsLibrary)
- **Launch Outpost 2**, launch it into the DLL picker (installs the bundled op2launcher), or open the game folder

### Safe by design
- Saves are written to memory, checked, then swapped in atomically; a failed save never damages the map
- A map that fails to open leaves the current one untouched
- Settings live in `Mapper3.ini` next to the program (the same file OP2Mapper3 uses); events are logged to `OP2Mapper.log`

## Command line

```
appOP2Mapper.exe [/op2dir "C:\Games\Outpost 2"] [map file]
```

A second launch hands its map to the running copy.

## Building

Qt 6 (Widgets, Network) and CMake 3.16+. Two libraries are expected next to this repository's `OP2Mapper` folder:

- [OP2Utility](https://github.com/OutpostUniverse/OP2Utility) in `OP2Utility/` (map and tileset file I/O)
- [OP2MappingLib](https://github.com/OutpostUniverse/OP2MappingLib) (op2maplib) checked out beside this repository as `OP2MappingLib/` or `op2maplib/`, or passed with `-DOP2MAPLIB_DIR=<path>`. It is built as a static library and its blend profile is embedded.

```
cmake -S OP2Mapper -B build -DCMAKE_PREFIX_PATH=<path-to-Qt>
cmake --build build
```

A headless self-test of the editing core can be built with `-DOP2MAPPER_BUILD_SELFTEST=ON`:

```
QT_QPA_PLATFORM=offscreen op2mapper_selftest "<Outpost 2 folder>" "<map file>" "<output folder>"
```

## Related Projects

- [OP2Mapper3](https://github.com/leviathan400/OP2Mapper) - The VB.net map editor this port is based on
- [OP2TileGroupTools](https://github.com/leviathan400/OP2TileGroupTools) - Tile group JSON format reference
- [OP2MapJsonTools](https://github.com/leviathan400/OP2MapJsonToolsLibrary) - Map ↔ JSON shared library
- [OP2MappingLib](https://github.com/OutpostUniverse/OP2MappingLib) - op2maplib: terrain blending, cliffs and passable regions
- [OP2Utility](https://github.com/OutpostUniverse/OP2Utility) - C++ library for working with OP2 files
