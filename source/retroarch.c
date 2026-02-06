#include "fmux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <stdarg.h>
#include <3ds/services/apt.h>

static bool g_retro_log_enabled = false;
static bool g_retro_log_touched = false;

static const char* g_retro_core_dirs[] = {
    RETRO_CORES_DIR,
    "sdmc:/3ds/RetroArch/cores",
    "sdmc:/3ds/retroarch/cores"
};
static const int g_retro_core_dir_count = (int)(sizeof(g_retro_core_dirs) / sizeof(g_retro_core_dirs[0]));

typedef struct {
    const char* folder;
    const char* core;
    const char* exts[MAX_EXTENSIONS];
    int ext_count;
} DefaultRule;

static const DefaultRule g_default_rules[] = {
    { "a26",  "Stella 2014",     { "a26" }, 1 },
    { "a52",  "Atari800",        { "a52" }, 1 },
    { "a78",  "ProSystem",       { "a78" }, 1 },
    { "col",  "blueMSX",         { "col" }, 1 },
    { "cpc",  "Caprice32",       { "dsk", "cdt", "cpr" }, 3 },
    { "gb",   "Gambatte",        { "gb", "gbc" }, 2 },
    { "gen",  "Genesis Plus GX", { "gen", "md", "smd", "bin" }, 4 },
    { "gg",   "Genesis Plus GX", { "gg" }, 1 },
    { "intv", "FreeIntv",        { "intv", "bin", "rom" }, 3 },
    { "m5",   "O2EM",            { "bin" }, 1 },
    { "nes",  "Nestopia UE",     { "nes" }, 1 },
    { "ngp",  "Beetle NeoPop",   { "ngp", "ngc" }, 2 },
    { "pkmni","PokeMini",        { "min" }, 1 },
    { "sg",   "Genesis Plus GX", { "sg" }, 1 },
    { "sms",  "Genesis Plus GX", { "sms" }, 1 },
    { "snes", "Snes9x 2002",     { "smc", "sfc" }, 2 },
    { "tg16", "Beetle PCE Fast", { "pce" }, 1 },
    { "ws",   "Beetle Cygne",    { "ws", "wsc" }, 2 }
};

static const int g_default_rule_count = (int)(sizeof(g_default_rules) / sizeof(g_default_rules[0]));

typedef struct {
    const char* label;
    const char* candidates[6];
    int count;
} CoreMap;

static const CoreMap g_core_map[] = {
    { "Stella 2014",     { "stella2014", "stella" }, 2 },
    { "Atari800",        { "atari800" }, 1 },
    { "ProSystem",       { "prosystem" }, 1 },
    { "blueMSX",         { "bluemsx", "blue_msx", "msx" }, 3 },
    { "Caprice32",       { "cap32", "caprice32" }, 2 },
    { "Gambatte",        { "gambatte" }, 1 },
    { "Genesis Plus GX", { "genesis_plus_gx", "genesisplusgx", "genesis" }, 3 },
    { "FreeIntv",        { "freeintv" }, 1 },
    { "O2EM",            { "o2em" }, 1 },
    { "Nestopia UE",     { "nestopia_ue", "nestopia" }, 2 },
    { "Beetle NeoPop",   { "beetle_ngp", "mednafen_ngp", "neopop", "ngp" }, 4 },
    { "PokeMini",        { "pokemini" }, 1 },
    { "Snes9x 2002",     { "snes9x2002", "snes9x_2002" }, 2 },
    { "Beetle PCE Fast", { "beetle_pce_fast", "mednafen_pce_fast", "pce_fast", "pcefast" }, 4 },
    { "Beetle Cygne",    { "beetle_cygne", "mednafen_wswan", "wswan", "wonderswan", "cygne" }, 5 }
};

static const int g_core_map_count = (int)(sizeof(g_core_map) / sizeof(g_core_map[0]));

static void lower_copy(char* out, size_t out_size, const char* in) {
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!in) return;
    size_t n = strlen(in);
    if (n >= out_size) n = out_size - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)in[i]);
    out[n] = 0;
}

