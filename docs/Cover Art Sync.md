# Cover Art Sync

Use `tools/firmmux_boxart_sync.py` on your PC to download and resize ROM cover art for FirmMux.

Output path:

- `sd:/3ds/FirmMux/cache/covers/<system>/<rom_name>.png`

Output size:

- `92x92`

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

## Data Source and Matching

- Provider fallback:
  1. `https://thumbnails.libretro.com`
  2. `https://raw.githubusercontent.com/libretro-thumbnails/...`
- Art priority:
  1. `Named_Titles`
  2. `Named_Boxarts`
  3. `Named_Snaps`
- Matching uses filename variants and optional SHA1 name matching via Libretro No-Intro DAT files.

## Cache/Logs

- Index: `sd:/3ds/FirmMux/cache/covers/index.json`
- Negative cache: `sd:/3ds/FirmMux/cache/covers/negative_cache.json`
- Log: `sd:/3ds/FirmMux/logs/cover_sync.log`
