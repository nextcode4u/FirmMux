#ifndef SHELL_STATE_H
#define SHELL_STATE_H

#include "firmware_shell.h"
#include "session_restore.h"

typedef enum {
    SHELL_MODE_STOCK = 0,
    SHELL_MODE_FRONTEND,
} ShellMode;

typedef enum {
    SHELL_GATE_NONE = 0,
    SHELL_GATE_HEURISTIC,
    SHELL_GATE_FIRMWARE,
} ShellGateSource;

typedef enum {
    SHELL_WAIT_NONE = 0,
    SHELL_WAIT_HEURISTIC_HOME_INIT,
    SHELL_WAIT_FIRMWARE_NOT_READY,
} ShellWaitReason;

typedef struct {
    ShellMode mode;
    ShellGateSource last_gate_source;
    ShellWaitReason wait_reason;
    bool home_init_attempted;
    bool home_init_pending;
    bool home_init_ready;
    bool resume_notice_pending;
    int home_init_delay;
    FirmwareShellState firmware;
    SessionRestoreState session;
} ShellState;

void shell_state_init(ShellState* state);
void shell_state_cancel_home_init(ShellState* state);
void shell_state_begin_home_init(ShellState* state, const LaunchContext* ctx);
void shell_state_begin_firmware_wait(ShellState* state, const LaunchContext* ctx);
bool shell_state_tick_home_init(ShellState* state);
void shell_state_complete_home_init_return(ShellState* state);
void shell_state_note_launch_allowed(ShellState* state, const LaunchContext* ctx);
bool shell_state_consume_resume_notice(ShellState* state);

#endif