static void sanitize_key(char* out, size_t out_size, const char* in) {
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!in) return;
    size_t w = 0;
    for (size_t i = 0; in[i] && w + 1 < out_size; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c)) out[w++] = (char)tolower(c);
    }
    out[w] = 0;
}

static bool parse_json_string(const char* text, const char* key, char* out, size_t out_size) {
    if (!text || !key || !out || out_size == 0) return false;
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(text, needle);
    if (!p) return false;
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return false;
    p++;
    const char* q = strchr(p, '"');
    if (!q) return false;
    size_t n = (size_t)(q - p);
    if (n >= out_size) n = out_size - 1;
    memcpy(out, p, n);
    out[n] = 0;
    return true;
}

static bool parse_json_int(const char* text, const char* key, int* out) {
    if (!text || !key || !out) return false;
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = strstr(text, needle);
    if (!p) return false;
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    char buf[32];
    size_t w = 0;
    while (*p && (isdigit((unsigned char)*p) || *p == '-') && w + 1 < sizeof(buf)) buf[w++] = *p++;
    buf[w] = 0;
    if (w == 0) return false;
    *out = atoi(buf);
    return true;
}

static int parse_ext_array(const char* start, const char* limit, char out_exts[MAX_EXTENSIONS][16]) {
    if (!start || !limit || !out_exts || start >= limit) return 0;
    const char* lb = strchr(start, '[');
    if (!lb || lb >= limit) return 0;
    const char* rb = strchr(lb, ']');
    if (!rb || rb > limit) rb = limit;
    int count = 0;
    const char* p = lb;
    while (p && p < rb && count < MAX_EXTENSIONS) {
        p = strchr(p, '"');
        if (!p || p >= rb) break;
        p++;
        const char* q = strchr(p, '"');
        if (!q || q > rb) break;
        size_t n = (size_t)(q - p);
        if (n > 0) {
            char tmp[32];
            if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
            memcpy(tmp, p, n);
            tmp[n] = 0;
            lower_copy(out_exts[count], sizeof(out_exts[count]), tmp);
            count++;
        }
        p = q + 1;
    }
    return count;
}

static void rules_set_defaults(RetroRules* rules) {
    if (!rules) return;
    memset(rules, 0, sizeof(*rules));
    rules->version = 1;
    copy_str(rules->mode, sizeof(rules->mode), "pure-retroarch-3dsx");
    copy_str(rules->retroarch_entry, sizeof(rules->retroarch_entry), RETRO_ENTRY_DEFAULT);
    copy_str(rules->handoff_path, sizeof(rules->handoff_path), "sd:/3ds/emulators/launch.json");
    for (int i = 0; i < g_default_rule_count && rules->rule_count < MAX_RETRO_RULES; i++) {
        RetroRule* r = &rules->rules[rules->rule_count++];
        lower_copy(r->folder, sizeof(r->folder), g_default_rules[i].folder);
        copy_str(r->core, sizeof(r->core), g_default_rules[i].core);
        r->ext_count = 0;
        for (int e = 0; e < g_default_rules[i].ext_count && r->ext_count < MAX_EXTENSIONS; e++) {
            lower_copy(r->extensions[r->ext_count], sizeof(r->extensions[r->ext_count]), g_default_rules[i].exts[e]);
            r->ext_count++;
        }
    }
}

