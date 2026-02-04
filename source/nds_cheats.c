#include "nds_cheats.h"
#include "fmux.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

static u32 fnv1a_32(const char* s) {
    u32 h = 2166136261u;
    while (s && *s) {
        h ^= (u8)*s++;
        h *= 16777619u;
    }
    return h;
}

static void selection_path(const char* rom_sd_path, char* out, size_t out_size) {
    char norm[512];
    norm[0] = 0;
    if (rom_sd_path && rom_sd_path[0]) {
        copy_str(norm, sizeof(norm), rom_sd_path);
        normalize_path_to_sd_colon(norm, sizeof(norm));
    }
    u32 h = fnv1a_32(norm[0] ? norm : "");
    snprintf(out, out_size, "%s/%08x.sel", NDS_CHEATS_DIR, (unsigned)h);
}

static u32 crc32_calc(const u8* data, size_t len) {
    u32 crc = 0xFFFFFFFFu;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
        }
    }
    return crc;
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

static u32 read_u32_le(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u64 read_u64_le(const u8* p) {
    return (u64)read_u32_le(p) | ((u64)read_u32_le(p + 4) << 32);
}

static bool read_dat_index(FILE* f, u32* out_gamecode, u32* out_crc32, u64* out_offset) {
    u8 buf[16];
    if (fread(buf, 1, sizeof(buf), f) != sizeof(buf)) return false;
    *out_gamecode = read_u32_le(buf + 0);
    *out_crc32 = read_u32_le(buf + 4);
    *out_offset = read_u64_le(buf + 8);
    return true;
}

static bool open_usrcheat(const char* mode, FILE** out, char* path_out, size_t path_size) {
    if (!out) return false;
    *out = NULL;
    if (path_out && path_size) path_out[0] = 0;
    const char* p1 = NDS_CHEATS_DB_PATH;
    FILE* f = fopen(p1, mode);
    if (!f) return false;
    if (path_out && path_size) snprintf(path_out, path_size, "%s", p1);
    *out = f;
    return true;
}

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

    u32 gc = 0;
    u32 cr = 0;
    u64 off = 0;
    u32 ngc = 0;
    u32 ncr = 0;
    u64 noff = 0;
    if (!read_dat_index(f, &ngc, &ncr, &noff)) return false;

    bool done = false;
    while (!done) {
        gc = ngc;
        cr = ncr;
        off = noff;
        if (!read_dat_index(f, &ngc, &ncr, &noff)) break;
        if (gamecode == gc && crc32 == cr && off) {
            long end = noff ? (long)noff : file_size;
            *pos = (long)off;
            *size = (size_t)(end - (long)off);
            return (*pos > 0 && *size > 0);
        }
        if (!noff) done = true;
    }

    if (!gamecode) return false;
    fseek(f, 0x100, SEEK_SET);
    if (!read_dat_index(f, &ngc, &ncr, &noff)) return false;
    done = false;
    while (!done) {
        gc = ngc;
        cr = ncr;
        off = noff;
        if (!read_dat_index(f, &ngc, &ncr, &noff)) break;
        if (gamecode == gc && off) {
            long end = noff ? (long)noff : file_size;
            *pos = (long)off;
            *size = (size_t)(end - (long)off);
            return (*pos > 0 && *size > 0);
        }
        if (!noff) done = true;
    }

    return false;
}

