# FirmMux UI and State Design

## Screens

Top screen
- Left 75%: preview panel for the currently highlighted item
- Right 25%: target selector list (scrollable if needed)
- No other scrollable lists on the top screen

Bottom screen
- Primary navigation area
- Scrollable content lists and grids
- Options overlay appears on the bottom screen when START is pressed

## Input

- L/R: cycle targets globally with wraparound
- D-pad/Circle Pad: navigate bottom content
- A: launch/open (placeholder messaging only)
- B: back (directory up or previous screen)
- START: open options overlay
- While options open: L/R page options list, B closes

## Target Types and Behavior

system_menu
- Single selectable entry
- A exits to HOME Menu

installed_titles
- Mock list of 200 titles
- Auto-bucketed into # and A-Z
- Grid layout on the bottom screen
- Alphabet index strip on the left side of the grid

homebrew_browser
- File browser rooted at configured path
- Shows .3dsx entries

rom_browser
- File browser rooted at configured path
- Folder-first sorting
- Files filtered by configured extensions

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
- State stores last target, and per-target directory path + selection + scroll

## Caching

- Build directory caches on demand while browsing
- No full SD scans at boot
- NDS banner/icon cache stored under /3ds/FirmMux/cache/nds keyed by path/size/mtime
- Cache clears/rebuilds available from Options

## Launch flow

- system_menu: exit to HOME
- installed_titles: launch selected title by TitleID (AM/NS path)
- homebrew_browser: chainload selected .3dsx via hbloader
- rom_browser: write handoff to `sd:/_nds/firmux/launch.txt` then jump to FirmMuxBootstrapLauncher CIA
  - Handoff format (text):
    ```
    sd:/roms/nds/Game.nds
    ```
  - Launcher TitleID constant: `FMUX_BOOTSTRAP_TITLEID` in `include/fmux.h`

## Autoboot

- Intended for Luma Hbmenu autoboot
- On cold boot, holding B bypasses FirmMux and returns to HOME
