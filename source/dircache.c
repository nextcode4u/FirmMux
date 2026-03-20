#include "fmux.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>

static int g_sort_mode = 0;
static const StatsData* g_sort_stats = NULL;
static int g_sort_kind = STATS_KIND_NONE;
static char g_sort_path[512];
static bool is_emulator_target(const Target* target);

static int is_letter_char(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool is_emulator_target(const Target* target) {
    return target && !strcmp(target->type, "retroarch_system");
}

static int entry_cmp(const void* a, const void* b) {
    const FileEntry* ea = (const FileEntry*)a;
    const FileEntry* eb = (const FileEntry*)b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    if (g_sort_mode == 2 && !ea->is_dir && !eb->is_dir && g_sort_stats && g_sort_kind != STATS_KIND_NONE) {
        char base_sd[STATS_KEY_SIZE];
        char akey[STATS_KEY_SIZE];
        char bkey[STATS_KEY_SIZE];
        make_sd_path(g_sort_path, base_sd, sizeof(base_sd));
        normalize_path_sd(base_sd, sizeof(base_sd));
        path_join(base_sd, ea->name, akey, sizeof(akey));
        path_join(base_sd, eb->name, bkey, sizeof(bkey));
        bool af = stats_is_favorite(g_sort_stats, g_sort_kind, akey);
        bool bf = stats_is_favorite(g_sort_stats, g_sort_kind, bkey);
        if (af != bf) return af ? -1 : 1;
    }
    int a_letter = is_letter_char((unsigned char)ea->name[0]) ? 1 : 0;
    int b_letter = is_letter_char((unsigned char)eb->name[0]) ? 1 : 0;
    if (a_letter != b_letter) {
        if (g_sort_mode == 0 || g_sort_mode == 2) return a_letter - b_letter;
        return b_letter - a_letter;
    }
    int cmp = strcasecmp(ea->name, eb->name);
    if (cmp == 0) return 0;
    if (g_sort_mode == 1 && a_letter && b_letter) return -cmp;
    return cmp;
}

void sort_dir_cache(DirCache* cache, int sort_mode) {
    if (!cache || !cache->entries || cache->count <= 1) return;
    g_sort_mode = sort_mode;
    g_sort_stats = NULL;
    g_sort_kind = STATS_KIND_NONE;
    g_sort_path[0] = 0;
    qsort(cache->entries, cache->count, sizeof(FileEntry), entry_cmp);
}

static void sort_dir_cache_for_target(DirCache* cache, int sort_mode, const Target* target, const TargetState* ts, const StatsData* stats) {
    if (!cache || !cache->entries || cache->count <= 1) return;
    g_sort_mode = sort_mode;
    g_sort_stats = stats;
    g_sort_kind = STATS_KIND_NONE;
    g_sort_path[0] = 0;
    if (target && ts && ts->path[0]) {
        if (!strcmp(target->type, "homebrew_browser")) g_sort_kind = STATS_KIND_HOMEBREW;
        else if (!strcmp(target->type, "rom_browser") || is_emulator_target(target)) g_sort_kind = STATS_KIND_ROM;
        copy_str(g_sort_path, sizeof(g_sort_path), ts->path);
    }
    qsort(cache->entries, cache->count, sizeof(FileEntry), entry_cmp);
}

static bool has_extension(const char* name, const char* ext) {
    size_t ln = strlen(name);
    size_t le = strlen(ext);
    if (le > ln) return false;
    return strcasecmp(name + (ln - le), ext) == 0;
}

static bool entry_allowed(const Target* target, const char* name, bool is_dir) {
    if (is_dir) return true;
    if (!strcmp(target->type, "homebrew_browser")) {
        return has_extension(name, ".3dsx");
    }
    if (!strcmp(target->type, "rom_browser")) {
        if (target->ext_count == 0) return is_nds_name(name);
        for (int i = 0; i < target->ext_count; i++) {
            if (has_extension(name, target->extensions[i])) return true;
        }
        return false;
    }
    if (!strcmp(target->type, "retroarch_system")) {
        if (target->ext_count == 0) return true;
        for (int i = 0; i < target->ext_count; i++) {
            if (has_extension(name, target->extensions[i])) return true;
        }
        return false;
    }
    return true;
}

static bool should_hide_entry(const Target* target, const TargetState* ts, const char* name, bool is_dir) {
    if (!target || !ts || !name || is_dir) return false;
    if (strcmp(target->type, "homebrew_browser") != 0) return false;

    char p[256];
    copy_str(p, sizeof(p), ts->path);
    normalize_path(p);
    if (!strcasecmp(p, "/3ds/FirmMux")) {
        if (!strcasecmp(name, "boot.3dsx")) return true;
        if (!strcasecmp(name, "firmux-bootstrap-prep.3dsx")) return true;
    }
    return false;
}

bool build_dir_cache(const Target* target, TargetState* ts, DirCache* cache) {
    if (!target || !ts || !cache) return false;
    if (!cache->entries) {
        cache->entries = (FileEntry*)calloc(MAX_ENTRIES, sizeof(FileEntry));
        if (!cache->entries) {
            cache->capacity = 0;
            return false;
        }
        cache->capacity = MAX_ENTRIES;
    }
    cache->count = 0;
    cache->valid = false;
    char path[512];
    if (ts->path[0] == 0) {
        snprintf(ts->path, sizeof(ts->path), "%s", target->root[0] ? target->root : "/");
        normalize_path(ts->path);
    }
    make_sd_path(ts->path, path, sizeof(path));
    DIR* dir = opendir(path);
    if (!dir) return false;
    struct dirent* ent;
    while ((ent = readdir(dir)) && cache->count < cache->capacity) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        bool is_dir = ent->d_type == DT_DIR;
        if (ent->d_type == DT_UNKNOWN) {
            char full[512];
            snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
            struct stat st;
            if (stat(full, &st) == 0) is_dir = S_ISDIR(st.st_mode);
        }
        if (!entry_allowed(target, ent->d_name, is_dir)) continue;
        if (should_hide_entry(target, ts, ent->d_name, is_dir)) continue;
        FileEntry* fe = &cache->entries[cache->count++];
        snprintf(fe->name, sizeof(fe->name), "%s", ent->d_name);
        fe->is_dir = is_dir;
    }
    closedir(dir);
    sort_dir_cache_for_target(cache, ts ? ts->sort_mode : 0, target, ts, stats_sort_data());
    snprintf(cache->path, sizeof(cache->path), "%s", ts->path);
    cache->valid = true;
    return true;
}

bool cache_matches(const DirCache* cache, const char* path) {
    return cache->valid && !strcmp(cache->path, path);
}

void dir_cache_release(DirCache* cache) {
    if (!cache) return;
    if (cache->entries) free(cache->entries);
    cache->entries = NULL;
    cache->capacity = 0;
    cache->count = 0;
    cache->valid = false;
    cache->path[0] = 0;
}
