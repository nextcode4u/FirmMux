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

retroarch_system
- File browser rooted at the system's configured ROM folder
- Missing folder shows a "Missing folder" state
- Empty folder shows a "No games found" state
- Launch writes a RetroArch handoff file and then chainloads RetroArch when possible
- If chainload is unavailable, FirmMux exits to hbmenu with instructions

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

## RetroArch Backend Files

FirmMux uses external RetroArch backend config under:
- /3ds/emulators/retroarch_rules.json
- /3ds/emulators/emulators.json
- /3ds/emulators/launch.json
- /3ds/emulators/rom_options.json
- /3ds/emulators/filter_favorites.txt (video filter favorites)
- /3ds/emulators/log.txt

Invalid or missing JSON is regenerated automatically.

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
- rom_browser: write handoff to `sd:/_nds/firmux/launch.txt` then launch FirmMuxBootstrapLauncher (CIA or 3DSX prep)
  - Handoff format (text):
    ```
    rom=sd:/roms/nds/Game.nds
    cheats=0/1
    widescreen=0/1
    ap_patch=0/1
    cpu_boost=0/1
    vram_boost=0/1
    async_read=0/1
    card_read_dma=0/1
    dsi_mode=0/1
    ```
  - CIA launcher is selected by product code (`CTR-P-FMBP`) or `loader_title_id` in config
  - 3DSX launcher lives at `sd:/3ds/FirmMux/firmux-bootstrap-prep.3dsx`
- retroarch_system:
  - resolve system key from emulators.json romFolder prefix, else parent folder
  - resolve core from retroarch_rules.json
  - write handoff to `sd:/3ds/emulators/launch.json`
- if RetroArch exists and chainload is available, chainload `sd:/3ds/FirmMux/emulators/retroarch.3dsx`
  - otherwise exit to hbmenu with instructions to launch RetroArch

## Custom RetroArch Build

- Custom RetroArch 3DSX is built from source in `retroarch_src/RetroArch-master/`
- Build script: `tools/build_retroarch_with_firmux.sh`
- Output: `SD/3ds/FirmMux/emulators/retroarch.3dsx`

## Autoboot

- Intended for Luma Hbmenu autoboot
- On cold boot, holding B bypasses FirmMux and returns to HOME
