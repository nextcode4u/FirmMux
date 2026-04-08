# FirmMux Usage

## Main controls

- `D-Pad` / `Circle Pad`: move selection
- `A`: open folder or launch selected item
- `B`: go back / go up one folder
- `L` / `R`: switch tabs
- `X`: change sort order in browser lists
- `START`: open Settings / Options
- `SELECT`: open Stats / Favorites from supported browser views
- Hold `SELECT`: add the highlighted launchable entry to Favorites

## Where SELECT works

Short `SELECT` and hold `SELECT` apply in launchable browser views:

- `3DS Titles`
- `NDS Titles`
- `Homebrew Launcher`
- RetroArch system tabs
- System Menu title rows

Notes:

- `START` always opens Settings
- `SELECT` always opens Stats / Favorites
- They are separate menus

## Browser behavior

### 3DS Titles

- `A`: launch selected installed title
- Includes installed CTR titles and DSiWare
- When firmware readiness support is present, FirmMux launches these directly without requiring a HOME round-trip
- `X`: change sort order

### System Menu

- `A` on `Return to HOME`: return to HOME
- `A` on `Turn Off Console`: power off
- `A` on a title row: launch selected system title
- `X`: change sort order

### Homebrew Launcher

- `A` on a `.3dsx`: launch it
- Hold `SELECT`: add the highlighted `.3dsx` to Favorites

### NDS Titles

- `A`: launch selected `.nds`
- `Y`: NDS game options
- Hold `SELECT`: add the highlighted game to Favorites

### RetroArch tabs

- `A`: launch selected ROM
- `Y`: RetroArch ROM options
- Hold `SELECT`: add the highlighted ROM to Favorites

## Stats / Favorites

Open with short `SELECT`.

Menu rows:

- `Close`
- `Last played`
- Favorites list

Controls:

- `A` on `Close`: close the menu
- `A` on `Last played`: launch the last played item if available
- `A` on a favorite: launch that favorite
- `X` on a favorite: remove it from Favorites
- `B`: close the menu
- `SELECT`: close the menu

## Favorites

- Favorites support:
  - ROMs
  - installed 3DS titles
  - system-menu title rows
  - homebrew `.3dsx` entries
- Duplicate favorites are ignored
- Favorites persist across launches

## Last Played

- Last Played updates only when something is actually launched
- It supports:
  - ROMs
  - installed 3DS titles
  - system-menu title rows
  - homebrew `.3dsx` entries

## Useful browser actions

- Directories are opened with `A`
- `B` returns to parent folder
- Sort mode changes with `X`
- NDS options are on `Y`
- RetroArch ROM options are on `Y`
