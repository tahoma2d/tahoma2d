# Welcome to Tahoma2D 1.5!

Version 1.5 brings a number of new features, enhancements, and even more fixes which are listed [here](https://github.com/tahoma2d/tahoma2d/releases/tag/v1.5).

### Timeline/Xsheet changed

- Cells and Columns do not have drag bars by default.
- Click an exposed cell or non-empty column then drag to move it.
- Select an empty cell/column and drag to initiate a multi-cell/column selection.
- `ALT` has replaced `CTRL` for selecting frames and animation keys.
- `CTRL` now initiates multi-cell/column selection when the 1st click is an exposed cell/column.
- If you want the old behavior, enable `Preferences` -> `Scene` -> `Show Column and Cell Drag Bars`

### Windows Users

If you are having pen issues with this new version, especially if you use a laptop-tablet/Surface tablet try 1 or both of these workarounds:
- Turn on `Preferences` -> `Touch/Tablet Settings` -> `Use Qt's Native Ink Support*` and restart Tahoma2D.
- Keep the setting above and install `WinTab` drivers. Check your device's manufacturer for appropriate WinTab drivers.

-----

## Upgrading from Tahoma2D 1.2 or Migrating from OpenToonz

If you are coming from Tahoma2D 1.2 or OpenToonz, here are a few things you should note:
- Implicit Holds
  - No need to manually extend a frame. An exposed frames is shown until the next exposed frame.
  - Use `Stop Frame Hold` to end a hold without exposing another frame.
  - Right-click any cell and select `Toggle Implicit Hold` to switch modes.
  - Converted existing scene to/from using implicit holds with `Scene` -> `Convert to use xxxx Holds`.
- Some OpenToonz features and settings are hidden.
  - Enable it with `Preferences` -> `General` -> `Show Advanced Preferences and Options` then restart.