#ifndef SESSION_RESTORE_H
#define SESSION_RESTORE_H

#include "fmux.h"

typedef enum {
    LAUNCH_KIND_NONE = 0,
    LAUNCH_KIND_CTR_TITLE,
    LAUNCH_KIND_SYSTEM_APPLET,
    LAUNCH_KIND_NDS_LOADER,
    LAUNCH_KIND_CARD_LAUNCHER,
    LAUNCH_KIND_HOMEBREW,
    LAUNCH_KIND_RETROARCH,
    LAUNCH_KIND_HOME_MENU,
} LaunchKind;

typedef struct {
    LaunchKind kind;
    u64 title_id;
    FS_MediaType media;
    NS_APPID applet_id;
    char target_id[32];
    char label[128];
    char path[512];
} LaunchContext;

typedef struct {
    bool has_last_launch;
    bool has_pending_launch;
    bool return_to_firmmux_expected;
    LaunchContext last_launch;
    LaunchContext pending_launch;
} SessionRestoreState;

void session_restore_init(SessionRestoreState* state);
void session_restore_set_pending(SessionRestoreState* state, const LaunchContext* ctx);
void session_restore_clear_pending(SessionRestoreState* state);
void session_restore_commit_launch(SessionRestoreState* state, const LaunchContext* ctx);
void session_restore_note_home_return(SessionRestoreState* state);

#endif
