# SD Layout

This project ships an SD folder in the repo. Copy the `SD/` contents to the root of your SD card.

## Required

```
sd:/
  3ds/
    FirmMux.3dsx
    FirmMux.smdh
    firmmux-updater.3dsx
    firmmux-updater.smdh
    FirmMux/
      boot.3dsx   # autoboot template used by Settings toggle
      boot.smdh   # autoboot name/icon metadata
      backgrounds/
        top/
        bottom/
      # In Options, choose "Theme provided" to use the theme's topbg.png/botbg.png.
      bgm/
      themes/
      firmux-bootstrap-prep.3dsx
    emulators/
      retroarch_rules.json
      emulators.json
      launch.json
      rom_options.json
      filter_favorites.txt
      log.txt
  cias/
    FirmMux.cia   # optional: setup helper placeholder (not runtime)
  retroarch/
    cores/
    filters/
      video/
      audio/
    system/
  roms/
    a26/ a52/ a78/ col/ cpc/ gb/ gba/ gen/ gg/ intv/ m5/ nes/ ngp/ pkmni/ sg/ sms/ snes/ tg16/ ws/
    arcade/ cps1/ cps2/ cps3/ neogeo/ neogeocd/
    c64/ c128/ vic20/ plus4/ pet/
    psx/ vb/ lynx/ n64/ jaguar/ dos/ pc98/ scummvm/ quake/ uzebox/ tic80/ wasm/ lowresnx/
  _nds/
    nds-bootstrap-hb-release.nds
    nds-bootstrap-release.nds
    nds-bootstrap.ini
    ntr_forwarder.ini
    release-bootstrap.ver
    ntr-forwarder/
      sdcard.nds
      usrcheat.dat
      Widescreen.cxi   # optional
    firmmux/
      launch.txt
      nds_options/
      nds_cheats/
        <hash>.sel
      nds_widescreen/
        *.bin
```

## Notes

- `sd:/3ds/emulators/` is only used for RetroArch handoff/config/log files.
- Optional `sd:/cias/FirmMux.cia` is built via external tools and is setup-helper only:
  - bannertool by carstene1ns: https://github.com/carstene1ns/3ds-bannertool
  - makerom from Project_CTR by 3DSGuy: https://github.com/3DSGuy/Project_CTR
- Official runtime path is `sd:/3ds/FirmMux.3dsx` from default Homebrew Menu.
- `sd:/3ds/firmmux-updater.3dsx` updates FirmMux files only (no dependency staging).
- Standalone pathfile routing is optional.
- To enable optional standalone routing, install marker + apps:
  - `sd:/3ds/emulators/pathfile`
  - `sd:/pathfile/mgba.3dsx`
  - `sd:/pathfile/snes9x_3ds.3dsx`
  - `sd:/pathfile/DaedalusX64.3dsx`
- Runtime-generated pathfiles:
  - `sd:/pathfile/gba_launch.txt`
  - `sd:/pathfile/snes_launch.txt`
  - `sd:/pathfile/n64_launch.txt`
- `sd:/3ds/emulators/emulators.json` controls which emulator tabs are enabled; existing files are preserved and not overwritten.
- `sd:/retroarch/` must come from the RetroArch 3DSX release (not CIA). It provides cores and system files.
- `sd:/_nds/firmmux/nds_widescreen/` stores per-game widescreen `.bin` files.
- `sd:/_nds/ntr-forwarder/` comes from the NTR Forwarder package and is required for current NDS flow.
- `sd:/_nds/ntr-forwarder/usrcheat.dat` is the cheat database.
- For the 3DSX launcher, cheat flags are written directly into `sd:/_nds/ntr-forwarder/usrcheat.dat`.
- `sd:/_nds/nds-bootstrap/` may still appear on some setups at runtime (e.g. temporary/generated files), but it is not the required package root layout.
- Optional BGM: place `bgm.wav` at `sd:/3ds/FirmMux/bgm/bgm.wav` (loops).