static bool write_default_rules_file(void) {
    ensure_dirs();
    mkdir(EMU_EXT_DIR, 0777);
    FILE* f = fopen(RETRO_RULES_PATH, "w");
    if (!f) return false;
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"mode\": \"pure-retroarch-3dsx\",\n");
    fprintf(f, "  \"retroarch_entry\": \"%s\",\n", RETRO_ENTRY_DEFAULT);
    fprintf(f, "  \"handoff_path\": \"sd:/3ds/emulators/launch.json\",\n");
    fprintf(f, "  \"rules\": [\n");
    for (int i = 0; i < g_default_rule_count; i++) {
        const DefaultRule* d = &g_default_rules[i];
        fprintf(f, "    {\"match\":{\"folder\":\"%s\",\"ext\":[", d->folder);
        for (int e = 0; e < d->ext_count; e++) {
            fprintf(f, "\"%s\"%s", d->exts[e], (e + 1 < d->ext_count) ? "," : "");
        }
        fprintf(f, "]},\"core\":\"%s\"}%s\n", d->core, (i + 1 < g_default_rule_count) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

static bool parse_rules_text(const char* text, RetroRules* out_rules) {
    if (!text || !out_rules) return false;
    RetroRules tmp;
    rules_set_defaults(&tmp);

    int ver = 0;
    if (parse_json_int(text, "version", &ver) && ver > 0) tmp.version = ver;
    parse_json_string(text, "mode", tmp.mode, sizeof(tmp.mode));
    parse_json_string(text, "retroarch_entry", tmp.retroarch_entry, sizeof(tmp.retroarch_entry));
    parse_json_string(text, "handoff_path", tmp.handoff_path, sizeof(tmp.handoff_path));

    tmp.rule_count = 0;
    const char* p = text;
    while (p && tmp.rule_count < MAX_RETRO_RULES) {
        const char* pfolder = strstr(p, "\"folder\"");
        if (!pfolder) break;
        const char* next_folder = strstr(pfolder + 8, "\"folder\"");
        const char* limit = next_folder ? next_folder : text + strlen(text);

        char folder[16] = {0};
        if (!parse_json_string(pfolder, "folder", folder, sizeof(folder))) {
            p = pfolder + 8;
            continue;
        }

        const char* pcore = strstr(pfolder, "\"core\"");
        if (!pcore || pcore >= limit) {
            p = pfolder + 8;
            continue;
        }
        char core[64] = {0};
        if (!parse_json_string(pcore, "core", core, sizeof(core))) {
            p = pfolder + 8;
            continue;
        }

        RetroRule* r = &tmp.rules[tmp.rule_count++];
        lower_copy(r->folder, sizeof(r->folder), folder);
        copy_str(r->core, sizeof(r->core), core);

        const char* pext = strstr(pfolder, "\"ext\"");
        if (pext && pext < limit) {
            r->ext_count = parse_ext_array(pext, limit, r->extensions);
        } else {
            r->ext_count = 0;
        }

        p = pfolder + 8;
    }

    if (tmp.rule_count == 0) return false;
    *out_rules = tmp;
    return true;
}

static void backup_external_rules(void) {
    u8* data = NULL;
    size_t size = 0;
    if (!read_file(RETRO_RULES_PATH, &data, &size) || !data || size == 0) {
        if (data) free(data);
        return;
    }
    ensure_dirs();
    FILE* f = fopen(RETRO_RULES_BAK_PATH, "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
    }
    free(data);
    remove(RETRO_RULES_PATH);
}

bool load_or_create_retro_rules(RetroRules* rules, bool* regenerated) {
    if (regenerated) *regenerated = false;
    if (!rules) return false;
    ensure_dirs();
    mkdir(EMU_EXT_DIR, 0777);

    if (!file_exists(RETRO_RULES_PATH)) {
        if (!write_default_rules_file()) {
            rules_set_defaults(rules);
            return false;
        }
        if (regenerated) *regenerated = true;
    }

    u8* data = NULL;
    size_t size = 0;
    if (!read_file(RETRO_RULES_PATH, &data, &size) || !data || size == 0) {
        rules_set_defaults(rules);
        return false;
    }

    char* text = (char*)malloc(size + 1);
    if (!text) {
        free(data);
        rules_set_defaults(rules);
        return false;
    }
    memcpy(text, data, size);
    text[size] = 0;
    free(data);

    if (!parse_rules_text(text, rules)) {
        free(text);
        backup_external_rules();
        if (write_default_rules_file()) {
            if (regenerated) *regenerated = true;
            rules_set_defaults(rules);
            return true;
        }
        rules_set_defaults(rules);
        return false;
    }

    free(text);
    return true;
}

static bool ext_matches_rule(const RetroRule* r, const char* ext_lower) {
    if (!r || !ext_lower || !ext_lower[0]) return false;
    for (int i = 0; i < r->ext_count; i++) {
        if (!strcasecmp(r->extensions[i], ext_lower)) return true;
    }
    return false;
}

static const DefaultRule* default_rule_for_system(const char* system_key) {
    if (!system_key || !system_key[0]) return NULL;
    for (int i = 0; i < g_default_rule_count; i++) {
        if (!strcasecmp(g_default_rules[i].folder, system_key)) return &g_default_rules[i];
    }
    return NULL;
}

static const char* fallback_core_for_ext(const char* ext_lower) {
    if (!ext_lower || !ext_lower[0]) return "Nestopia UE";
    if (!strcasecmp(ext_lower, "nes")) return "Nestopia UE";
    if (!strcasecmp(ext_lower, "smc") || !strcasecmp(ext_lower, "sfc")) return "Snes9x 2002";
    if (!strcasecmp(ext_lower, "gb") || !strcasecmp(ext_lower, "gbc")) return "Gambatte";
    if (!strcasecmp(ext_lower, "gg") || !strcasecmp(ext_lower, "sms") || !strcasecmp(ext_lower, "sg")) return "Genesis Plus GX";
    if (!strcasecmp(ext_lower, "gen") || !strcasecmp(ext_lower, "md") || !strcasecmp(ext_lower, "smd")) return "Genesis Plus GX";
    if (!strcasecmp(ext_lower, "a26")) return "Stella 2014";
    if (!strcasecmp(ext_lower, "a52")) return "Atari800";
    if (!strcasecmp(ext_lower, "a78")) return "ProSystem";
    if (!strcasecmp(ext_lower, "col")) return "blueMSX";
    if (!strcasecmp(ext_lower, "dsk") || !strcasecmp(ext_lower, "cdt") || !strcasecmp(ext_lower, "cpr")) return "Caprice32";
    if (!strcasecmp(ext_lower, "intv") || !strcasecmp(ext_lower, "rom")) return "FreeIntv";
    if (!strcasecmp(ext_lower, "ngp") || !strcasecmp(ext_lower, "ngc")) return "Beetle NeoPop";
    if (!strcasecmp(ext_lower, "min")) return "PokeMini";
    if (!strcasecmp(ext_lower, "pce")) return "Beetle PCE Fast";
    if (!strcasecmp(ext_lower, "ws") || !strcasecmp(ext_lower, "wsc")) return "Beetle Cygne";
    if (!strcasecmp(ext_lower, "bin")) return "Genesis Plus GX";
    return "Nestopia UE";
}

const char* retro_resolve_core(const RetroRules* rules, const char* system_key, const char* ext_lower, bool* matched_rule) {
    if (matched_rule) *matched_rule = false;
    if (rules && system_key && system_key[0] && ext_lower && ext_lower[0]) {
        for (int i = 0; i < rules->rule_count; i++) {
            const RetroRule* r = &rules->rules[i];
            if (strcasecmp(r->folder, system_key) != 0) continue;
            if (!ext_matches_rule(r, ext_lower)) continue;
            if (matched_rule) *matched_rule = true;
            return r->core;
        }
    }
    const DefaultRule* d = default_rule_for_system(system_key);
    if (d && d->core[0]) return d->core;
    return fallback_core_for_ext(ext_lower);
}

int retro_extensions_for_system(const RetroRules* rules, const char* system_key, char out_exts[MAX_EXTENSIONS][16]) {
    if (!out_exts) return 0;
    for (int i = 0; i < MAX_EXTENSIONS; i++) out_exts[i][0] = 0;
    if (!rules || !system_key || !system_key[0]) return 0;
    int count = 0;
    for (int i = 0; i < rules->rule_count && count < MAX_EXTENSIONS; i++) {
        const RetroRule* r = &rules->rules[i];
        if (strcasecmp(r->folder, system_key) != 0) continue;
        for (int e = 0; e < r->ext_count && count < MAX_EXTENSIONS; e++) {
            bool dup = false;
            for (int j = 0; j < count; j++) {
                if (!strcasecmp(out_exts[j], r->extensions[e])) { dup = true; break; }
            }
            if (dup) continue;
            copy_str(out_exts[count], sizeof(out_exts[count]), r->extensions[e]);
            count++;
        }
    }
    if (count > 0) return count;
    const DefaultRule* d = default_rule_for_system(system_key);
    if (!d) return 0;
    for (int e = 0; e < d->ext_count && count < MAX_EXTENSIONS; e++) {
        copy_str(out_exts[count], sizeof(out_exts[count]), d->exts[e]);
        count++;
    }
    return count;
}

static void sd_to_sdmc_path(const char* in, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!in || !in[0]) return;
    if (!strncasecmp(in, "sdmc:/", 6)) {
        copy_str(out, out_size, in);
        return;
    }
    if (!strncasecmp(in, "sd:/", 4)) {
        snprintf(out, out_size, "sdmc:/%s", in + 4);
        return;
    }
    if (in[0] == '/') {
        snprintf(out, out_size, "sdmc:%s", in);
        return;
    }
    snprintf(out, out_size, "sdmc:/%s", in);
}

static void sd_to_hbldr_path(const char* in, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!in || !in[0]) return;
    char tmp[512];
    copy_str(tmp, sizeof(tmp), in);
    normalize_path_sd(tmp, sizeof(tmp));
    if (!strncasecmp(tmp, "sdmc:/", 6)) {
        snprintf(out, out_size, "%s", tmp + 5);
        return;
    }
    if (!strncasecmp(tmp, "sd:/", 4)) {
        snprintf(out, out_size, "/%s", tmp + 4);
        return;
    }
    if (tmp[0] == '/') {
        copy_str(out, out_size, tmp);
        return;
    }
    snprintf(out, out_size, "/%s", tmp);
}

