# RetroArch Emulators (FirmMux)

FirmMux is the frontend. RetroArch 3DSX is the backend.

## Requirements

- RetroArch 3DSX release (not CIA): https://buildbot.libretro.com/stable/
- Copy the `retroarch/` folder from the 3DSX release to:
  - `sd:/retroarch/`
- RetroArch binary used by FirmMux:
  - `sd:/3ds/FirmMux/emulators/retroarch.3dsx`

## Supported Systems

FirmMux maps these folders under `sd:/roms/`:

- Atari 2600 (`a26`)
- Atari 5200 (`a52`)
- Atari 7800 (`a78`)
- ColecoVision (`col`)
- Amstrad CPC (`cpc`)
- Game Boy/Color (`gb`)
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
