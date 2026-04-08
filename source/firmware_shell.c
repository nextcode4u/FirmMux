#include "firmware_shell.h"

#include <string.h>

#define FIRMWARE_SHELL_INFO_TYPE 0x10000u
#define FIRMWARE_SHELL_SLOT_BASE 0x188
#define FIRMWARE_SHELL_SLOT_COUNT 8
#define FIRMWARE_SHELL_CONTRACT_VERSION 1u

static Result firmware_shell_read_slot(s32 slot, s64* out) {
    if (!out) return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_INVALID_ADDRESS);
    *out = 0;
    return svcGetSystemInfo(out, FIRMWARE_SHELL_INFO_TYPE, slot);
}

void firmware_shell_init(FirmwareShellState* state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->support = FW_SUPPORT_NONE;
    state->shell_ready = FW_SHELL_UNKNOWN;
    state->jump_ready = FW_JUMP_UNKNOWN;
}

void firmware_shell_refresh(FirmwareShellState* state) {
    if (!state) return;

    FirmwareSupport prev_support = state->support;
    FirmwareShellReadiness prev_shell_ready = state->shell_ready;
    FirmwareJumpReadiness prev_jump_ready = state->jump_ready;
    bool prev_trustworthy = state->trustworthy;

    s64 first = 0;
    Result rc = firmware_shell_read_slot(FIRMWARE_SHELL_SLOT_BASE, &first);
    state->probed = true;
    state->last_rc = rc;
    if (R_FAILED(rc)) {
        state->support = FW_SUPPORT_NONE;
        state->contract_version = 0;
        state->shell_ready = FW_SHELL_UNKNOWN;
        state->jump_ready = FW_JUMP_UNKNOWN;
        state->trustworthy = false;
        state->route_kind = 0;
        state->flags = 0;
        memset(state->raw_slots, 0, sizeof(state->raw_slots));
        if (prev_support != state->support || prev_shell_ready != state->shell_ready || prev_jump_ready != state->jump_ready || prev_trustworthy != state->trustworthy) {
            debug_log("fw_shell: unavailable rc=%08lX", (unsigned long)rc);
        }
        return;
    }

    memset(state->raw_slots, 0, sizeof(state->raw_slots));
    state->raw_slots[0] = (u64)first;
    for (int i = 1; i < FIRMWARE_SHELL_SLOT_COUNT; i++) {
        s64 value = 0;
        Result slot_rc = firmware_shell_read_slot(FIRMWARE_SHELL_SLOT_BASE + i, &value);
        if (R_FAILED(slot_rc)) break;
        state->raw_slots[i] = (u64)value;
    }

    state->contract_version = (u32)state->raw_slots[4];
    state->route_kind = state->raw_slots[1];
    state->flags = state->raw_slots[3];
    if (state->contract_version == 0) {
        state->support = FW_SUPPORT_NONE;
    } else if (state->contract_version == FIRMWARE_SHELL_CONTRACT_VERSION) {
        state->support = FW_SUPPORT_PRESENT;
    } else {
        state->support = FW_SUPPORT_UNKNOWN;
    }

    switch ((u32)state->raw_slots[0]) {
        case 1:
            state->shell_ready = FW_SHELL_NOT_READY;
            break;
        case 2:
            state->shell_ready = FW_SHELL_READY;
            break;
        default:
            state->shell_ready = FW_SHELL_UNKNOWN;
            break;
    }

    switch ((u32)state->raw_slots[2]) {
        case 1:
            state->jump_ready = FW_JUMP_NOT_READY;
            break;
        case 2:
            state->jump_ready = FW_JUMP_READY;
            break;
        default:
            state->jump_ready = FW_JUMP_UNKNOWN;
            break;
    }

    state->trustworthy = (state->support == FW_SUPPORT_PRESENT &&
        (state->shell_ready != FW_SHELL_UNKNOWN || state->jump_ready != FW_JUMP_UNKNOWN));

    if (prev_support != state->support || prev_shell_ready != state->shell_ready || prev_jump_ready != state->jump_ready || prev_trustworthy != state->trustworthy) {
        debug_log("fw_shell: support=%d ver=%u shell=%d jump=%d trustworthy=%d route=%016llX flags=%016llX",
            (int)state->support,
            state->contract_version,
            (int)state->shell_ready,
            (int)state->jump_ready,
            state->trustworthy ? 1 : 0,
            (unsigned long long)state->route_kind,
            (unsigned long long)state->flags);
    }
}
