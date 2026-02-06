#include "fmux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

typedef struct {
    char rom[512];
    RetroRomOptions opt;
} RetroRomEntry;

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

static bool parse_json_int_limited(const char* start, const char* limit, const char* key, int* out) {
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
    char buf[32];
    size_t w = 0;
    while (p < limit && *p && (isdigit((unsigned char)*p) || *p == '-') && w + 1 < sizeof(buf)) buf[w++] = *p++;
    buf[w] = 0;
    if (w == 0) return false;
    *out = atoi(buf);
    return true;
}

static bool parse_json_bool_limited(const char* start, const char* limit, const char* key, int* out) {
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
    if (!strncmp(p, "true", 4)) { *out = 1; return true; }
    if (!strncmp(p, "false", 5)) { *out = 0; return true; }
    if (*p == '0' || *p == '1') { *out = (*p == '1'); return true; }
    return false;
}

static bool parse_json_float_limited(const char* start, const char* limit, const char* key, float* out) {
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
    char buf[64];
    size_t w = 0;
    while (p < limit && *p && (isdigit((unsigned char)*p) || *p == '-' || *p == '.') && w + 1 < sizeof(buf)) buf[w++] = *p++;
    buf[w] = 0;
    if (w == 0) return false;
    *out = (float)atof(buf);
    return true;
}

void retro_rom_options_default(RetroRomOptions* opt) {
    if (!opt) return;
    memset(opt, 0, sizeof(*opt));
    opt->cpu_profile = -1;
    opt->frameskip = -1;
    opt->vsync = -1;
    opt->audio_latency = -1;
    opt->threaded_video = -1;
    opt->hard_gpu_sync = -1;
    opt->integer_scale = -1;
    opt->aspect_ratio = -1;
    opt->aspect_ratio_value = 0.0f;
    opt->bilinear = -1;
    opt->runahead = -1;
    opt->rewind = -1;
    opt->core_override[0] = 0;
    opt->video_filter[0] = 0;
    opt->audio_filter[0] = 0;
}

static bool parse_entry(const char* start, const char* limit, RetroRomEntry* out) {
    if (!start || !limit || !out || start >= limit) return false;
    RetroRomEntry tmp;
    memset(&tmp, 0, sizeof(tmp));
    retro_rom_options_default(&tmp.opt);

    if (!parse_json_string_limited(start, limit, "rom", tmp.rom, sizeof(tmp.rom))) return false;
    parse_json_string_limited(start, limit, "core_override", tmp.opt.core_override, sizeof(tmp.opt.core_override));
    parse_json_string_limited(start, limit, "video_filter", tmp.opt.video_filter, sizeof(tmp.opt.video_filter));
    parse_json_string_limited(start, limit, "audio_filter", tmp.opt.audio_filter, sizeof(tmp.opt.audio_filter));
    if (!tmp.opt.video_filter[0]) {
        parse_json_string_limited(start, limit, "shader", tmp.opt.video_filter, sizeof(tmp.opt.video_filter));
    }

    char prof[32] = {0};
    if (parse_json_string_limited(start, limit, "cpu_profile", prof, sizeof(prof))) {
        char low[32];
        lower_copy(low, sizeof(low), prof);
        if (!strcmp(low, "performance")) tmp.opt.cpu_profile = 1;
        else if (!strcmp(low, "normal")) tmp.opt.cpu_profile = 0;
    }

    parse_json_int_limited(start, limit, "frameskip", &tmp.opt.frameskip);
    parse_json_int_limited(start, limit, "audio_latency", &tmp.opt.audio_latency);
    parse_json_int_limited(start, limit, "aspect_ratio", &tmp.opt.aspect_ratio);
    parse_json_float_limited(start, limit, "aspect_ratio_value", &tmp.opt.aspect_ratio_value);
    parse_json_bool_limited(start, limit, "vsync", &tmp.opt.vsync);
    parse_json_bool_limited(start, limit, "threaded_video", &tmp.opt.threaded_video);
    parse_json_bool_limited(start, limit, "hard_gpu_sync", &tmp.opt.hard_gpu_sync);
    parse_json_bool_limited(start, limit, "integer_scale", &tmp.opt.integer_scale);
    parse_json_bool_limited(start, limit, "bilinear", &tmp.opt.bilinear);
    parse_json_bool_limited(start, limit, "runahead", &tmp.opt.runahead);
    parse_json_bool_limited(start, limit, "rewind", &tmp.opt.rewind);

    *out = tmp;
    return true;
}

