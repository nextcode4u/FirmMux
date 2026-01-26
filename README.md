# FirmMux 3DS

FirmMux is a unified front-end that multiplexes execution between CTR, TWL, the System Menu, and a Homebrew Launcher-style browser.

## Requirements (WSL Ubuntu)

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

## Alpha release

Current alpha: v0.1.0-alpha1. See `docs/release-v0.1.0-alpha1.md`.

Note: emulator targets (NES/GBA/SNES/etc) are not implemented yet.

## NDS launching via FirmMuxBootstrapLauncher

- Install the FirmMuxBootstrapLauncher CIA on SD.
- Ensure `sd:/_nds/nds-bootstrap.nds` is present.
- Place ROMs under `sd:/roms/nds/`.
- Selecting an `.nds` in FirmMux writes `sd:/_nds/firmux/launch.txt` with a single line `sd:/roms/nds/<file>.nds` (newline terminated) then jumps to the launcher CIA.
- Use “Select NDS launcher” in Options to bind the launcher by product code `FMUXBOOT`, or set `loader_title_id` and optional `loader_media` on the NDS target in `config.yaml`.
- NDS card entries launch a separate card launcher title; use “Select NTR launcher” in Options (product code `NTR Launcher`) or set `card_launcher_title_id` (and optional `card_launcher_media`) on the NDS target.
- NTR Launcher can be installed from: https://github.com/ApacheThunder/NTR_Launcher/releases (install `NTR Launcher.cia`).
- NTR Forwarder pack (includes nds-bootstrap for both NTR Forwarder and FirmMux Bootstrap Launcher): https://github.com/RocketRobz/NTR_Forwarder/releases

## Homebrew launching

- FirmMux launches `.3dsx` via Rosalina `hb:ldr` (Luma).

## Autoboot and bypass

- Set FirmMux as the autoboot target in Luma Hbmenu autoboot.
- On cold boot, hold `B` to bypass FirmMux and return to HOME Menu.

## Themes

- Set `theme: <name>` at the top of `sdmc:/3ds/FirmMux/config.yaml`.
- Theme files live at `sdmc:/3ds/FirmMux/themes/<name>/theme.yaml`.

Theme schema (colors are hex RGB or ARGB, e.g. `0xRRGGBB` or `0xAARRGGBB`):

```yaml
name: Default
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
```
