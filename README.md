# FirmMux 3DS

FirmMux is a unified front‑end for CTR, TWL, System Menu, Homebrew, and RetroArch‑backed systems: Atari 2600, Atari 5200, Atari 7800, ColecoVision, Amstrad CPC, Game Boy/Color, Genesis/Mega Drive, Game Gear, Intellivision, Sord M5, NES, Neo Geo Pocket/Color, PokeMini, SG‑1000, Master System, SNES, TurboGrafx‑16/PC Engine, WonderSwan/Color.

## Build (Linux / WSL / MSYS2)

```
sudo dkp-pacman -Syu
sudo dkp-pacman -S devkitARM libctru citro2d citro3d 3ds-dev
make
```

Output: `FirmMux.3dsx`

## Run

Copy `FirmMux.3dsx` to `sd:/3ds/` and launch via hbmenu.

## Documentation

See the `docs/` folder for setup and backend details:

- `docs/SD Layout.md`
- `docs/RetroArch Emulators.md`
- `docs/NDS Options.md`