static int parse_entries(const char* text, RetroRomEntry* out, int max_entries) {
    if (!text || !out || max_entries <= 0) return 0;
    int count = 0;
    const char* p = text;
    while (p && count < max_entries) {
        const char* rom = strstr(p, "\"rom\"");
        if (!rom) break;
        const char* next = strstr(rom + 5, "\"rom\"");
        const char* limit = next ? next : text + strlen(text);
        RetroRomEntry e;
        if (parse_entry(rom, limit, &e)) {
            out[count++] = e;
        }
        p = rom + 5;
    }
    return count;
}

static bool write_rom_options_file(const RetroRomEntry* entries, int count) {
    ensure_dirs();
    mkdir(EMU_EXT_DIR, 0777);
    FILE* f = fopen(RETRO_ROM_OPTIONS_TMP_PATH, "w");
    if (!f) return false;
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"entries\": [\n");
    for (int i = 0; i < count; i++) {
        const RetroRomEntry* e = &entries[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"rom\": \"%s\",\n", e->rom);
        if (e->opt.core_override[0]) fprintf(f, "      \"core_override\": \"%s\",\n", e->opt.core_override);
        if (e->opt.cpu_profile >= 0) fprintf(f, "      \"cpu_profile\": \"%s\",\n", e->opt.cpu_profile ? "performance" : "normal");
        if (e->opt.frameskip >= 0) fprintf(f, "      \"frameskip\": %d,\n", e->opt.frameskip);
        if (e->opt.vsync >= 0) fprintf(f, "      \"vsync\": %s,\n", e->opt.vsync ? "true" : "false");
        if (e->opt.audio_latency >= 0) fprintf(f, "      \"audio_latency\": %d,\n", e->opt.audio_latency);
        if (e->opt.threaded_video >= 0) fprintf(f, "      \"threaded_video\": %s,\n", e->opt.threaded_video ? "true" : "false");
        if (e->opt.hard_gpu_sync >= 0) fprintf(f, "      \"hard_gpu_sync\": %s,\n", e->opt.hard_gpu_sync ? "true" : "false");
        if (e->opt.integer_scale >= 0) fprintf(f, "      \"integer_scale\": %s,\n", e->opt.integer_scale ? "true" : "false");
        if (e->opt.aspect_ratio >= 0) fprintf(f, "      \"aspect_ratio\": %d,\n", e->opt.aspect_ratio);
        if (e->opt.aspect_ratio_value > 0.0f) fprintf(f, "      \"aspect_ratio_value\": %.3f,\n", e->opt.aspect_ratio_value);
        if (e->opt.bilinear >= 0) fprintf(f, "      \"bilinear\": %s,\n", e->opt.bilinear ? "true" : "false");
        if (e->opt.video_filter[0]) fprintf(f, "      \"video_filter\": \"%s\",\n", e->opt.video_filter);
        if (e->opt.audio_filter[0]) fprintf(f, "      \"audio_filter\": \"%s\",\n", e->opt.audio_filter);
        if (e->opt.runahead >= 0) fprintf(f, "      \"runahead\": %s,\n", e->opt.runahead ? "true" : "false");
        if (e->opt.rewind >= 0) fprintf(f, "      \"rewind\": %s,\n", e->opt.rewind ? "true" : "false");
        fprintf(f, "      \"_\": 0\n");
        fprintf(f, "    }%s\n", (i + 1 < count) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    remove(RETRO_ROM_OPTIONS_PATH);
    return rename(RETRO_ROM_OPTIONS_TMP_PATH, RETRO_ROM_OPTIONS_PATH) == 0;
}

static void normalize_sd_path(const char* in, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!in || !in[0]) return;
    char tmp[512];
    copy_str(tmp, sizeof(tmp), in);
    normalize_path_sd(tmp, sizeof(tmp));
    copy_str(out, out_size, tmp);
}

