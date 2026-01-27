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
   - Top background / Bottom background (optional)
   - Background visibility (optional)

FirmMux Bootstrap Launcher releases:
https://github.com/nextcode4u/firmux-bootstrap-prep/releases

Notes
- FirmMux writes: sd:/_nds/firmux/launch.txt for .nds launches.
- NDS card row only appears at /roms/nds/ root.
- Theme files live at /3ds/FirmMux/themes/<name>/theme.yaml.
- Background PNGs go in:
  - /3ds/FirmMux/backgrounds/top/
  - /3ds/FirmMux/backgrounds/bottom/
  Recommended sizes: top 400x240, bottom 320x240. Images are stretched to fit. Use standard RGB PNGs; alpha is ignored.
- Background visibility can be adjusted in Options → Background visibility.
  Default background visibility is 80%.
- Sample themes: default, amber, cobalt, dark_material, ember, glacier, graphite, midnight, mint, neon_cyber, paper_light, sage, sandstone, sunset, synthwave_hass.