bool retro_retroarch_exists(const RetroRules* rules) {
    char path[512];
    const char* entry = (rules && rules->retroarch_entry[0]) ? rules->retroarch_entry : RETRO_ENTRY_DEFAULT;
    sd_to_sdmc_path(entry, path, sizeof(path));
    return file_exists(path);
}

bool retro_chainload_available(void) {
    Handle hbldr = 0;
    Result rc = svcConnectToPort(&hbldr, "hb:ldr");
    if (R_SUCCEEDED(rc)) svcCloseHandle(hbldr);
    return R_SUCCEEDED(rc);
}

static Result hbldr_set_target(Handle hbldr, const char* path) {
    if (!path || !path[0]) return -1;
    u32 path_len = (u32)strlen(path) + 1;
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(2, 0, 2);
    cmdbuf[1] = IPC_Desc_StaticBuffer(path_len, 0);
    cmdbuf[2] = (u32)path;
    Result rc = svcSendSyncRequest(hbldr);
    if (R_SUCCEEDED(rc)) rc = (Result)cmdbuf[1];
    return rc;
}

typedef struct {
    char* dst;
    u32 buf[0x400 / sizeof(u32)];
} argData_s;

static void hbldr_args_init(argData_s* args) {
    memset(args, 0, sizeof(*args));
    args->dst = (char*)&args->buf[1];
}

