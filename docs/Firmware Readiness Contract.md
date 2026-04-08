# Firmware Readiness Contract

FirmMux consumes a narrow read-only firmware readiness contract when present.

This integration is app-side only.
- No route mediation
- No userland shell ownership
- No Nexus3DS source changes from this repo

## Slots

FirmMux reads custom `svcGetSystemInfo` slots from `type=0x10000`.

- `0x188` = `fw_shell_ready`
- `0x189` = `fw_route_kind`
- `0x18A` = `fw_jump_readiness`
- `0x18B` = `fw_flags`
- `0x18C` = contract version/support

## Contract values

`0x18C`
- `0` = unsupported
- `1` = supported contract version

`fw_shell_ready`
- `0` = unknown
- `1` = not ready
- `2` = ready

`fw_jump_readiness`
- `0` = unknown
- `1` = not ready
- `2` = ready

`fw_route_kind`
- informational only

`fw_flags`
- diagnostic only

## FirmMux behavior

Raw probing stays isolated in:
- `source/firmware_shell.c`

Launch gating stays centralized in:
- `source/launch_policy.c`

Current policy:
- contract version `1` and `fw_jump_readiness == 2` -> allow launch
- contract version `1` and (`fw_shell_ready == 1` or `fw_jump_readiness == 1`) -> defer launch
- unsupported or unknown values -> fall back to the HOME-init heuristic
- installed CTR title and DSiWare launch from FirmMux use the homebrew-compatible chainloader path instead of the applet application-jump path
- direct installed-title launch sets the Nexus3DS direct-chainload target latch with `svcKernelSetState(0x10083, mediaType, lowTitleId, highTitleId)` immediately before setting intent
- direct installed-title launch then sets the Nexus3DS direct-chainload intent latch with `svcKernelSetState(0x10082, 1, 0, 0)` immediately before `aptSetChainloader(...)`
- direct installed-title chainload exits through a minimal immediate app-exit path after `aptSetChainloader(...)`
- that immediate chainload exit still shuts down audio/NDSP first so libctru worker threads do not fault during teardown
- for the direct installed-title chainload path, FirmMux now terminates with `svcExitProcess()` instead of returning from `main`, so it does not re-enter libctru `__ctru_exit` and invoke the homebrew loader return callback
- once the direct installed-title chainload is queued, FirmMux now skips normal post-launch stats/audio/UI success handling and transitions straight into exit mode in the same frame
- the old HOME fallback/relaunch path is not used for direct installed-title launch

## Fallback

Without firmware support, FirmMux remains stock-compatible.

The old HOME-init heuristic is fallback only.

## Limits

- `fw_route_kind` must not be treated as routing authority
- `fw_flags` must not be treated as launch authority
- future mediation belongs on the firmware side, not in FirmMux userland
