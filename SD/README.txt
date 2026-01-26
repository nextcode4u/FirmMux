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
   - Select NDS launcher (FMUXBOOT)
   - Select NTR launcher (NTR Launcher)

Notes
- FirmMux writes: sd:/_nds/firmux/launch.txt for .nds launches.
- NDS card row only appears at /roms/nds/ root.
