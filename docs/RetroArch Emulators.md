# RetroArch Emulators (FirmMux)

FirmMux is the frontend. RetroArch 3DSX is the backend.

## Requirements

- RetroArch 3DSX release (not CIA): https://buildbot.libretro.com/stable/
- Copy the `retroarch/` folder from the 3DSX release to:
  - `sd:/retroarch/`
- FirmMux custom RetroArch binary:
  - `sd:/3ds/FirmMux/emulators/retroarch.3dsx`
- Optional stock RetroArch app files for manual launch/testing:
  - `sd:/3ds/retroarch.3dsx`
  - `sd:/3ds/retroarch.smdh`

## Supported Systems

FirmMux maps these folders under `sd:/roms/`:

- Atari 2600 (`a26`)
- Atari 5200 (`a52`)
- Atari 7800 (`a78`)
- ColecoVision (`col`)
- Amstrad CPC (`cpc`)
- Game Boy/Color (`gb`)
- Game Boy Advance (`gba`)
- Genesis/Mega Drive (`gen`)
- Game Gear (`gg`)
- Intellivision (`intv`)
- Sord M5 (`m5`)
- NES (`nes`)
- Neo Geo Pocket/Color (`ngp`)
- PokeMini (`pkmni`)
- SG‑1000 (`sg`)
- Master System (`sms`)
- SNES (`snes`)
- TurboGrafx‑16/PC Engine (`tg16`)
- WonderSwan/Color (`ws`)
- Arcade (`arcade`)
- Capcom Play System 1 (`cps1`)
- Capcom Play System 2 (`cps2`)
- Capcom Play System 3 (`cps3`, New 3DS only tab)
- Neo Geo (`neogeo`)
- Neo Geo CD (`neogeocd`)
- Commodore 64 (`c64`)
- Commodore 128 (`c128`)
- Commodore VIC-20 (`vic20`)
- Commodore Plus/4 (`plus4`)
- Commodore PET (`pet`)
- PlayStation (`psx`, New 3DS only tab)
- Virtual Boy (`vb`)
- Atari Lynx (`lynx`)
- Nintendo 64 (`n64`, New 3DS only tab)
- Atari Jaguar (`jaguar`)
- DOS (`dos`, New 3DS only tab)
- NEC PC-98 (`pc98`, New 3DS only tab)
- ScummVM (`scummvm`)
- Quake (`quake`)
- Uzebox (`uzebox`)
- TIC-80 (`tic80`)
- WASM-4 (`wasm`)
- LowRes NX (`lowresnx`)

## Default Enabled Systems

By default, FirmMux enables these tabs:

- NES
- SNES
- Game Boy / Game Boy Color
- Game Boy Advance
- Genesis / Mega Drive
- Master System
- Game Gear
- TurboGrafx-16 / PC Engine
- Capcom Play System 1
- Capcom Play System 2
- Neo Geo
- Atari Lynx
- Virtual Boy
- Neo Geo Pocket
- WonderSwan

All other supported systems are disabled by default and can be enabled in Settings -> Emulators.

Note: if `sd:/3ds/emulators/emulators.json` already exists, FirmMux keeps the user's current enabled/disabled choices.

## Backend Files (auto‑created)

FirmMux uses:
- `sd:/3ds/emulators/retroarch_rules.json`
- `sd:/3ds/emulators/emulators.json`
- `sd:/3ds/emulators/launch.json`
- `sd:/3ds/emulators/log.txt`

These are regenerated if missing/invalid.

## Per‑ROM Options

Per‑ROM RetroArch options are stored in:

- `sd:/3ds/emulators/rom_options.json`

Set these from FirmMux (press **Y** on a ROM in an emulator tab).

Supported options include:
core override, CPU/GPU profile, frameskip, VSync, audio latency, threaded video,
hard GPU sync, integer scale, aspect ratio, bilinear filter, video filter, audio filter,
run‑ahead, rewind.

Video filter favorites (optional) can be listed one per line in:

- `sd:/3ds/emulators/filter_favorites.txt`

If no favorites file is present or it is empty, FirmMux will list files from:
- `sd:/retroarch/filters/video/`

Audio filters are listed from:
- `sd:/retroarch/filters/audio/`

## Standalone Pathfile Routing (Optional)

If `sd:/3ds/emulators/pathfile` exists and standalone apps are installed in `sd:/pathfile/`, FirmMux can route specific systems to standalone `.3dsx` apps using pathfiles:

- `sd:/pathfile/gba_launch.txt` -> `sd:/pathfile/mgba.3dsx` (New 3DS only)
- `sd:/pathfile/snes_launch.txt` -> `sd:/pathfile/snes9x_3ds.3dsx` (Old/New 3DS)
- `sd:/pathfile/n64_launch.txt` -> `sd:/pathfile/DaedalusX64.3dsx` (New 3DS only)

Pathfile contents are a single absolute ROM path, for example:

`sdmc:/roms/gba/Metroid Fusion.gba`

Fallback behavior if standalone package/app is missing:

- GBA/SNES/N64 use normal RetroArch backend resolution.

## Custom RetroArch Build (FirmMux)

Source:
- https://github.com/libretro/RetroArch/tree/master

Place source in:
- `retroarch_src/RetroArch-master/`

Build:
```
tools/build_retroarch_with_firmux.sh
```

Output:
- `SD/3ds/FirmMux/emulators/retroarch.3dsx`

## Stability Note

State persistence now uses atomic writes (`state.json.tmp` -> `state.json`) and deferred idle saves.
FirmMux does not write state during launch handoff paths.