static bool parse_cheat_block(const char* buf, size_t size, long base_offset, NdsCheatList* out) {
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
    char current_folder[64];
    current_folder[0] = 0;
    while (cc < cheat_count) {
        u32 folder_count = 1;
        u32 flag_item = 0;
        if ((*ccode >> 28) & 1) {
            flag_item |= 2;
            if ((*ccode >> 24) == 0x11) flag_item |= 4;
            folder_count = *ccode & 0x00FFFFFF;
            const char* folder_name_ptr = (const char*)((const char*)ccode + 4);
            const char* folder_note_ptr = folder_name_ptr + strnlen(folder_name_ptr, end - folder_name_ptr) + 1;
            const char* after = folder_note_ptr + strnlen(folder_note_ptr, end - folder_note_ptr) + 1;
            uintptr_t ap = ((uintptr_t)after + 3) & ~((uintptr_t)3);
            if (ap > (uintptr_t)end) break;
            copy_str(current_folder, sizeof(current_folder), folder_name_ptr ? folder_name_ptr : "");
            ccode = (const u32*)ap;
            cc++;
        }

        u32 select_value = 1;
        for (u32 i = 0; i < folder_count && cc < cheat_count; i++) {
            u32 cheat_flags = *ccode;
            const char* cheat_name = (const char*)((const char*)ccode + 4);
            if (cheat_name >= end) return true;
            const char* cheat_note = cheat_name + strnlen(cheat_name, end - cheat_name) + 1;
            if (cheat_note >= end) return true;
            const char* cheat_data_ptr = cheat_note + strnlen(cheat_note, end - cheat_note) + 1;
            uintptr_t ap = ((uintptr_t)cheat_data_ptr + 3) & ~((uintptr_t)3);
            if (ap + 4 > (uintptr_t)end) return true;
            const u32* cheat_data = (const u32*)ap;
            u32 cheat_len = *cheat_data++;

            if (out->count < 1024) {
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
                copy_str(it->folder, sizeof(it->folder), current_folder);
                if (cheat_len > 0) {
                    it->data = (u32*)malloc(sizeof(u32) * cheat_len);
                    if (it->data) {
                        size_t bytes = (size_t)cheat_len * 4;
                        if ((const char*)cheat_data + bytes <= end) {
                            memcpy(it->data, cheat_data, bytes);
                        } else {
                            memset(it->data, 0, bytes);
                        }
                    }
                } else {
                    it->data = NULL;
                }
                it->selected = false;
                if (base_offset > 0) {
                    it->db_offset = (u32)(base_offset + ((const char*)ccode - buf) + 3);
                } else {
                    it->db_offset = 0;
                }
            }

            cc++;
            ccode = (const u32*)((const char*)ccode + (((cheat_flags & 0x00FFFFFF) + 1) * 4));
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
    if (!rom_header_data(rom_sd_path, &gamecode, &crc)) {
        if (debug_log_enabled()) debug_log("cheats: rom header read failed %s", rom_sd_path);
        return false;
    }

    char db_path[512];
    FILE* f = NULL;
    if (!open_usrcheat("rb", &f, db_path, sizeof(db_path))) {
        out->has_db = false;
        if (debug_log_enabled()) debug_log("cheats: db missing %s", NDS_CHEATS_DB_PATH);
        return false;
    }
    out->has_db = true;

    long pos = 0;
    size_t size = 0;
    if (!search_cheat_block(f, gamecode, crc, &pos, &size)) {
        fclose(f);
        if (debug_log_enabled()) debug_log("cheats: no entry gamecode=%08X crc=%08X", gamecode, crc);
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
    bool ok = parse_cheat_block(buf, size, pos, out);
    free(buf);
    if (debug_log_enabled()) debug_log("cheats: load %s gamecode=%08X crc=%08X count=%d ok=%d", rom_sd_path, gamecode, crc, out->count, ok ? 1 : 0);
    return ok;
}

bool nds_cheatdb_has_cheats(const char* rom_sd_path) {
    if (!rom_sd_path || !rom_sd_path[0]) return false;
    u32 gamecode = 0;
    u32 crc = 0;
    if (!rom_header_data(rom_sd_path, &gamecode, &crc)) return false;

    char db_path[512];
    FILE* f = NULL;
    if (!open_usrcheat("rb", &f, db_path, sizeof(db_path))) return false;

    long pos = 0;
    size_t size = 0;
    bool ok = search_cheat_block(f, gamecode, crc, &pos, &size);
    fclose(f);
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
        if (debug_log_enabled()) debug_log("cheats: disabled or no list, removed %s", out_path);
        return false;
    }
    size_t total_words = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].selected) total_words += list->items[i].data_len;
    }
    if (total_words == 0) {
        remove(out_path);
        if (debug_log_enabled()) debug_log("cheats: no selected codes, removed %s", out_path);
        return false;
    }
    mkdir("sdmc:/_nds", 0777);
    mkdir("sdmc:/_nds/nds-bootstrap", 0777);
    FILE* f = fopen(out_path, "wb");
    if (!f) return false;
    for (int i = 0; i < list->count; i++) {
        if (!list->items[i].selected || list->items[i].data_len <= 0) continue;
        fwrite(list->items[i].data, 4, (size_t)list->items[i].data_len, f);
    }
    u32 end = 0xCF000000u;
    fwrite(&end, 4, 1, f);
    fclose(f);
    copy_file_simple(out_path, "sdmc:/_nds/firmmux/last_cheatData.bin");
    if (debug_log_enabled()) debug_log("cheats: wrote %u words to %s", (unsigned)total_words, out_path);
    return true;
}

bool nds_cheatdb_apply_usrcheat(const char* rom_sd_path, const NdsCheatList* list, bool cheats_enabled) {
    (void)rom_sd_path;
    if (!list) return false;
    char db_path[512];
    FILE* db = NULL;
    if (!open_usrcheat("r+b", &db, db_path, sizeof(db_path))) {
        if (debug_log_enabled()) debug_log("cheats: usrcheat missing %s", NDS_CHEATS_DB_PATH);
        return false;
    }
    bool any = false;
    for (int i = 0; i < list->count; i++) {
        const NdsCheatItem* it = &list->items[i];
        if (!it->db_offset) continue;
        u8 value = (cheats_enabled && it->selected) ? 1 : 0;
        if (fseek(db, (long)it->db_offset, SEEK_SET) != 0) continue;
        u8 old = 0;
        if (fread(&old, 1, 1, db) != 1) continue;
        if (old != value) {
            if (fseek(db, (long)it->db_offset, SEEK_SET) != 0) continue;
            fwrite(&value, 1, 1, db);
        }
        if (value) any = true;
    }
    fclose(db);
    bool ok = true;
    if (debug_log_enabled()) {
        debug_log("cheats: usrcheat updated %s (enabled=%d any=%d)", db_path, cheats_enabled ? 1 : 0, any ? 1 : 0);
    }
    return ok;
}
