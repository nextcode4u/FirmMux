# FirmMux PC Setup

FirmMux keeps health checks on-device, but setup/update staging is PC-side.

Run:

```bash
python3 tools/firmmux_setup_pc.py
```

Windows quick start (Windows-only):

- Double-click `tools/FirmMux_Setup.bat`
- The `.bat` downloads the latest `firmmux_setup_pc.py` from GitHub and runs it.

The script prompts for:

1. SD card root selection
2. Action mode:
- Install FirmMux + dependencies
- Update FirmMux only
- Download dependencies only
3. It reads `sd:/3ds/FirmMux/logs/health_check.txt` (if present) and suggests a default mode.

Optional non-interactive args:

```bash
python3 tools/firmmux_setup_pc.py --sd-root <SD_ROOT> --mode full
```

What it stages:

- `sd:/cias/bootstrap.cia`
- `sd:/cias/NTR_Launcher.cia`
- NTR Forwarder SD files
- RetroArch 3DSX package files
- Latest FirmMux release SD package (`FirmMux-SD.zip`) when using install/update modes

After script completes:

1. Put SD back in 3DS.
2. Install `bootstrap.cia` and `NTR_Launcher.cia` with FBI.
3. If `sd:/3ds/dspfirm.cdc` is missing, open Rosalina and dump DSP firmware:
- `L + D-Pad Down + Select` -> `Miscellaneous options` -> `Dump DSP firmware`
4. Launch FirmMux and run Health check.
