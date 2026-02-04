# NDS Options (per‑ROM)

Per‑ROM settings are stored under:

- `sd:/_nds/firmmux/nds_options/`

Launcher:
- 3DSX launcher (default): `sd:/3ds/FirmMux/firmux-bootstrap-prep.3dsx`
- CIA launcher (optional): FirmMuxBootstrapLauncher (TitleID `CTR-P-FMBP`)
In Settings:
- “Check NDS launcher” checks for the CIA.
- “NDS launcher mode” chooses Auto / CIA / 3DSX.

## Widescreen

Widescreen uses per‑game `.bin` files from:

- `sd:/_nds/firmmux/nds_widescreen/`

Copy files from:

- https://github.com/DS-Homebrew/TWiLightMenu/tree/master/resources/widescreen

FirmMux will look for:
- `<ROM basename>.bin`
- `<GAMECODE>-<CRC16>.bin`

If missing, it shows a toast and keeps widescreen OFF.

### System requirement (must be installed once)

Widescreen patches require TWPatch + Luma external modules enabled.

1) Download and install TWPatch:
   https://gbatemp.net/download/twpatch.37400/history
2) Run TWPatch, press **Y + B** to open the patch menu.
3) Enable **Widescreen**, then press **Select** to patch.
4) In Luma config, enable **External FIRM and modules**.

Without this, widescreen will not take effect even if the `.bin` exists.

### Move TwlBg.cxi for forwarders

TWPatch generates:

- `sd:/luma/sysmodules/TwlBg.cxi`

For NTR forwarder / 3DSX launcher usage, move and rename it to:

- `sd:/_nds/ntr-forwarder/Widescreen.cxi`

## Cheats

Cheats use `usrcheat.dat` placed at:

- `sd:/_nds/ntr-forwarder/usrcheat.dat`

Cheat selection is per‑ROM and saved under:

- `sd:/_nds/firmmux/nds_cheats/<hash>.sel`

FirmMux updates the cheat flags directly inside:
- `sd:/_nds/ntr-forwarder/usrcheat.dat`

Cheats are grouped by categories (folder names from `usrcheat.dat`).
