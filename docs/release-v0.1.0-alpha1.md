# FirmMux v0.1.0-alpha1

This is the first public alpha build of FirmMux. It is focused on CTR/TWL browsing and handoff, not emulation.

## Highlights

- Config-driven targets (System Menu, 3DS Titles, Homebrew, NDS Titles).
- NDS browser with Sprite/Title Data toggle and cached banner previews.
- DSiWare (00048004) titles supported in the 3DS list with banner preview.
- Homebrew launching via hb:ldr.
- NDS launching via FirmMuxBootstrapLauncher (launch.txt handoff).
- Top status bar (time, Wi-Fi, battery) and System Info panel on Return to HOME.

## SD layout

- FirmMux app: sd:/3ds/FirmMux.3dsx and sd:/3ds/FirmMux.smdh
- Config/state: sd:/3ds/FirmMux/config.yaml and sd:/3ds/FirmMux/state.json
- NDS launcher handoff: sd:/_nds/firmux/launch.txt

## Limitations

- No emulator targets yet (NES/GBA/SNES/etc are not implemented).
- 3DS title list is text-only; preview shows the icon on selection.
- NDS list is filename-based; banner/title data is optional and may load after selection.

## Known issues

- Game Card titles are not listed yet.

## NDS launching

- Install FirmMuxBootstrapLauncher CIA (TitleID 000400000FF40500).
- Ensure sd:/_nds/nds-bootstrap.nds exists.
- FirmMux writes sd:/_nds/firmux/launch.txt and then launches the CIA.