bool retro_rom_options_load(const char* rom_sd_path, RetroRomOptions* out) {
    if (!rom_sd_path || !out) return false;
    retro_rom_options_default(out);
    u8* data = NULL;
    size_t size = 0;
    if (!read_file(RETRO_ROM_OPTIONS_PATH, &data, &size) || !data || size == 0) {
        if (data) free(data);
        return false;
    }
    const int max_entries = 128;
    RetroRomEntry* entries = (RetroRomEntry*)calloc((size_t)max_entries, sizeof(RetroRomEntry));
    if (!entries) {
        free(data);
        return false;
    }
    int count = parse_entries((const char*)data, entries, max_entries);
    free(data);
    if (count <= 0) {
        free(entries);
        return false;
    }
    char want[512];
    normalize_sd_path(rom_sd_path, want, sizeof(want));
    for (int i = 0; i < count; i++) {
        char have[512];
        normalize_sd_path(entries[i].rom, have, sizeof(have));
        if (!strcmp(have, want)) {
            *out = entries[i].opt;
            free(entries);
            return true;
        }
    }
    free(entries);
    return false;
}

bool retro_rom_options_save(const char* rom_sd_path, const RetroRomOptions* opt) {
    if (!rom_sd_path || !opt) return false;
    const int max_entries = 128;
    RetroRomEntry* entries = (RetroRomEntry*)calloc((size_t)max_entries, sizeof(RetroRomEntry));
    if (!entries) return false;
    int count = 0;
    u8* data = NULL;
    size_t size = 0;
    if (read_file(RETRO_ROM_OPTIONS_PATH, &data, &size) && data && size > 0) {
        count = parse_entries((const char*)data, entries, max_entries);
        free(data);
    }
    char norm[512];
    normalize_sd_path(rom_sd_path, norm, sizeof(norm));
    bool replaced = false;
    for (int i = 0; i < count; i++) {
        char have[512];
        normalize_sd_path(entries[i].rom, have, sizeof(have));
        if (!strcmp(have, norm)) {
            copy_str(entries[i].rom, sizeof(entries[i].rom), norm);
            entries[i].opt = *opt;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        if (count >= max_entries) {
            free(entries);
            return false;
        }
        copy_str(entries[count].rom, sizeof(entries[count].rom), norm);
        entries[count].opt = *opt;
        count++;
    }
    bool ok = write_rom_options_file(entries, count);
    free(entries);
    return ok;
}

static bool is_dir_path(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static void add_filter_item(char out_list[][192], int* count, int max_items, const char* path) {
    if (!out_list || !count || !path || !path[0]) return;
    if (*count >= max_items) return;
    for (int i = 0; i < *count; i++) {
        if (!strcmp(out_list[i], path)) return;
    }
    copy_str(out_list[*count], sizeof(out_list[*count]), path);
    (*count)++;
}

int retro_shader_favorites_load(char out_list[][192], int max_items) {
    if (!out_list || max_items <= 0) return 0;
    for (int i = 0; i < max_items; i++) out_list[i][0] = 0;
    int count = 0;
    FILE* f = fopen(RETRO_FILTER_FAV_PATH, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) && count < max_items) {
            trim(line);
            if (!line[0]) continue;
            add_filter_item(out_list, &count, max_items, line);
        }
        fclose(f);
    }
    if (count > 0) return count;

    DIR* d = opendir(RETRO_VIDEO_FILTERS_DIR);
    if (!d) return 0;
    struct dirent* ent;
    while ((ent = readdir(d)) && count < max_items) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", RETRO_FILTERS_DIR, ent->d_name);
        if (is_dir_path(full)) continue;
        add_filter_item(out_list, &count, max_items, full);
    }
    closedir(d);
    return count;
}