static bool hbldr_args_add(argData_s* args, const char* arg) {
    if (!args || !arg || !arg[0]) return false;
    size_t len = strlen(arg) + 1;
    char* end = (char*)args->buf + sizeof(args->buf);
    if (args->dst + len > end) return false;
    args->buf[0]++;
    memcpy(args->dst, arg, len);
    args->dst += len;
    return true;
}

static Result hbldr_set_argv(Handle hbldr, const char* arg0) {
    static argData_s args;
    hbldr_args_init(&args);
    hbldr_args_add(&args, arg0);
    retro_log_line("chainload: argv argc=%lu arg0=%s", (unsigned long)args.buf[0], arg0 ? arg0 : "");
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(3, 0, 2);
    cmdbuf[1] = IPC_Desc_StaticBuffer(sizeof(args.buf), 1);
    cmdbuf[2] = (u32)args.buf;
    Result rc = svcSendSyncRequest(hbldr);
    if (R_SUCCEEDED(rc)) rc = (Result)cmdbuf[1];
    return rc;
}

bool retro_chainload(const char* retroarch_sd_path, char* status_message, size_t status_size) {
    if (status_message && status_size > 0) status_message[0] = 0;
    char target[512];
    sd_to_hbldr_path(retroarch_sd_path, target, sizeof(target));
    Handle hbldr = 0;
    Result rc = svcConnectToPort(&hbldr, "hb:ldr");
    retro_log_line("chainload: connect rc=%08lX", (unsigned long)rc);
    if (R_FAILED(rc)) {
        if (status_message && status_size > 0) snprintf(status_message, status_size, "hb:ldr not available");
        return false;
    }
    rc = hbldr_set_target(hbldr, target);
    retro_log_line("chainload: target rc=%08lX path=%s", (unsigned long)rc, target);
    if (R_FAILED(rc)) {
        svcCloseHandle(hbldr);
        if (status_message && status_size > 0) snprintf(status_message, status_size, "hb:ldr target failed");
        return false;
    }
    rc = hbldr_set_argv(hbldr, target);
    svcCloseHandle(hbldr);
    retro_log_line("chainload: argv rc=%08lX", (unsigned long)rc);
    if (R_FAILED(rc)) {
        if (status_message && status_size > 0) snprintf(status_message, status_size, "hb:ldr argv failed");
        return false;
    }
    return true;
}

