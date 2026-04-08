#include "apt_bridge.h"

#define DIRECT_CHAINLOAD_INTENT_TYPE 0x10082u
#define DIRECT_CHAINLOAD_TARGET_TYPE 0x10083u

static void apt_bridge_set_direct_chainload_request(u64 title_id, FS_MediaType media) {
    svcKernelSetState(DIRECT_CHAINLOAD_TARGET_TYPE, (u32)media, (u32)title_id, (u32)(title_id >> 32));
    svcKernelSetState(DIRECT_CHAINLOAD_INTENT_TYPE, 1, 0, 0);
}

void apt_bridge_init(AptBridgeState* state, bool home_allowed) {
    if (!state) return;
    state->home_allowed = !home_allowed;
    state->chainload_pending = false;
    apt_bridge_set_home_allowed(state, home_allowed);
}

void apt_bridge_set_home_allowed(AptBridgeState* state, bool allowed) {
    if (!state) return;
    if (state->home_allowed == allowed) return;
    aptSetHomeAllowed(allowed);
    state->home_allowed = allowed;
}

void apt_bridge_jump_home_menu(void) {
    aptJumpToHomeMenu();
}

bool apt_bridge_chainload_application(AptBridgeState* state, u64 title_id, FS_MediaType media) {
    if (!state || !title_id) return false;
    apt_bridge_set_direct_chainload_request(title_id, media);
    aptSetChainloader(title_id, (u8)media);
    state->chainload_pending = true;
    return true;
}

bool apt_bridge_consume_chainload_pending(AptBridgeState* state) {
    if (!state || !state->chainload_pending) return false;
    state->chainload_pending = false;
    return true;
}

bool apt_bridge_launch_system_applet(NS_APPID applet_id) {
    u32 aptbuf[0x400 / 4];
    memset(aptbuf, 0, sizeof(aptbuf));
    aptLaunchSystemApplet(applet_id, aptbuf, sizeof(aptbuf), 0);
    return true;
}
