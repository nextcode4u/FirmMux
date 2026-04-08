#ifndef APT_BRIDGE_H
#define APT_BRIDGE_H

#include "fmux.h"

typedef struct {
    bool home_allowed;
    bool chainload_pending;
} AptBridgeState;

void apt_bridge_init(AptBridgeState* state, bool home_allowed);
void apt_bridge_set_home_allowed(AptBridgeState* state, bool allowed);
void apt_bridge_jump_home_menu(void);
bool apt_bridge_chainload_application(AptBridgeState* state, u64 title_id, FS_MediaType media);
bool apt_bridge_consume_chainload_pending(AptBridgeState* state);
bool apt_bridge_launch_system_applet(NS_APPID applet_id);

#endif
