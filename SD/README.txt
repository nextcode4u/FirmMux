FirmMux SD setup

1) Pure RetroArch backend (3DSX only):
   Expected RetroArch path:
   /3ds/FirmMux/emulators/retroarch.3dsx
   RetroArch cores/system: /retroarch/ (cores in /retroarch/cores/, BIOS in /retroarch/system/)

2) RetroArch config files (auto-created if missing/invalid):
   /3ds/emulators/retroarch_rules.json
   /3ds/emulators/emulators.json
   /3ds/emulators/launch.json
   /3ds/emulators/log.txt (only when enabled)

3) Supported RetroArch system folders under /roms:
   a26 a52 a78 col cpc gb gen gg intv m5 nes ngp pkmni sg sms snes tg16 ws

4) In FirmMux Options:
   - Emulators... (per-system enable/disable and folder assignment)
   - RetroArch log: On/Off
   - RetroArch backend requirements

5) Backgrounds (optional):
   /3ds/FirmMux/backgrounds/top/
   /3ds/FirmMux/backgrounds/bottom/
   Recommended sizes: top 400x240, bottom 320x240
   PNGs are stretched to fit and alpha is ignored.
   Default background visibility is 50%.

6) NDS pipeline (separate from RetroArch):
   Put NDS ROMs in: /roms/nds/

7) NDS launchers (optional):
   - Select NDS launcher (CTR-P-FMBP)
   - Select NTR launcher (NTR Launcher)
   FirmMux Bootstrap Launcher releases:
   https://github.com/nextcode4u/firmux-bootstrap-prep/releases

Notes
- RetroArch emulator tabs are appended after Homebrew.
- Empty or missing system folders are safe; FirmMux will not crash.
- Theme files live at /3ds/FirmMux/themes/<name>/theme.yaml.
- Sample themes: default, amber, cobalt, dark_material, ember, glacier, graphite, midnight, mint, neon_cyber, paper_light, sage, sandstone, sunset, synthwave_hass.

Custom RetroArch build
1) Place RetroArch source in:
   /retroarch_src/RetroArch-master/
   Source: https://github.com/libretro/RetroArch/tree/master

2) RetroArch 3DSX data bundle required (NOT the CIA):
   https://buildbot.libretro.com/stable/
   Copy the "retroarch" folder from the 3DS 3DSX build to:
   /retroarch/  (must include cores/ and system/)
2) Run:
   tools/build_retroarch_with_firmux.sh
3) Copy output:
   /SD/3ds/FirmMux/emulators/retroarch.3dsx -> SD:/3ds/FirmMux/emulators/retroarch.3dsx
