FirmMux SD setup (alpha)

1) First boot creates:
   /3ds/FirmMux/config.yaml
   /3ds/FirmMux/state.json

2) Put NDS ROMs in:
   /roms/nds/

3) Install NTR Launcher (for Slot-1 cards):
   https://github.com/ApacheThunder/NTR_Launcher/releases
   Install "NTR Launcher.cia"

4) Install nds-bootstrap pack (for .nds files):
   https://github.com/RocketRobz/NTR_Forwarder/releases

5) In FirmMux Options:
   - Select NDS launcher (CTR-P-FMBP)
   - Select NTR launcher (NTR Launcher)
   - Themes... (optional)

FirmMux Bootstrap Launcher releases:
https://github.com/nextcode4u/firmux-bootstrap-prep/releases

Notes
- FirmMux writes: sd:/_nds/firmux/launch.txt for .nds launches.
- NDS card row only appears at /roms/nds/ root.
- Theme files live at /3ds/FirmMux/themes/<name>/theme.yaml.
- Sample themes: default, amber, cobalt, ember, epoxy_resin, glacier, graphite, sage, synthwave.
- RetroArch rules + emulator settings live at /3ds/Emulators/.
- RetroArch BIOS go in /retroarch/system/.
 - Use the RetroArch homebrew 3DSX package (not the CIA build).
Common BIOS names:
- Atari800: 5200.rom, ATARIXL.ROM, ATARIBAS.ROM, ATARIOSB.ROM
- blueMSX: Databases/ and Machines/ folders
- Beetle PCE Fast: syscard3.pce (optional syscard2.pce/syscard1.pce)
- FreeIntv: exec.bin, grom.bin (optional ecs.bin)
