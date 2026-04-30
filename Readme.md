## dolphin-3ds

This fork adds 3D screenshot capture support to Dolphin.

### Added features

- Added a 3D screenshot command under `Emulation > Take 3D Screenshot`.
- Added a `Take 3D Screenshot` hotkey action with the default shortcut `Ctrl+F9`.
- Added a `Disable Shadows` graphics enhancement toggle.
- Captures the next available frame of perspective 3D geometry from Dolphin's native vertex stream.
- Writes captures as Wavefront OBJ files with matching MTL material files.
- Exports referenced texture images as PNG files when texture data is available.
- Stores 3D captures in Dolphin's dump objects directory, grouped by game ID.
- Shows an on-screen status message when a 3D screenshot is saved or when no geometry could be captured.

### Output format

3D screenshots are saved with names like:

```text
GAMEID_3d_YYYY-MM-DD_HH-MM-SS.obj
GAMEID_3d_YYYY-MM-DD_HH-MM-SS.mtl
tex_0001_texture-name.png
```

The OBJ output includes vertex positions, normals, texture coordinates, vertex colors, and material assignments. Geometry is captured from triangle and triangle strip draws after clipping against the active perspective projection.

### Notes

This feature is intended for extracting visible 3D scene geometry from supported games. Orthographic draws, UI elements, and frames with no captured 3D geometry are skipped.

The `Disable Shadows` option skips draw calls that match common projected shadow rendering patterns. It can help produce cleaner captures, but it may also remove other dark translucent effects in some games.
