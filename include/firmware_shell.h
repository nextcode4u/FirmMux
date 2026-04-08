#ifndef FIRMWARE_SHELL_H
#define FIRMWARE_SHELL_H

#include "fmux.h"

typedef enum {
    FW_SUPPORT_NONE = 0,
    FW_SUPPORT_UNKNOWN,
    FW_SUPPORT_PRESENT,
} FirmwareSupport;

typedef enum {
    FW_SHELL_UNKNOWN = 0,
    FW_SHELL_NOT_READY,
    FW_SHELL_READY,
} FirmwareShellReadiness;

typedef enum {
    FW_JUMP_UNKNOWN = 0,
    FW_JUMP_NOT_READY,
    FW_JUMP_READY,
} FirmwareJumpReadiness;

typedef struct {
    bool probed;
    bool trustworthy;
    Result last_rc;
    FirmwareSupport support;
    u32 contract_version;
    FirmwareShellReadiness shell_ready;
    FirmwareJumpReadiness jump_ready;
    u64 route_kind;
    u64 flags;
    u64 raw_slots[8];
} FirmwareShellState;

void firmware_shell_init(FirmwareShellState* state);
void firmware_shell_refresh(FirmwareShellState* state);

#endif
