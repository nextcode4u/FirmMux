# Cover Art Sync

Use `tools/firmmux_boxart_sync.py` on your PC to download and resize ROM cover art for FirmMux.

Output path:

- `sd:/3ds/FirmMux/cache/covers/<system>/<rom_name>.png`

Output size:

- `92x92`
- normalized `RGBA` PNG
- metadata stripped
- verified after write for FirmMux compatibility

## Requirements

- Python 3.9+
- Pillow

Install:

```bash
pip install pillow
```

## Easiest Use (Recommended)

- Double-click `tools/firmmux_boxart_sync.py`
- Pick your SD drive
- Choose scan type: Recommended/Fast/Deep
- Start scan

The script will show a summary at the end (scanned, found, unresolved, errors).

It also now supports clearing the negative cache before a scan so previously missed ROMs can be retried immediately.

It also supports auditing existing cached covers to catch incompatible PNGs before copying them to the SD card.

## Supported RetroArch Systems

The sync script now covers all FirmMux RetroArch system keys:

- `a26`, `a52`, `a78`
- `arcade`, `cps1`, `cps2`, `cps3`
- `col`, `cpc`
- `gb`, `gba`
- `gen`, `gg`
- `intv`
- `c64`, `c128`, `vic20`, `plus4`, `pet`
- `nes`
- `ngp`, `pkmni` (`pknmini` alias supported)
- `sg`, `sms`, `snes`, `tg16`, `ws`
- `neogeo`, `neogeocd`
- `psx`, `vb`, `lynx`, `n64`, `jaguar`
- `dos`, `pc98`, `scummvm`, `quake`
- `uzebox`, `tic80`, `wasm`, `lowresnx`

Note: availability of artwork still depends on what exists in Libretro thumbnail sources for the exact ROM naming.

## CLI Use

Linux:

```bash
python3 tools/firmmux_boxart_sync.py --sd-root /media/<user>/<SDCARD>
```

Windows:

```powershell
py tools/firmmux_boxart_sync.py --sd-root E:\
```

## Recommended Settings

- Match mode: `balanced`
- Hash mode: `missing`
- Clear negative cache: `yes` when testing new matching changes

## Data Source and Matching

- Provider fallback:
  1. `https://thumbnails.libretro.com`
  2. `https://raw.githubusercontent.com/libretro-thumbnails/...`
- Art source:
  - `Named_Boxarts` only
- Matching uses stronger filename normalization plus optional SHA1 name matching via Libretro No-Intro DAT files.

## Cache/Logs

- Index: `sd:/3ds/FirmMux/cache/covers/index.json`
- Negative cache: `sd:/3ds/FirmMux/cache/covers/negative_cache.json`
- Log: `sd:/3ds/FirmMux/logs/cover_sync.log`

CLI example to retry old misses immediately:

```bash
python3 tools/firmmux_boxart_sync.py --sd-root /media/<user>/<SDCARD> --clear-negative-cache
```

CLI example to audit cached covers only:

```bash
python3 tools/firmmux_boxart_sync.py --sd-root /media/<user>/<SDCARD> --audit-cache
```