static void format_timestamp(char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    time_t now = time(NULL);
    struct tm tmv;
    gmtime_r(&now, &tmv);
    strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

static bool write_handoff_file(const char* handoff_sd_path, const char* rom_sd_path, const char* core, const char* ts, char* status_message, size_t status_size, bool is_primary) {
    if (!handoff_sd_path || !handoff_sd_path[0]) return false;
    char handoff_sd_norm[512];
    copy_str(handoff_sd_norm, sizeof(handoff_sd_norm), handoff_sd_path);
    normalize_path_sd(handoff_sd_norm, sizeof(handoff_sd_norm));

    char handoff_sdmc[512];
    sd_to_sdmc_path(handoff_sd_norm, handoff_sdmc, sizeof(handoff_sdmc));

    char handoff_dir[512];
    copy_str(handoff_dir, sizeof(handoff_dir), handoff_sdmc);
    char* slash = strrchr(handoff_dir, '/');
    if (slash && slash > handoff_dir) {
        *slash = 0;
        mkdir(handoff_dir, 0777);
    }

    char handoff_tmp[544];
    snprintf(handoff_tmp, sizeof(handoff_tmp), "%s.tmp", handoff_sdmc);

    FILE* f = fopen(handoff_tmp, "w");
    if (!f) {
        if (is_primary && status_message && status_size > 0) snprintf(status_message, status_size, "launch.json write failed");
        retro_log_line("handoff write failed: %s", handoff_sdmc);
        return false;
    }
    fprintf(f, "{\n");
    fprintf(f, "  \"rom\": \"%s\",\n", rom_sd_path);
    fprintf(f, "  \"core\": \"%s\",\n", core);
    fprintf(f, "  \"timestamp\": \"%s\"\n", ts);
    fprintf(f, "}\n");
    fclose(f);
    remove(handoff_sdmc);
    if (rename(handoff_tmp, handoff_sdmc) != 0) {
        remove(handoff_tmp);
        if (is_primary && status_message && status_size > 0) snprintf(status_message, status_size, "launch.json rename failed");
        retro_log_line("handoff rename failed: %s", handoff_sdmc);
        return false;
    }
    retro_log_line("handoff written: %s", handoff_sdmc);
    return true;
}

bool retro_write_launch(const RetroRules* rules, const char* rom_sd_path, const char* core, char* status_message, size_t status_size) {
    if (status_message && status_size > 0) status_message[0] = 0;
    if (!rom_sd_path || !rom_sd_path[0] || !core || !core[0]) {
        if (status_message && status_size > 0) snprintf(status_message, status_size, "Invalid launch data");
        return false;
    }
    ensure_dirs();
    mkdir(EMU_EXT_DIR, 0777);

    const char* primary_handoff = (rules && rules->handoff_path[0]) ? rules->handoff_path : RETRO_LAUNCH_PATH;
    char ts[32];
    format_timestamp(ts, sizeof(ts));

    if (!write_handoff_file(primary_handoff, rom_sd_path, core, ts, status_message, status_size, true)) {
        return false;
    }
    return true;
}

static DIR* open_retro_core_dir(const char** out_path) {
    if (out_path) *out_path = NULL;
    for (int i = 0; i < g_retro_core_dir_count; i++) {
        DIR* d = opendir(g_retro_core_dirs[i]);
        if (d) {
            if (out_path) *out_path = g_retro_core_dirs[i];
            return d;
        }
    }
    return NULL;
}

bool retro_core_available(const char* core_label, bool* known, bool* available) {
    if (known) *known = false;
    if (available) *available = false;
    if (!core_label || !core_label[0]) return false;
    const char* core_dir = NULL;
    DIR* dir = open_retro_core_dir(&core_dir);
    if (!dir) return false;
    const CoreMap* map = NULL;
    for (int i = 0; i < g_core_map_count; i++) {
        if (!strcasecmp(g_core_map[i].label, core_label)) {
            map = &g_core_map[i];
            break;
        }
    }
    if (!map) {
        closedir(dir);
        return false;
    }
    if (known) *known = true;
    char want[6][128];
    for (int i = 0; i < map->count && i < 6; i++) {
        sanitize_key(want[i], sizeof(want[i]), map->candidates[i]);
    }
    bool found = false;
    struct dirent* ent;
    while ((ent = readdir(dir))) {
        const char* dot = strrchr(ent->d_name, '.');
        if (!dot) continue;
        if (strcasecmp(dot, ".so") != 0 && strcasecmp(dot, ".3dsx") != 0) continue;
        char base[256];
        base_name_no_ext(ent->d_name, base, sizeof(base));
        char have[256];
        sanitize_key(have, sizeof(have), base);
        for (int i = 0; i < map->count && i < 6; i++) {
            if (want[i][0] && have[0] && strstr(have, want[i])) {
                found = true;
                break;
            }
        }
        if (found) break;
    }
    closedir(dir);
    if (available) *available = found;
    if (core_dir && g_retro_log_enabled) {
        retro_log_line("core scan dir: %s", core_dir);
    }
    return found;
}

void retro_log_set_enabled(bool on) {
    g_retro_log_enabled = on;
    if (on) retro_log_reset();
}

bool retro_log_is_enabled(void) {
    return g_retro_log_enabled;
}

void retro_log_reset(void) {
    if (!g_retro_log_enabled) return;
    ensure_dirs();
    mkdir(EMU_EXT_DIR, 0777);
    FILE* f = fopen(RETRO_LOG_PATH, "w");
    if (f) fclose(f);
    g_retro_log_touched = true;
}

void retro_log_line(const char* fmt, ...) {
    if (!g_retro_log_enabled) return;
    ensure_dirs();
    mkdir(EMU_EXT_DIR, 0777);
    FILE* f = fopen(RETRO_LOG_PATH, g_retro_log_touched ? "a" : "w");
    if (!f) return;
    g_retro_log_touched = true;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(f, "%s\n", buf);
    fclose(f);
}
