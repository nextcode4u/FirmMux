# FirmMux v1.0.0-beta1

## Highlights
- Pure RetroArch 3DSX backend integration with firm handoff (`sd:/3ds/emulators/launch.json`) and core resolution.
- Custom FirmMux RetroArch build that boots directly into the selected core + ROM.
- Emulator system tabs with enable/disable, folder mapping, and robust missing/empty folder handling.
- Background picker (top/bottom) + background visibility control.
- Updated SD layout and documentation for RetroArch dependencies and build steps.

## RetroArch Requirements
- Custom RetroArch 3DSX: `sd:/3ds/FirmMux/emulators/retroarch.3dsx`
- RetroArch data bundle (3DSX build) from:
  https://buildbot.libretro.com/stable/
- Dependency folders on SD:
  - `sd:/retroarch/cores/`
  - `sd:/retroarch/system/`

## SD Layout (Core Paths)
- FirmMux: `sd:/3ds/FirmMux.3dsx`
- FirmMux assets: `sd:/3ds/FirmMux/`
- RetroArch backend configs: `sd:/3ds/emulators/`

## Notes
- This is a beta release; expect iterative updates as new systems and refinements land.
