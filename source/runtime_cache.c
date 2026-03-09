#include "runtime_cache.h"

#include <string.h>

#define RUNTIME_SLOT_COUNT 3

typedef struct {
    bool in_use;
    u32 use_tick;
    char target_id[32];
    TargetRuntime runtime;
} RuntimeSlot;

static RuntimeSlot g_slots[RUNTIME_SLOT_COUNT];
static u32 g_tick = 0;

void runtime_cache_init(void) {
    memset(g_slots, 0, sizeof(g_slots));
    g_tick = 0;
}

void runtime_cache_shutdown(void) {
    for (int i = 0; i < RUNTIME_SLOT_COUNT; i++) {
        dir_cache_release(&g_slots[i].runtime.cache);
        g_slots[i].in_use = false;
        g_slots[i].target_id[0] = 0;
        g_slots[i].runtime.root_missing = false;
        g_slots[i].use_tick = 0;
    }
    g_tick = 0;
}

TargetRuntime* runtime_get(const char* target_id) {
    if (!target_id || !target_id[0]) return NULL;

    for (int i = 0; i < RUNTIME_SLOT_COUNT; i++) {
        RuntimeSlot* slot = &g_slots[i];
        if (!slot->in_use) continue;
        if (strcmp(slot->target_id, target_id) == 0) {
            slot->use_tick = ++g_tick;
            return &slot->runtime;
        }
    }

    int victim = -1;
    for (int i = 0; i < RUNTIME_SLOT_COUNT; i++) {
        if (!g_slots[i].in_use) {
            victim = i;
            break;
        }
    }
    if (victim < 0) {
        victim = 0;
        for (int i = 1; i < RUNTIME_SLOT_COUNT; i++) {
            if (g_slots[i].use_tick < g_slots[victim].use_tick) victim = i;
        }
    }

    RuntimeSlot* slot = &g_slots[victim];
    slot->in_use = true;
    slot->use_tick = ++g_tick;
    copy_str(slot->target_id, sizeof(slot->target_id), target_id);
    slot->runtime.root_missing = false;
    slot->runtime.cache.count = 0;
    slot->runtime.cache.valid = false;
    slot->runtime.cache.path[0] = 0;
    return &slot->runtime;
}
