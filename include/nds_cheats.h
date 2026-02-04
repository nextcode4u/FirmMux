#ifndef NDS_CHEATS_H
#define NDS_CHEATS_H

#include <stddef.h>
#include <stdbool.h>
#include <3ds.h>

typedef struct {
    char name[128];
    char note[256];
    char folder[64];
    u32* data;
    int data_len;
    bool selected;
    u32 db_offset;
} NdsCheatItem;

typedef struct {
    NdsCheatItem* items;
    int count;
    bool has_db;
} NdsCheatList;

bool nds_cheatdb_load(const char* rom_sd_path, NdsCheatList* out);
bool nds_cheatdb_has_cheats(const char* rom_sd_path);
void nds_cheatdb_free(NdsCheatList* list);
void nds_cheatdb_load_selection(const char* rom_sd_path, NdsCheatList* list);
void nds_cheatdb_save_selection(const char* rom_sd_path, const NdsCheatList* list);
bool nds_cheatdb_write_cheat_data(const char* rom_sd_path, const NdsCheatList* list, bool cheats_enabled);
bool nds_cheatdb_apply_usrcheat(const char* rom_sd_path, const NdsCheatList* list, bool cheats_enabled);

#endif
