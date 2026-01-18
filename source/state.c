#include "fmux.h"
#include <string.h>
#include <stdlib.h>

static bool json_find_string(const char* text, const char* key, char* out, size_t out_size) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* p = strstr(text, pattern);
    if (!p) return false;
    p += strlen(pattern);
    const char* end = strchr(p, '\"');
    if (!end) return false;
    size_t len = (size_t)(end - p);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, p, len);
    out[len] = 0;
    return true;
}

static bool json_find_int(const char* text, const char* key, int* out) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* p = strstr(text, pattern);
    if (!p) return false;
    p += strlen(pattern);
    *out = atoi(p);
    return true;
}

bool parse_state(const char* text, State* state) {
    memset(state, 0, sizeof(*state));
    if (!json_find_string(text, "last_target", state->last_target, sizeof(state->last_target))) {
        state->last_target[0] = 0;
    }
    const char* p = strstr(text, "\"targets\":");
    if (!p) return true;
    p = strchr(p, '[');
    if (!p) return true;
    p++;
    while (*p && state->count < MAX_TARGETS) {
        if (*p == '{') {
            const char* end = strchr(p, '}');
            if (!end) break;
            char block[256];
            size_t len = (size_t)(end - p + 1);
            if (len >= sizeof(block)) len = sizeof(block) - 1;
            memcpy(block, p, len);
            block[len] = 0;
            TargetState* ts = &state->entries[state->count++];
            json_find_string(block, "id", ts->id, sizeof(ts->id));
            json_find_string(block, "path", ts->path, sizeof(ts->path));
            json_find_int(block, "selection", &ts->selection);
            json_find_int(block, "scroll", &ts->scroll);
            json_find_int(block, "nds_banner_mode", &ts->nds_banner_mode);
            json_find_string(block, "loader_title_id", ts->loader_title_id, sizeof(ts->loader_title_id));
            json_find_string(block, "loader_media", ts->loader_media, sizeof(ts->loader_media));
            p = end + 1;
        } else {
            p++;
        }
    }
    return true;
}

bool load_state(State* state) {
    ensure_dirs();
    if (!file_exists(STATE_PATH) && file_exists(STATE_PATH_OLD)) {
        rename(STATE_PATH_OLD, STATE_PATH);
    }
    if (!file_exists(STATE_PATH)) {
        memset(state, 0, sizeof(*state));
        return true;
    }
    FILE* f = fopen(STATE_PATH, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = (char*)malloc(size + 1);
    if (!data) { fclose(f); return false; }
    fread(data, 1, size, f);
    data[size] = 0;
    fclose(f);
    bool ok = parse_state(data, state);
    free(data);
    if (ok) return true;
    rename(STATE_PATH, STATE_BAK_PATH);
    if (file_exists(STATE_PATH_OLD)) rename(STATE_PATH_OLD, STATE_BAK_PATH_OLD);
    memset(state, 0, sizeof(*state));
    return true;
}

void json_escape(const char* in, char* out, size_t out_size) {
    size_t oi = 0;
    for (size_t i = 0; in[i] && oi + 2 < out_size; i++) {
        if (in[i] == '\"' || in[i] == '\\') {
            out[oi++] = '\\';
            out[oi++] = in[i];
        } else {
            out[oi++] = in[i];
        }
    }
    out[oi] = 0;
}

bool save_state(const State* state) {
    FILE* f = fopen(STATE_PATH, "w");
    if (!f) return false;
    char last_esc[64];
    json_escape(state->last_target, last_esc, sizeof(last_esc));
    fprintf(f, "{\n");
    fprintf(f, "  \"last_target\":\"%s\",\n", last_esc);
    fprintf(f, "  \"targets\":[\n");
    for (int i = 0; i < state->count; i++) {
        const TargetState* ts = &state->entries[i];
        char id_esc[64];
        char path_esc[512];
        char loader_id_esc[64];
        char loader_media_esc[32];
        json_escape(ts->id, id_esc, sizeof(id_esc));
        json_escape(ts->path, path_esc, sizeof(path_esc));
        json_escape(ts->loader_title_id, loader_id_esc, sizeof(loader_id_esc));
        json_escape(ts->loader_media, loader_media_esc, sizeof(loader_media_esc));
        fprintf(f, "    {\"id\":\"%s\",\"path\":\"%s\",\"selection\":%d,\"scroll\":%d,\"nds_banner_mode\":%d,\"loader_title_id\":\"%s\",\"loader_media\":\"%s\"}%s\n",
            id_esc, path_esc, ts->selection, ts->scroll, ts->nds_banner_mode, loader_id_esc, loader_media_esc, (i + 1 == state->count) ? "" : ",");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

TargetState* get_target_state(State* state, const char* id) {
    for (int i = 0; i < state->count; i++) {
        if (!strcmp(state->entries[i].id, id)) return &state->entries[i];
    }
    if (state->count >= MAX_TARGETS) return NULL;
    TargetState* ts = &state->entries[state->count++];
    memset(ts, 0, sizeof(*ts));
    snprintf(ts->id, sizeof(ts->id), "%s", id);
    ts->nds_banner_mode = 0;
    return ts;
}
