# SD Layout

This project ships an SD folder in the repo. Copy the `SD/` contents to the root of your SD card.

## Required

```
sd:/
  3ds/
    FirmMux.3dsx
    FirmMux.smdh
    FirmMux/
      backgrounds/
        top/
        bottom/
      themes/
    emulators/
      retroarch_rules.json
      emulators.json
      launch.json
      log.txt
  retroarch/
    cores/
    system/
  roms/
    a26/ a52/ a78/ col/ cpc/ gb/ gen/ gg/ intv/ m5/ nes/ ngp/ pkmni/ sg/ sms/ snes/ tg16/ ws/
  _nds/
    nds-bootstrap.nds
    nds-bootstrap.ini
    nds-bootstrap/
      cheatData.bin
      wideCheatData.bin
    firmmux/
      launch.txt
      nds_options/
      nds_cheats/
        usrcheat.dat
      nds_widescreen/
        *.bin
```

## Notes

- `sd:/3ds/emulators/` is only used for RetroArch handoff/config/log files.
- `sd:/retroarch/` must come from the RetroArch 3DSX release (not CIA). It provides cores and system files.
- `sd:/_nds/firmmux/nds_widescreen/` stores per-game widescreen `.bin` files.
- `sd:/_nds/firmmux/nds_cheats/usrcheat.dat` is the cheat database.
