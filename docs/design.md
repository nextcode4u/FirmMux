# FirmMux UI and State Design

## Screens

Top screen
- Left 75%: preview panel for the currently highlighted item
- Right 25%: target selector list (scrollable if needed)
- No other scrollable lists on the top screen
- Status bar at the bottom edge (time, Wi-Fi, battery)
- Optional top background image is drawn behind the UI panels

Bottom screen
- Primary navigation area
- Scrollable content lists and grids
- Options overlay appears on the bottom screen when START is pressed
- Optional bottom background image is drawn behind the UI

## Input

- L/R: cycle targets globally with wraparound
- D-pad/Circle Pad: navigate bottom content
- A: launch/open (placeholder messaging only)
- B: back (directory up or previous screen)
- START: open options overlay
- While options open: L/R page options list, B closes

## Target Types and Behavior

system_menu
- First entry is "Return to HOME"
- Remaining entries are system titles (friendly names when available)
- Selecting a system title launches it
- Return to HOME shows a System Info panel in the preview

installed_titles
- Enumerated from SD/NAND (user titles only)
- List layout on the bottom screen (text only)
- Preview shows title icon and TitleID

homebrew_browser
- File browser rooted at configured path
- Shows .3dsx entries
- Preview uses SMDH title/description/icon when available

rom_browser
- File browser rooted at configured path
- Folder-first sorting
- Files filtered by configured extensions
- List shows filenames; preview shows banner icon/title when enabled

## State Machine

States
- Boot
- Main (per-target content)
- OptionsOverlay
- Exit

Transitions
- Boot -> Main after config/state load
- Main -> OptionsOverlay on START
- OptionsOverlay -> Main on B
- Main -> Exit on system_menu A select

## Persistence

- Config at /3ds/FirmMux/config.yaml
- State at /3ds/FirmMux/state.json
- On missing config, create default
- On malformed config, rename to .bak and regenerate
- State stores last target, per-target directory path + selection + scroll, selected theme, selected backgrounds, and background visibility

## Backgrounds

- Background images live under:
  - /3ds/FirmMux/backgrounds/top
  - /3ds/FirmMux/backgrounds/bottom
- Options includes:
  - Top background picker
  - Bottom background picker
  - Background visibility picker (controls panel opacity when a background is present)

## Caching

- Build directory caches on demand while browsing
- No full SD scans at boot
- NDS banner/icon cache stored under /3ds/FirmMux/cache/nds keyed by path/size/mtime
- Cache clears/rebuilds available from Options
- Banner display mode for NDS is per-target (Sprite or Title Data)

## Launch flow

- system_menu: exit to HOME
- installed_titles: launch selected title by TitleID (AM/NS path)
- homebrew_browser: chainload selected .3dsx via hbloader
- rom_browser: write handoff to `sd:/_nds/firmux/launch.txt` then jump to FirmMuxBootstrapLauncher CIA
  - Handoff format (text):
    ```
    sd:/roms/nds/Game.nds
    ```
  - Launcher is selected by product code (`FMUXBOOT`) or `loader_title_id` in config

## Autoboot

- Intended for Luma Hbmenu autoboot
- On cold boot, holding B bypasses FirmMux and returns to HOME
