# Known Issues

## System applets

- `Miiverse` can be launched from a minimal `aptLaunchSystemApplet(...)` test app, but `Friends List`, `Game Notes`, and `Notifications` are not currently safe to expose in FirmMux.
- Raw `APT_PrepareToStartSystemApplet` / `APT_StartSystemApplet` was not sufficient.
- With the newer libctru wrapper path, `Friends List`, `Game Notes`, and `Notifications` still crash from generic homebrew context.
- Some system applets are known to require applet-specific parameter buffers. A known example is the browser applet, which is launched with a URL payload rather than a NULL/zeroed buffer.
- These applets are therefore not listed in FirmMux at this time.

## HOME Menu init for 3DS title launching

- On direct autoboot flows, HOME Menu may not be initialized yet when FirmMux starts.
- In that state, launching installed 3DS titles can fail until HOME Menu has been initialized once.
- FirmMux already prompts for this and uses a HOME-init flow, but the dependency still exists.

## NDS cheats

- NDS cheats are still work in progress.
- Cheat selection UI and `usrcheat.dat` flag writes are implemented.
- In-game cheat activation is not yet reliable across all titles.

## System Menu app coverage

- FirmMux supports enumerated system titles through the normal title path.
- Some OS-level applets are not exposed unless they are proven safe to launch from homebrew context.

## Theme compatibility constraints

- Theme-controlled fonts are disabled for stability.
- Theme-controlled line spacing is disabled for stability.
- Theme-controlled list item height is disabled for stability.
- Theme-controlled status bar height is disabled for stability.
- Background visibility controls were removed; backgrounds now render at full opacity.
- Older theme keys for these settings are ignored safely.

## RetroArch launcher dependency

- RetroArch-backed systems depend on the FirmMux custom RetroArch handoff flow.
- If the custom RetroArch binary or required data is missing, ROM launch will fail until dependencies are staged correctly.

## Standalone/pathfile support

- Pathfile support is implemented for supported standalone emulators, but only where the expected standalone package and pathfile layout are present on SD.
- Unsupported or incomplete standalone installs fall back to the normal FirmMux launch path where possible.

## open_agb_firm integration

- FirmMux does not currently support direct per-ROM launching into `open_agb_firm`.
- `open_agb_firm` does not provide a simple file-path handoff model like the standalone pathfile emulators.
- Luma also does not provide a general boot-once mechanism that FirmMux can rely on for a one-shot reboot into `open_agb_firm` and then automatically return to normal boot behavior.
- Because of those limitations, `open_agb_firm` is not currently integrated as a normal launch target in FirmMux.
