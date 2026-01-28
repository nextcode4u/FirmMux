#include "fmux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

typedef struct {
    const char* key;
    const char* display;
} KnownSystem;

static const KnownSystem g_known_systems[] = {
    { "a26", "Atari 2600" },
    { "a52", "Atari 5200" },
    { "a78", "Atari 7800" },
    { "col", "ColecoVision" },
    { "cpc", "Amstrad CPC" },
    { "gb",  "Game Boy" },
    { "gen", "Genesis" },
    { "gg",  "Game Gear" },
    { "intv","Intellivision" },
    { "m5",  "Sord M5" },
    { "nes", "NES" },
    { "ngp", "Neo Geo Pocket" },
    { "pkmni","PokeMini" },
    { "sg",  "SG-1000" },
    { "sms", "Master System" },
    { "snes","SNES" },
    { "tg16","TurboGrafx-16" },
    { "ws",  "WonderSwan" }
};

static const int g_known_system_count = (int)(sizeof(g_known_systems) / sizeof(g_known_systems[0]));

static void lower_copy(char* out, size_t out_size, const char* in) {
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!in) return;
    size_t n = strlen(in);
    if (n >= out_size) n = out_size - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)in[i]);
    out[n] = 0;
}

static bool parse_json_string_limited(const char* start, const char* limit, const char* key, char* out, size_t out_size) {
    if (!start || !limit || !key || !out || out_size == 0 || start >= limit) return false;
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(start, needle);
    if (!p || p >= limit) return false;
    p = strchr(p, ':');
    if (!p || p >= limit) return false;
    p++;
    while (p < limit && *p && isspace((unsigned char)*p)) p++;
    if (p >= limit || *p != '"') return false;
    p++;
    const char* q = strchr(p, '"');
    if (!q || q > limit) return false;
    size_t n = (size_t)(q - p);
    if (n >= out_size) n = out_size - 1;
    memcpy(out, p, n);
    out[n] = 0;
    return true;
}

static bool parse_json_bool_limited(const char* start, const char* limit, const char* key, bool* out) {
    if (!start || !limit || !key || !out || start >= limit) return false;
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(start, needle);
    if (!p || p >= limit) return false;
    p = strchr(p, ':');
    if (!p || p >= limit) return false;
    p++;
    while (p < limit && *p && isspace((unsigned char)*p)) p++;
    if (p >= limit) return false;
    if (!strncmp(p, "true", 4)) { *out = true; return true; }
    if (!strncmp(p, "false", 5)) { *out = false; return true; }
    return false;
}

static void set_system_defaults(EmuSystem* sys, const char* key, const char* display) {
    if (!sys || !key || !display) return;
    memset(sys, 0, sizeof(*sys));
    lower_copy(sys->key, sizeof(sys->key), key);
    copy_str(sys->display_name, sizeof(sys->display_name), display);
    sys->enabled = true;
    snprintf(sys->rom_folder, sizeof(sys->rom_folder), "sd:/roms/%s", key);
}

static void emu_set_defaults(EmuConfig* cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->version = 1;
    for (int i = 0; i < g_known_system_count && cfg->count < MAX_SYSTEMS; i++) {
        set_system_defaults(&cfg->systems[cfg->count++], g_known_systems[i].key, g_known_systems[i].display);
    }
}

static bool write_default_emulators_file(const EmuConfig* cfg) {
    if (!cfg) return false;
    ensure_dirs();
    mkdir(EMU_EXT_DIR, 0777);
    FILE* f = fopen(RETRO_EMULATORS_PATH, "w");
    if (!f) return false;
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": %d,\n", cfg->version > 0 ? cfg->version : 1);
    fprintf(f, "  \"systems\": {\n");
    for (int i = 0; i < cfg->count; i++) {
        const EmuSystem* s = &cfg->systems[i];
        fprintf(f, "    \"%s\": {\"displayName\":\"%s\",\"enabled\":%s,\"romFolder\":\"%s\"}%s\n",
            s->key,
            s->display_name,
            s->enabled ? "true" : "false",
            s->rom_folder,
            (i + 1 < cfg->count) ? "," : "");
    }
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

static const char* find_system_block(const char* text, const char* key, const char** out_limit) {
    if (out_limit) *out_limit = NULL;
    if (!text || !key) return NULL;
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(text, needle);
    if (!p) return NULL;
    const char* lb = strchr(p, '{');
    if (!lb) return NULL;
    const char* rb = strchr(lb, '}');
    if (!rb) return NULL;
    if (out_limit) *out_limit = rb;
    return lb;
}

static bool parse_emulators_text(const char* text, EmuConfig* out_cfg) {
    if (!text || !out_cfg) return false;
    EmuConfig tmp;
    emu_set_defaults(&tmp);

    const char* systems = strstr(text, "\"systems\"");
    if (!systems) return false;

    int parsed_blocks = 0;
    for (int i = 0; i < tmp.count; i++) {
        EmuSystem* sys = &tmp.systems[i];
        const char* limit = NULL;
        const char* block = find_system_block(text, sys->key, &limit);
        if (!block || !limit || block >= limit) continue;
        parsed_blocks++;

        char disp[64] = {0};
        if (parse_json_string_limited(block, limit, "displayName", disp, sizeof(disp)) && disp[0]) {
            copy_str(sys->display_name, sizeof(sys->display_name), disp);
        }

        bool en = true;
        if (parse_json_bool_limited(block, limit, "enabled", &en)) sys->enabled = en;

        char folder[256] = {0};
        if (parse_json_string_limited(block, limit, "romFolder", folder, sizeof(folder)) && folder[0]) {
            copy_str(sys->rom_folder, sizeof(sys->rom_folder), folder);
        }
    }

    if (parsed_blocks == 0) return false;
    *out_cfg = tmp;
    return true;
}

