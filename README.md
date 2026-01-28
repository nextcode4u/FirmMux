# FirmMux 3DS

FirmMux is a unified front-end that multiplexes execution between CTR, TWL, the System Menu, Homebrew Launcher-style browsing, and RetroArch-backed systems (Atari 2600, Atari 5200, Atari 7800, ColecoVision, Amstrad CPC, Game Boy/Color, Genesis/Mega Drive, Game Gear, Intellivision, Sord M5, NES, Neo Geo Pocket/Color, PokeMini, SG-1000, Master System, SNES, TurboGrafx-16/PC Engine, WonderSwan/Color).

## Requirements (Linux / WSL / MSYS2)

- devkitPro with devkitARM and libctru
- 3ds-dev tools installed via devkitPro pacman

Example install:

```
sudo dkp-pacman -Syu
sudo dkp-pacman -S devkitARM libctru citro2d citro3d 3ds-dev
```

## Build

```
make
```

The output `.3dsx` will be in the project root.

## Run

Copy `FirmMux.3dsx` to your SD card and launch from the Homebrew Launcher.

Config is stored at `/3ds/FirmMux/config.yaml` and state at `/3ds/FirmMux/state.json`.

## Pure RetroArch Backend (3DSX Only)

FirmMux acts as the frontend. RetroArch (3DSX) is the backend.

- RetroArch entry: `sd:/3ds/FirmMux/emulators/retroarch.3dsx` (custom FirmMux RetroArch build)
- RetroArch cores/system: `sd:/retroarch/` (cores in `sd:/retroarch/cores/`, BIOS/system in `sd:/retroarch/system/`)
- No CIA titles for emulators
- No standalone emulator binaries
- No user-facing RetroArch menus; FirmMux drives content launch

On ROM launch:
1. FirmMux resolves the system key
2. FirmMux resolves a RetroArch core
3. FirmMux writes a handoff file
4. FirmMux chainloads RetroArch if available, otherwise exits to hbmenu with instructions

### External RetroArch Config Files

These live under `sd:/3ds/emulators/` and are the only files FirmMux writes there:

- `retroarch_rules.json`
- `emulators.json`
- `launch.json`
- `log.txt` (only when RetroArch logging is enabled)

FirmMux regenerates defaults automatically if JSON is missing or invalid.

### Emulator Systems and ROM Folders

FirmMux expects these systems under `sd:/roms/`:

`a26 a52 a78 col cpc gb gen gg intv m5 nes ngp pkmni sg sms snes tg16 ws`

`nds` and BIOS folders are intentionally excluded from RetroArch systems.

### Emulator Tabs and Settings

Tab order is always:

1. System Menu
2. 3DS Titles
3. NDS Titles
4. Homebrew
5. Emulator tabs (enabled systems only)

In Options:

- `Emulators...` opens per-system settings.
- Each system page includes an Enabled toggle.
- Each system page includes a ROM folder assignment.
- `RetroArch log: On/Off` toggles `sd:/3ds/emulators/log.txt`
- `RetroArch backend requirements` shows required paths and BIOS notes

## Backgrounds

Background images are independent from themes and can be picked at runtime.

- Put PNGs in:
  - `sd:/3ds/FirmMux/backgrounds/top/`
  - `sd:/3ds/FirmMux/backgrounds/bottom/`
- Backgrounds are stretched to fit.
- Use standard RGB PNGs; alpha is ignored.
- Recommended sizes:
  - top: `400x240`
  - bottom: `320x240`

In FirmMux:
- Options → Top background / Bottom background opens a picker.
- Options → Background visibility controls how strongly UI panels overlay the background (higher means more background showing).
- Default background visibility is 50%.

## NDS launching via FirmMuxBootstrapLauncher

- Install the FirmMuxBootstrapLauncher CIA on SD.
- Ensure `sd:/_nds/nds-bootstrap.nds` is present.
- Place ROMs under `sd:/roms/nds/`.
- Selecting an `.nds` in FirmMux writes `sd:/_nds/firmux/launch.txt` with a single line `sd:/roms/nds/<file>.nds` (newline terminated) then jumps to the launcher CIA.
- Use “Select NDS launcher” in Options to bind the launcher by product code `CTR-P-FMBP`, or set `loader_title_id` and optional `loader_media` on the NDS target in `config.yaml`.
- FirmMux Bootstrap Launcher releases: https://github.com/nextcode4u/firmux-bootstrap-prep/releases
- NDS card entries launch a separate card launcher title; use “Select NTR launcher” in Options (product code `NTR Launcher`) or set `card_launcher_title_id` (and optional `card_launcher_media`) on the NDS target.
- NTR Launcher can be installed from: https://github.com/ApacheThunder/NTR_Launcher/releases (install `NTR Launcher.cia`).
- NTR Forwarder pack (includes nds-bootstrap for both NTR Forwarder and FirmMux Bootstrap Launcher): https://github.com/RocketRobz/NTR_Forwarder/releases
- YANBF releases: https://github.com/YANBForwarder/YANBF/releases
- FirmMux Bootstrap depends on YANBF Bootstrap (`bootstrap.cia`).

