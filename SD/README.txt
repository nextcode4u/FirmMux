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
- NDS cheats: /_nds/firmmux/nds_cheats/usrcheat.dat
- NDS widescreen bins: /_nds/firmmux/nds_widescreen/

Widescreen system requirement:
- Install TWPatch and enable Widescreen, then patch via Select.
  https://gbatemp.net/download/twpatch.37400/history
- Enable Luma: External FIRM and modules.
- Move TwlBg.cxi from sd:/luma/sysmodules/ to sd:/_nds/ntr-forwarder/Widescreen.cxi
