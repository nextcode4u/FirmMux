# FirmMux v0.1.0-alpha2

This update focuses on theming, launcher selection updates, and UI polish for the alpha.

## Highlights

- Theme system with a theme picker (Options → Themes...) and per-theme layout/spacing.
- New color themes: default, amber, cobalt, ember, epoxy_resin, glacier, graphite, sage.
- Epoxy Resin theme includes asset-based backgrounds and sprite icon.
- NDS launcher auto-select uses product code `CTR-P-FMBP` (Select NDS launcher).
- System info panel and status bar refinements.

## SD layout

- FirmMux app: sd:/3ds/FirmMux.3dsx and sd:/3ds/FirmMux.smdh
- Themes: sd:/3ds/FirmMux/themes/<name>/theme.yaml
- NDS launcher handoff: sd:/_nds/firmux/launch.txt

## NDS launching

- Install FirmMuxBootstrapLauncher (`CTR-P-FMBP`).
- FirmMux writes sd:/_nds/firmux/launch.txt and then launches the CIA.
- FirmMux Bootstrap Launcher releases: https://github.com/nextcode4u/firmux-bootstrap-prep/releases
- YANBF releases: https://github.com/YANBForwarder/YANBF/releases (FirmMux Bootstrap depends on `bootstrap.cia`).
- NTR Launcher is used for Slot-1 cards: https://github.com/ApacheThunder/NTR_Launcher/releases
- NTR Forwarder pack (includes nds-bootstrap): https://github.com/RocketRobz/NTR_Forwarder/releases

## Known issues

- Game Card titles are not listed yet.
