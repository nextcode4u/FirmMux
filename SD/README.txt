FirmMux SD setup

See docs in the repo for full details:
- docs/SD Layout.md
- docs/RetroArch Emulators.md
- docs/NDS Options.md

Quick paths:
- FirmMux: /3ds/FirmMux.3dsx
- RetroArch (FirmMux build): /3ds/FirmMux/emulators/retroarch.3dsx
- RetroArch data/cores: /retroarch/
- External RetroArch configs: /3ds/emulators/
- NDS options: /_nds/firmmux/nds_options/
- NDS cheats: /_nds/ntr-forwarder/usrcheat.dat
- NDS widescreen bins: /_nds/firmmux/nds_widescreen/
- NDS launcher 3DSX (default): /3ds/FirmMux/firmux-bootstrap-prep.3dsx
- NDS launcher CIA: FirmMuxBootstrapLauncher (TitleID CTR-P-FMBP)
- Settings: “Check NDS launcher” (CIA) and “NDS launcher mode” (Auto/CIA/3DSX)

Widescreen system requirement:
- Install TWPatch and enable Widescreen, then patch via Select.
  https://gbatemp.net/download/twpatch.37400/history
- Enable Luma: External FIRM and modules.
- Move TwlBg.cxi from sd:/luma/sysmodules/ to sd:/_nds/ntr-forwarder/Widescreen.cxi
