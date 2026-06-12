# OP2MapperQt

![Screenshot](https://images.outpostuniverse.org/OP2MapperQt.png)

## What is it?

OP2MapperQt is a cross-platform map editor for Outpost 2: Divided Destiny. Create new maps, edit tile graphics and cell types, work with tile groups, and export maps as images or JSON — a Qt/C++ port of [OP2Mapper3](https://github.com/leviathan400/OP2Mapper3).

It requires an Outpost 2 (1.4.1) folder. Tile graphics are loaded from the game's well BMP files (from .vol archives or loose files).

## Features

### Map editing
- **Open / Save / New** — create maps from 10 bundled blank templates (64×64 up to 512×256)
- **Tile painting** — pick a tile from any loaded tileset, click or drag-paint onto the map
- **Cell-type painting** — full set of 32 OP2 cell types (passability / pathing layer)
- **Copy & paste** — two-click region select or Shift+drag, paste with live ghost preview
- **Undo / redo** for every edit; drag strokes undo as a single step
- **Bottom-row protection** — the special impassable border row can't be edited

### Tile groups
- **Browse** tile groups by folder, with BMP previews and header details
- **Place** any group onto the map (Ctrl+B), under full undo
- **Export** a copied region as a tile group (`.json` + `.bmp` pair)
- Format is shared with [OP2Mapper3](https://github.com/leviathan400/OP2Mapper3) and [OP2TileGroupTools](https://github.com/leviathan400/OP2TileGroupTools) — files round-trip between all three

### Viewing
- **5 render modes** — game map, cell-type overlays (with or without labels), cell-type only
- **Overlays** — small grid (Ctrl+G), large grid (Ctrl+L), tile coordinates (Ctrl+D)
- **Zoom** (Ctrl+wheel) and pan, with a click-to-navigate minimap
- Floating tool palettes (Tile Set, Cell Types, Tile Selection, Tile Groups, Minimap) that remember their positions

### Import / export
- **Export to JSON** — full map as JSON, byte-compatible with [OP2MapJsonTools](https://github.com/leviathan400/OP2MapJsonToolsLibrary)
- **Import from JSON** — rebuild a map from a JSON file
- **Export image** — JPG / PNG / BMP at full size down to 1/16 scale

### Quality of life
- Recent files, drag-drop a `.map` onto the window to open it
- Unsaved-changes prompts, window layout persistence
- Launch Outpost 2 from the Tools menu

## Building

Qt 6 (Widgets) and CMake 3.16+. The [OP2Utility](https://github.com/OutpostUniverse/OP2Utility) C++ library is vendored in this repository and linked automatically.

```
cmake -S OP2Mapper -B build -DCMAKE_PREFIX_PATH=<path-to-Qt>
cmake --build build
```

## Related Projects

- [OP2Mapper3](https://github.com/leviathan400/OP2Mapper) - The VB.net map editor this port is based on
- [OP2TileGroupTools](https://github.com/leviathan400/OP2TileGroupTools) - Tile group JSON format reference
- [OP2MapJsonTools](https://github.com/leviathan400/OP2MapJsonToolsLibrary) - Map ↔ JSON shared library
- [OP2Utility](https://github.com/OutpostUniverse/OP2Utility) - C++ library for working with OP2 files
