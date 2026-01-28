#include "fmux.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>

static int entry_cmp(const void* a, const void* b) {
    const FileEntry* ea = (const FileEntry*)a;
    const FileEntry* eb = (const FileEntry*)b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcasecmp(ea->name, eb->name);
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

bool build_dir_cache(const Target* target, TargetState* ts, DirCache* cache) {
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
    while ((ent = readdir(dir)) && cache->count < MAX_ENTRIES) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        bool is_dir = ent->d_type == DT_DIR;
        if (ent->d_type == DT_UNKNOWN) {
            char full[512];
            snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
            struct stat st;
            if (stat(full, &st) == 0) is_dir = S_ISDIR(st.st_mode);
        }
        if (!entry_allowed(target, ent->d_name, is_dir)) continue;
        FileEntry* fe = &cache->entries[cache->count++];
        snprintf(fe->name, sizeof(fe->name), "%s", ent->d_name);
        fe->is_dir = is_dir;
    }
    closedir(dir);
    qsort(cache->entries, cache->count, sizeof(FileEntry), entry_cmp);
    snprintf(cache->path, sizeof(cache->path), "%s", ts->path);
    cache->valid = true;
    return true;
}

bool cache_matches(const DirCache* cache, const char* path) {
    return cache->valid && !strcmp(cache->path, path);
}
