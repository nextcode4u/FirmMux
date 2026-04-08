#include "shell_state.h"

void shell_state_init(ShellState* state) {
    if (!state) return;
    state->mode = SHELL_MODE_FRONTEND;
    state->last_gate_source = SHELL_GATE_NONE;
    state->wait_reason = SHELL_WAIT_NONE;
    state->home_init_attempted = false;
    state->home_init_pending = false;
    state->home_init_ready = false;
    state->resume_notice_pending = false;
    state->home_init_delay = 0;
    firmware_shell_init(&state->firmware);
    session_restore_init(&state->session);
}

void shell_state_cancel_home_init(ShellState* state) {
    if (!state) return;
    if (state->home_init_pending || state->session.has_pending_launch) {
        debug_log("shell: pending launch cancelled reason=%d", (int)state->wait_reason);
    }
    state->home_init_pending = false;
    state->wait_reason = SHELL_WAIT_NONE;
    state->home_init_delay = 0;
    session_restore_clear_pending(&state->session);
}

void shell_state_begin_home_init(ShellState* state, const LaunchContext* ctx) {
    if (!state) return;
    state->last_gate_source = SHELL_GATE_HEURISTIC;
    state->wait_reason = SHELL_WAIT_HEURISTIC_HOME_INIT;
    state->home_init_attempted = true;
    state->home_init_pending = true;
    state->home_init_delay = 45;
    state->home_init_ready = false;
    state->resume_notice_pending = false;
    if (ctx) session_restore_set_pending(&state->session, ctx);
    debug_log("shell: heuristic fallback begin kind=%d delay=%d fw_support=%d ver=%u",
        ctx ? (int)ctx->kind : 0,
        state->home_init_delay,
        (int)state->firmware.support,
        state->firmware.contract_version);
}

void shell_state_begin_firmware_wait(ShellState* state, const LaunchContext* ctx) {
    if (!state) return;
    state->last_gate_source = SHELL_GATE_FIRMWARE;
    state->wait_reason = SHELL_WAIT_FIRMWARE_NOT_READY;
    state->home_init_pending = false;
    state->home_init_delay = 0;
    state->resume_notice_pending = false;
    if (ctx) session_restore_set_pending(&state->session, ctx);
    debug_log("shell: firmware wait begin kind=%d ver=%u shell=%d jump=%d route=%016llX flags=%016llX",
        ctx ? (int)ctx->kind : 0,
        state->firmware.contract_version,
        (int)state->firmware.shell_ready,
        (int)state->firmware.jump_ready,
        (unsigned long long)state->firmware.route_kind,
        (unsigned long long)state->firmware.flags);
}

bool shell_state_tick_home_init(ShellState* state) {
    if (!state || !state->home_init_pending) return false;
    if (state->home_init_delay > 0) state->home_init_delay--;
    if (state->home_init_delay > 0) return false;
    state->home_init_pending = false;
    state->home_init_ready = true;
    state->resume_notice_pending = true;
    debug_log("shell: HOME init jump ready");
    return true;
}

void shell_state_complete_home_init_return(ShellState* state) {
    if (!state) return;
    state->home_init_pending = false;
    state->wait_reason = SHELL_WAIT_NONE;
    state->resume_notice_pending = false;
    if (state->session.has_pending_launch) {
        session_restore_clear_pending(&state->session);
    }
    debug_log("shell: HOME init return complete");
}

void shell_state_note_launch_allowed(ShellState* state, const LaunchContext* ctx) {
    if (!state || !ctx) return;
    state->wait_reason = SHELL_WAIT_NONE;
    if (state->session.has_pending_launch) {
        debug_log("shell: deferred launch now allowed kind=%d source=%d", (int)ctx->kind, (int)state->last_gate_source);
    } else {
        debug_log("shell: launch allowed kind=%d source=%d", (int)ctx->kind, (int)state->last_gate_source);
    }
}

bool shell_state_consume_resume_notice(ShellState* state) {
    if (!state || !state->resume_notice_pending) return false;
    state->resume_notice_pending = false;
    debug_log("shell: resume notice consumed");
    return true;
}