static void backup_external_emulators(void) {
    u8* data = NULL;
    size_t size = 0;
    if (!read_file(RETRO_EMULATORS_PATH, &data, &size) || !data || size == 0) {
        if (data) free(data);
        return;
    }
    ensure_dirs();
    FILE* f = fopen(RETRO_EMULATORS_BAK_PATH, "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
    }
    free(data);
    remove(RETRO_EMULATORS_PATH);
}

bool load_or_create_emulators(EmuConfig* cfg, bool* regenerated) {
    if (regenerated) *regenerated = false;
    if (!cfg) return false;
    ensure_dirs();
    mkdir(EMU_EXT_DIR, 0777);

    if (!file_exists(RETRO_EMULATORS_PATH)) {
        emu_set_defaults(cfg);
        if (!write_default_emulators_file(cfg)) return false;
        if (regenerated) *regenerated = true;
        retro_log_line("emulators.json regenerated (missing)");
        return true;
    }

    u8* data = NULL;
    size_t size = 0;
    if (!read_file(RETRO_EMULATORS_PATH, &data, &size) || !data || size == 0) {
        emu_set_defaults(cfg);
        return false;
    }

    char* text = (char*)malloc(size + 1);
    if (!text) {
        free(data);
        emu_set_defaults(cfg);
        return false;
    }
    memcpy(text, data, size);
    text[size] = 0;
    free(data);

    if (!parse_emulators_text(text, cfg)) {
        free(text);
        backup_external_emulators();
        emu_set_defaults(cfg);
        if (write_default_emulators_file(cfg)) {
            if (regenerated) *regenerated = true;
            retro_log_line("emulators.json regenerated (invalid)");
            return true;
        }
        return false;
    }

    free(text);
    return true;
}

bool save_emulators(const EmuConfig* cfg) {
    if (!cfg) return false;
    return write_default_emulators_file(cfg);
}

const EmuSystem* emu_find_by_key(const EmuConfig* cfg, const char* key) {
    if (!cfg || !key || !key[0]) return NULL;
    for (int i = 0; i < cfg->count; i++) {
        if (!strcasecmp(cfg->systems[i].key, key)) return &cfg->systems[i];
    }
    return NULL;
}

EmuSystem* emu_find_by_key_mut(EmuConfig* cfg, const char* key) {
    if (!cfg || !key || !key[0]) return NULL;
    for (int i = 0; i < cfg->count; i++) {
        if (!strcasecmp(cfg->systems[i].key, key)) return &cfg->systems[i];
    }
    return NULL;
}

static int prefix_match_len(const char* path, const char* prefix) {
    if (!path || !prefix) return 0;
    size_t lp = strlen(prefix);
    if (lp == 0) return 0;
    if (strncasecmp(path, prefix, lp) != 0) return 0;
    if (path[lp] == 0 || path[lp] == '/') return (int)lp;
    return 0;
}

const EmuSystem* emu_find_by_path(const EmuConfig* cfg, const char* rom_sd_path) {
    if (!cfg || !rom_sd_path || !rom_sd_path[0]) return NULL;
    char norm_path[512];
    copy_str(norm_path, sizeof(norm_path), rom_sd_path);
    normalize_path_sd(norm_path, sizeof(norm_path));

    const EmuSystem* best = NULL;
    int best_len = 0;
    for (int i = 0; i < cfg->count; i++) {
        char norm_folder[512];
        copy_str(norm_folder, sizeof(norm_folder), cfg->systems[i].rom_folder);
        normalize_path_sd(norm_folder, sizeof(norm_folder));
        int len = prefix_match_len(norm_path, norm_folder);
        if (len > best_len) {
            best_len = len;
            best = &cfg->systems[i];
        }
    }
    return best;
}

static void parent_folder_name(const char* rom_sd_path, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!rom_sd_path || !rom_sd_path[0]) return;
    char tmp[512];
    copy_str(tmp, sizeof(tmp), rom_sd_path);
    normalize_path_sd(tmp, sizeof(tmp));
    char* last = strrchr(tmp, '/');
    if (!last) return;
    *last = 0;
    const char* parent = strrchr(tmp, '/');
    parent = parent ? parent + 1 : tmp;
    lower_copy(out, out_size, parent);
}

bool emu_resolve_system(const EmuConfig* cfg, const char* rom_sd_path, const char* fallback_key, char* out_key, size_t out_size) {
    if (!out_key || out_size == 0) return false;
    out_key[0] = 0;
    const EmuSystem* by_path = emu_find_by_path(cfg, rom_sd_path);
    if (by_path) {
        copy_str(out_key, out_size, by_path->key);
        return true;
    }
    char parent[32];
    parent_folder_name(rom_sd_path, parent, sizeof(parent));
    if (parent[0]) {
        copy_str(out_key, out_size, parent);
        return true;
    }
    if (fallback_key && fallback_key[0]) {
        lower_copy(out_key, out_size, fallback_key);
        return true;
    }
    return false;
}

int emu_known_system_keys(const char** out_keys, int max_keys) {
    if (!out_keys || max_keys <= 0) return 0;
    int count = 0;
    for (int i = 0; i < g_known_system_count && count < max_keys; i++) {
        out_keys[count++] = g_known_systems[i].key;
    }
    return count;
}
