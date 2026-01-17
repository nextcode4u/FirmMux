# FirmMux

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

## NDS launching via FirmMuxBootstrapLauncher

- Install the FirmMuxBootstrapLauncher CIA (TitleID `00040000FF401000`) on SD.
- Ensure `sd:/_nds/nds-bootstrap.nds` is present.
- Place ROMs under `sd:/roms/nds/`.
- Selecting an `.nds` in FirmMux writes `sd:/_nds/firmux/launch.txt` with a single line `sd:/roms/nds/<file>.nds` (newline terminated) then jumps to the launcher CIA.
- Default TitleID is defined in `include/fmux.h` as `FMUX_BOOTSTRAP_TITLEID`; you can override per NDS target via `loader_title_id` and optional `loader_media` in `config.yaml`.

## Autoboot and bypass

- Set FirmMux as the autoboot target in Luma Hbmenu autoboot.
- On cold boot, hold `B` to bypass FirmMux and return to HOME Menu.
