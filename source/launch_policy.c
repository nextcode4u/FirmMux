#include "launch_policy.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void set_status(char* status_message, size_t status_size, const char* fmt, ...) {
    if (!status_message || status_size == 0 || !fmt) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status_message, status_size, fmt, ap);
    va_end(ap);
}

static void launch_policy_note_firmware_fallback(const FirmwareShellState* fw, const LaunchContext* ctx) {
    if (!fw || !ctx) return;
    debug_log("launch_policy: firmware fallback kind=%d support=%d ver=%u shell=%d jump=%d route=%016llX",
        (int)ctx->kind,
        (int)fw->support,
        fw->contract_version,
        (int)fw->shell_ready,
        (int)fw->jump_ready,
        (unsigned long long)fw->route_kind);
}

static bool launch_policy_launch_ctr_title_once(AptBridgeState* apt_state, u64 title_id, FS_MediaType media, Result* out_rc) {
    apt_bridge_set_home_allowed(apt_state, false);
    bool ok = apt_bridge_chainload_application(apt_state, title_id, media);
    if (out_rc) *out_rc = ok ? 0 : MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLET, RD_INVALID_POINTER);
    return ok;
}

void launch_policy_make_title_context(LaunchContext* ctx, const char* target_id, const char* label, u64 title_id, FS_MediaType media) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->kind = LAUNCH_KIND_CTR_TITLE;
    ctx->title_id = title_id;
    ctx->media = media;
    copy_str(ctx->target_id, sizeof(ctx->target_id), target_id ? target_id : "");
    copy_str(ctx->label, sizeof(ctx->label), label ? label : "");
}

void launch_policy_make_applet_context(LaunchContext* ctx, const char* target_id, const char* label, NS_APPID applet_id) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->kind = LAUNCH_KIND_SYSTEM_APPLET;
    ctx->applet_id = applet_id;
    copy_str(ctx->target_id, sizeof(ctx->target_id), target_id ? target_id : "");
    copy_str(ctx->label, sizeof(ctx->label), label ? label : "");
}

void launch_policy_make_path_context(LaunchContext* ctx, LaunchKind kind, const char* target_id, const char* label, const char* path) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->kind = kind;
    copy_str(ctx->target_id, sizeof(ctx->target_id), target_id ? target_id : "");
    copy_str(ctx->label, sizeof(ctx->label), label ? label : "");
    copy_str(ctx->path, sizeof(ctx->path), path ? path : "");
}

LaunchGateResult launch_policy_can_launch_now(ShellState* shell, const LaunchContext* ctx, char* status_message, size_t status_size) {
    if (!shell || !ctx) {
        set_status(status_message, status_size, "Launch policy unavailable");
        return LAUNCH_GATE_FAILED;
    }
    firmware_shell_refresh(&shell->firmware);
    if (shell->firmware.trustworthy) {
        shell->last_gate_source = SHELL_GATE_FIRMWARE;
        debug_log("launch_policy: firmware readiness kind=%d shell=%d jump=%d route=%016llX",
            (int)ctx->kind,
            (int)shell->firmware.shell_ready,
            (int)shell->firmware.jump_ready,
            (unsigned long long)shell->firmware.route_kind);
        if (shell->firmware.jump_ready == FW_JUMP_READY) {
            shell_state_note_launch_allowed(shell, ctx);
            debug_log("launch_policy: allow kind=%d reason=firmware_ready", (int)ctx->kind);
            return LAUNCH_GATE_ALLOW;
        }
        if (shell->firmware.shell_ready == FW_SHELL_NOT_READY || shell->firmware.jump_ready == FW_JUMP_NOT_READY) {
            shell_state_begin_firmware_wait(shell, ctx);
            set_status(status_message, status_size, "Shell not ready yet. Launch deferred.");
            debug_log("launch_policy: defer kind=%d reason=firmware_not_ready", (int)ctx->kind);
            return LAUNCH_GATE_DEFERRED_FIRMWARE;
        }
    }
    if (shell->firmware.support != FW_SUPPORT_NONE || shell->firmware.contract_version != 0) {
        launch_policy_note_firmware_fallback(&shell->firmware, ctx);
    }
    if (!shell->home_init_attempted) {
        shell_state_begin_home_init(shell, ctx);
        if (shell->firmware.support == FW_SUPPORT_UNKNOWN) {
            set_status(status_message, status_size, "Firmware contract unknown. HOME fallback.");
        } else if (shell->firmware.support == FW_SUPPORT_NONE) {
            set_status(status_message, status_size, "Firmware contract unsupported. HOME fallback.");
        } else {
            set_status(status_message, status_size, "HOME init missing. Launch deferred.");
        }
        debug_log("launch_policy: defer kind=%d reason=home_init_missing", (int)ctx->kind);
        return LAUNCH_GATE_DEFERRED_HOME_INIT;
    }
    if (shell->home_init_pending) {
        set_status(status_message, status_size, "HOME init pending. Return to FirmMux and retry.");
        debug_log("launch_policy: wait kind=%d reason=home_init_pending", (int)ctx->kind);
        return LAUNCH_GATE_WAITING_HOME_INIT;
    }
    shell->last_gate_source = SHELL_GATE_HEURISTIC;
    shell_state_note_launch_allowed(shell, ctx);
    debug_log("launch_policy: allow kind=%d reason=heuristic_ready", (int)ctx->kind);
    return LAUNCH_GATE_ALLOW;
}

bool launch_policy_launch_ctr_title_with_fallback_media(ShellState* shell, AptBridgeState* apt_state, const LaunchContext* ctx, char* status_message, size_t status_size) {
    if (!shell || !apt_state || !ctx) {
        set_status(status_message, status_size, "Launch policy unavailable");
        return false;
    }
    LaunchGateResult gate = launch_policy_can_launch_now(shell, ctx, status_message, status_size);
    if (gate != LAUNCH_GATE_ALLOW) return false;

    Result rc = 0;
    if (launch_policy_launch_ctr_title_once(apt_state, ctx->title_id, ctx->media, &rc)) {
        session_restore_commit_launch(&shell->session, ctx);
        debug_log("launch_policy: title launch ok title=%016llX media=%d", (unsigned long long)ctx->title_id, (int)ctx->media);
        return true;
    }

    set_status(status_message, status_size, "Launch failed %08lX", (unsigned long)rc);
    debug_log("launch_policy: title launch fail title=%016llX rc=%08lX", (unsigned long long)ctx->title_id, (unsigned long)rc);
    return false;
}

bool launch_policy_launch_system_applet(ShellState* shell, AptBridgeState* apt_state, const LaunchContext* ctx, char* status_message, size_t status_size) {
    if (!shell || !apt_state || !ctx) {
        set_status(status_message, status_size, "Launch policy unavailable");
        return false;
    }
    LaunchGateResult gate = launch_policy_can_launch_now(shell, ctx, status_message, status_size);
    if (gate != LAUNCH_GATE_ALLOW) return false;
    apt_bridge_set_home_allowed(apt_state, false);
    session_restore_commit_launch(&shell->session, ctx);
    debug_log("launch_policy: applet launch applet=%lu", (unsigned long)ctx->applet_id);
    return apt_bridge_launch_system_applet(ctx->applet_id);
}