## Homebrew launching

- FirmMux launches `.3dsx` via Rosalina `hb:ldr` (Luma).

## Autoboot and bypass

- Set FirmMux as the autoboot target in Luma Hbmenu autoboot.
- On cold boot, hold `B` to bypass FirmMux and return to HOME Menu.

## Themes

- Set `theme: <name>` at the top of `sdmc:/3ds/FirmMux/config.yaml`.
- Or use Options → Themes... to pick from installed themes (saved in `state.json`).
- Themes control UI colors and layout sizing. Top/bottom backgrounds are configured separately via the Background options.
- Theme files live at `sdmc:/3ds/FirmMux/themes/<name>/theme.yaml`.

Theme schema (colors are hex RGB or ARGB, e.g. `0xRRGGBB` or `0xAARRGGBB`):

```yaml
name: Default
list_item_h:
line_spacing:
status_bar_h:
top_bg:
bottom_bg:
panel_left:
panel_right:
preview_bg:
text_primary:
text_secondary:
text_muted:
tab_bg:
tab_sel:
tab_text:
list_bg:
list_sel:
list_text:
option_bg:
option_sel:
option_text:
option_header:
overlay_bg:
help_bg:
help_line:
help_text:
status_bg:
status_text:
status_icon:
status_dim:
status_bolt:
toast_bg:
toast_text:
list_text_offset_y:
tab_text_offset_y:
option_text_offset_y:
help_text_offset_y:
status_text_offset_y:
```

Sample themes shipped in SD layout:
`default`, `amber`, `cobalt`, `dark_material`, `ember`, `glacier`, `graphite`, `midnight`, `mint`, `neon_cyber`, `paper_light`, `sage`, `sandstone`, `sunset`, `synthwave_hass`.

## Custom RetroArch (FirmMux Build)

We ship a custom RetroArch 3DSX build (salmander-based) that reads `sd:/3ds/emulators/launch.json` and immediately boots the selected core + ROM.

To build it:

1. Place RetroArch source under:
   `retroarch_src/RetroArch-master/`
   Source: https://github.com/libretro/RetroArch/tree/master

2. Run:
```
tools/build_retroarch_with_firmux.sh
```

This outputs:
`SD/3ds/FirmMux/emulators/retroarch.3dsx`

RetroArch dependency folders expected on SD:
`sd:/retroarch/` (cores in `sd:/retroarch/cores/`, BIOS/system in `sd:/retroarch/system/`)

RetroArch 3DSX data bundle required:
Use the 3DSX release (not CIA) from:
```
https://buildbot.libretro.com/stable/
```
Copy the `retroarch/` folder from the 3DS 3DSX build to the SD root:
`sd:/retroarch/` (must include `cores/`, `system/`, and supporting assets).

## SD Folder Layout (Required)

```
sd:/
  3ds/
    FirmMux.3dsx
    FirmMux.smdh
    FirmMux/
      backgrounds/
        top/
        bottom/
      emulators/
        retroarch.3dsx
        retroarch.smdh
      themes/
        <theme-name>/theme.yaml
      ui sounds/
    emulators/
      retroarch_rules.json
      emulators.json
      launch.json
      log.txt
  retroarch/
    cores/
      *_libretro.3dsx
    system/
```

## On-Device Testing (hbmenu)

1. Copy `FirmMux.3dsx` to `sd:/3ds/`.
2. Ensure RetroArch exists at `sd:/3ds/FirmMux/emulators/retroarch.3dsx`.
3. Ensure RetroArch dependency folders exist at `sd:/retroarch/` (cores/system).
3. Place ROMs under the system folders in `sd:/roms/` (see list above).
4. Launch FirmMux from hbmenu.
5. In Options:
6. Open `Emulators...` and confirm systems and folders.
7. Optionally enable `RetroArch log: On`.
8. Go to an emulator tab and select a ROM.
9. If chainloading is supported, RetroArch should open.
10. Otherwise FirmMux exits with a message telling you to launch RetroArch from hbmenu.
