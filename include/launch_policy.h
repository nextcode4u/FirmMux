#ifndef LAUNCH_POLICY_H
#define LAUNCH_POLICY_H

#include "apt_bridge.h"
#include "shell_state.h"

typedef enum {
    LAUNCH_GATE_ALLOW = 0,
    LAUNCH_GATE_DEFERRED_HOME_INIT,
    LAUNCH_GATE_DEFERRED_FIRMWARE,
    LAUNCH_GATE_WAITING_HOME_INIT,
    LAUNCH_GATE_FAILED,
} LaunchGateResult;

void launch_policy_make_title_context(LaunchContext* ctx, const char* target_id, const char* label, u64 title_id, FS_MediaType media);
void launch_policy_make_applet_context(LaunchContext* ctx, const char* target_id, const char* label, NS_APPID applet_id);
void launch_policy_make_path_context(LaunchContext* ctx, LaunchKind kind, const char* target_id, const char* label, const char* path);
LaunchGateResult launch_policy_can_launch_now(ShellState* shell, const LaunchContext* ctx, char* status_message, size_t status_size);
bool launch_policy_launch_ctr_title_with_fallback_media(ShellState* shell, AptBridgeState* apt_state, const LaunchContext* ctx, char* status_message, size_t status_size);
bool launch_policy_launch_system_applet(ShellState* shell, AptBridgeState* apt_state, const LaunchContext* ctx, char* status_message, size_t status_size);

#endif
