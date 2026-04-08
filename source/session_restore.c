#include "session_restore.h"

#include <string.h>

void session_restore_init(SessionRestoreState* state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

void session_restore_set_pending(SessionRestoreState* state, const LaunchContext* ctx) {
    if (!state || !ctx) return;
    state->pending_launch = *ctx;
    state->has_pending_launch = true;
    state->return_to_firmmux_expected = true;
    debug_log("session: pending kind=%d title=%016llX applet=%lu path=%s",
        (int)ctx->kind,
        (unsigned long long)ctx->title_id,
        (unsigned long)ctx->applet_id,
        ctx->path);
}

void session_restore_clear_pending(SessionRestoreState* state) {
    if (!state) return;
    if (state->has_pending_launch) {
        debug_log("session: pending cleared kind=%d", (int)state->pending_launch.kind);
    }
    memset(&state->pending_launch, 0, sizeof(state->pending_launch));
    state->has_pending_launch = false;
}

void session_restore_commit_launch(SessionRestoreState* state, const LaunchContext* ctx) {
    if (!state || !ctx) return;
    state->last_launch = *ctx;
    state->has_last_launch = true;
    state->return_to_firmmux_expected = false;
    session_restore_clear_pending(state);
    debug_log("session: launch committed kind=%d title=%016llX applet=%lu path=%s",
        (int)ctx->kind,
        (unsigned long long)ctx->title_id,
        (unsigned long)ctx->applet_id,
        ctx->path);
}

void session_restore_note_home_return(SessionRestoreState* state) {
    if (!state) return;
    state->return_to_firmmux_expected = true;
    debug_log("session: HOME return requested");
}
