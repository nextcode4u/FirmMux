#include "nds_cheats.h"
#include "fmux.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static u32 fnv1a_32(const char* s) {
    u32 h = 2166136261u;
    while (s && *s) {
        h ^= (u8)*s++;
        h *= 16777619u;
    }
    return h;
}

static void selection_path(const char* rom_sd_path, char* out, size_t out_size) {
    u32 h = fnv1a_32(rom_sd_path ? rom_sd_path : "");
    snprintf(out, out_size, "%s/%08x.sel", NDS_CHEATS_DIR, (unsigned)h);
}

static u32 crc32_table[256];
static bool crc32_init = false;

static void crc32_init_table(void) {
    if (crc32_init) return;
    for (u32 i = 0; i < 256; i++) {
        u32 c = i;
        for (u32 j = 0; j < 8; j++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_init = true;
}

static u32 crc32_calc(const u8* data, size_t len) {
    crc32_init_table();
    u32 c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        c = crc32_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

static void to_sdmc_path(const char* in, char* out, size_t out_size) {
    if (!in || !out || out_size == 0) return;
    if (!strncasecmp(in, "sdmc:/", 6)) {
        snprintf(out, out_size, "%s", in);
    } else if (!strncasecmp(in, "sd:/", 4)) {
        snprintf(out, out_size, "sdmc:/%s", in + 4);
    } else {
        snprintf(out, out_size, "%s", in);
    }
}

static bool rom_header_data(const char* rom_path, u32* gamecode_out, u32* crc_out) {
    if (!rom_path || !gamecode_out || !crc_out) return false;
    char path[512];
    to_sdmc_path(rom_path, path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    u8 header[512];
    size_t r = fread(header, 1, sizeof(header), f);
    fclose(f);
    if (r != sizeof(header)) return false;
    u32 gc = (u32)header[12] | ((u32)header[13] << 8) | ((u32)header[14] << 16) | ((u32)header[15] << 24);
    *gamecode_out = gc;
    *crc_out = crc32_calc(header, sizeof(header));
    return true;
}

typedef struct {
    u32 gamecode;
    u32 crc32;
    u64 offset;
} DatIndex;

static bool search_cheat_block(FILE* f, u32 gamecode, u32 crc32, long* pos, size_t* size) {
    if (!f || !pos || !size) return false;
    *pos = 0;
    *size = 0;
    char header[12];
    if (fread(header, 1, 12, f) != 12) return false;
    if (strncmp(header, "R4 CheatCode", 12) != 0) return false;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0x100, SEEK_SET);

    DatIndex idx;
    DatIndex next;
    if (fread(&next, sizeof(next), 1, f) != 1) return false;

    bool done = false;
    while (!done) {
        idx = next;
        if (fread(&next, sizeof(next), 1, f) != 1) break;
        if (gamecode == idx.gamecode && crc32 == idx.crc32) {
            long end = next.offset ? (long)next.offset : file_size;
            *pos = (long)idx.offset;
            *size = (size_t)(end - (long)idx.offset);
            return (*pos > 0 && *size > 0);
        }
        if (!next.offset) done = true;
    }
    return false;
}

static bool parse_cheat_block(const char* buf, size_t size, NdsCheatList* out) {
    if (!buf || size < 16 || !out) return false;
    const char* end = buf + size;
    const char* game_title = buf;
    size_t gt_len = strnlen(game_title, size);
    if (gt_len + 4 >= size) return false;
    const char* p = game_title + gt_len + 4;
    uintptr_t up = (uintptr_t)p;
    up = (up + 3) & ~((uintptr_t)3);
    if (up + 36 > (uintptr_t)end) return false;
    const u32* ccode = (const u32*)up;
    u32 cheat_count = ccode[0] & 0x0FFFFFFF;
    ccode += 9;

    int capacity = 64;
    out->items = (NdsCheatItem*)calloc(capacity, sizeof(NdsCheatItem));
    out->count = 0;
    if (!out->items) return false;

    u32 cc = 0;
    while (cc < cheat_count) {
        u32 flags = *ccode;
        u32 folder_count = 1;
        const char* folder_name = NULL;
        const char* folder_note = NULL;
        u32 flag_item = 0;
        if ((flags >> 28) & 1) {
            flag_item |= 2;
            if ((flags >> 24) == 0x11) flag_item |= 4;
            folder_count = flags & 0x00FFFFFF;
            const char* folder_name_ptr = (const char*)((const char*)ccode + 4);
            const char* folder_note_ptr = folder_name_ptr + strnlen(folder_name_ptr, end - folder_name_ptr) + 1;
            folder_name = folder_name_ptr;
            folder_note = folder_note_ptr;
            const char* after = folder_note_ptr + strnlen(folder_note_ptr, end - folder_note_ptr) + 1;
            uintptr_t ap = ((uintptr_t)after + 3) & ~((uintptr_t)3);
            if (ap > (uintptr_t)end) break;
            ccode = (const u32*)ap;
            cc++;
        }

        for (u32 i = 0; i < folder_count && cc < cheat_count; i++) {
            const char* cheat_name = (const char*)((const char*)ccode + 4);
            if (cheat_name >= end) return true;
            const char* cheat_note = cheat_name + strnlen(cheat_name, end - cheat_name) + 1;
            if (cheat_note >= end) return true;
            const char* cheat_data_ptr = cheat_note + strnlen(cheat_note, end - cheat_note) + 1;
            uintptr_t ap = ((uintptr_t)cheat_data_ptr + 3) & ~((uintptr_t)3);
            if (ap + 4 > (uintptr_t)end) return true;
            const u32* cheat_data = (const u32*)ap;
            u32 cheat_len = *cheat_data++;

            if (cheat_len > 0 && out->count < 1024) {
                if (out->count >= capacity) {
                    capacity *= 2;
                    NdsCheatItem* n = (NdsCheatItem*)realloc(out->items, capacity * sizeof(NdsCheatItem));
                    if (!n) break;
                    out->items = n;
                }
                NdsCheatItem* it = &out->items[out->count++];
                snprintf(it->name, sizeof(it->name), "%s", cheat_name);
                snprintf(it->note, sizeof(it->note), "%s", cheat_note);
                it->data_len = (int)cheat_len;
                it->data = (u32*)malloc(sizeof(u32) * cheat_len);
                if (it->data) {
                    size_t bytes = (size_t)cheat_len * 4;
                    if ((const char*)cheat_data + bytes <= end) {
                        memcpy(it->data, cheat_data, bytes);
                    } else {
                        memset(it->data, 0, bytes);
                    }
                }
            }

            cc++;
            ccode = (const u32*)((const char*)ccode + (((flags & 0x00FFFFFF) + 1) * 4));
        }
    }
    return true;
}

bool nds_cheatdb_load(const char* rom_sd_path, NdsCheatList* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!rom_sd_path || !rom_sd_path[0]) return false;

    u32 gamecode = 0;
    u32 crc = 0;
    if (!rom_header_data(rom_sd_path, &gamecode, &crc)) return false;

    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/usrcheat.dat", NDS_CHEATS_DIR);
    FILE* f = fopen(db_path, "rb");
    if (!f) {
        out->has_db = false;
        return false;
    }
    out->has_db = true;

    long pos = 0;
    size_t size = 0;
    if (!search_cheat_block(f, gamecode, crc, &pos, &size)) {
        fclose(f);
        return false;
    }
    if (fseek(f, pos, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    char* buf = (char*)malloc(size);
    if (!buf) {
        fclose(f);
        return false;
    }
    if (fread(buf, 1, size, f) != size) {
        free(buf);
        fclose(f);
        return false;
    }
    fclose(f);
    bool ok = parse_cheat_block(buf, size, out);
    free(buf);
    return ok;
}

void nds_cheatdb_free(NdsCheatList* list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        free(list->items[i].data);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

void nds_cheatdb_load_selection(const char* rom_sd_path, NdsCheatList* list) {
    if (!rom_sd_path || !list || list->count <= 0) return;
    char path[512];
    selection_path(rom_sd_path, path, sizeof(path));
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return;
    }
    fclose(f);
    char* p = line;
    while (*p) {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        int idx = atoi(p);
        if (idx >= 0 && idx < list->count) list->items[idx].selected = true;
        while (*p && *p != ',' && *p != '\n' && *p != '\r') p++;
    }
}

void nds_cheatdb_save_selection(const char* rom_sd_path, const NdsCheatList* list) {
    if (!rom_sd_path || !list) return;
    char path[512];
    selection_path(rom_sd_path, path, sizeof(path));
    FILE* f = fopen(path, "w");
    if (!f) return;
    bool first = true;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].selected) {
            if (!first) fprintf(f, ",");
            fprintf(f, "%d", i);
            first = false;
        }
    }
    fprintf(f, "\n");
    fclose(f);
}

bool nds_cheatdb_write_cheat_data(const char* rom_sd_path, const NdsCheatList* list, bool cheats_enabled) {
    if (!rom_sd_path) return false;
    const char* out_path = "sdmc:/_nds/nds-bootstrap/cheatData.bin";
    if (!cheats_enabled || !list) {
        remove(out_path);
        return false;
    }
    size_t total_words = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].selected) total_words += list->items[i].data_len;
    }
    if (total_words == 0) {
        remove(out_path);
        return false;
    }
    FILE* f = fopen(out_path, "wb");
    if (!f) return false;
    for (int i = 0; i < list->count; i++) {
        if (!list->items[i].selected || list->items[i].data_len <= 0) continue;
        fwrite(list->items[i].data, 4, (size_t)list->items[i].data_len, f);
    }
    u32 end = 0xCF000000u;
    fwrite(&end, 4, 1, f);
    fclose(f);
    return true;
}
