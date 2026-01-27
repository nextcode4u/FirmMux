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
    state->background_visibility = 25;
    if (!json_find_string(text, "last_target", state->last_target, sizeof(state->last_target))) {
        state->last_target[0] = 0;
    }
    if (!json_find_string(text, "theme", state->theme, sizeof(state->theme))) {
        state->theme[0] = 0;
    }
    if (!json_find_string(text, "top_background", state->top_background, sizeof(state->top_background))) {
        state->top_background[0] = 0;
    }
    if (!json_find_string(text, "bottom_background", state->bottom_background, sizeof(state->bottom_background))) {
        state->bottom_background[0] = 0;
    }
    json_find_int(text, "background_visibility", &state->background_visibility);
    if (state->background_visibility < 0) state->background_visibility = 0;
    if (state->background_visibility > 100) state->background_visibility = 100;
    const char* p = strstr(text, "\"targets\":");
    if (!p) return true;
    p = strchr(p, '[');
    if (!p) return true;
    p++;
    while (*p && state->count < MAX_TARGETS) {
        if (*p == '{') {
            const char* end = strchr(p, '}');
            if (!end) break;
            char block[512];
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
            json_find_string(block, "card_launcher_title_id", ts->card_launcher_title_id, sizeof(ts->card_launcher_title_id));
            json_find_string(block, "card_launcher_media", ts->card_launcher_media, sizeof(ts->card_launcher_media));
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
        state->background_visibility = 25;
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
    state->background_visibility = 25;
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
    char theme_esc[64];
    char top_bg_esc[96];
    char bottom_bg_esc[96];
    json_escape(state->last_target, last_esc, sizeof(last_esc));
    json_escape(state->theme, theme_esc, sizeof(theme_esc));
    json_escape(state->top_background, top_bg_esc, sizeof(top_bg_esc));
    json_escape(state->bottom_background, bottom_bg_esc, sizeof(bottom_bg_esc));
    fprintf(f, "{\n");
    fprintf(f, "  \"last_target\":\"%s\",\n", last_esc);
    fprintf(f, "  \"theme\":\"%s\",\n", theme_esc);
    fprintf(f, "  \"top_background\":\"%s\",\n", top_bg_esc);
    fprintf(f, "  \"bottom_background\":\"%s\",\n", bottom_bg_esc);
    fprintf(f, "  \"background_visibility\":%d,\n", state->background_visibility);
    fprintf(f, "  \"targets\":[\n");
    for (int i = 0; i < state->count; i++) {
        const TargetState* ts = &state->entries[i];
        char id_esc[64];
        char path_esc[512];
        char loader_id_esc[64];
        char loader_media_esc[32];
        char card_id_esc[64];
        char card_media_esc[32];
        json_escape(ts->id, id_esc, sizeof(id_esc));
        json_escape(ts->path, path_esc, sizeof(path_esc));
        json_escape(ts->loader_title_id, loader_id_esc, sizeof(loader_id_esc));
        json_escape(ts->loader_media, loader_media_esc, sizeof(loader_media_esc));
        json_escape(ts->card_launcher_title_id, card_id_esc, sizeof(card_id_esc));
        json_escape(ts->card_launcher_media, card_media_esc, sizeof(card_media_esc));
        fprintf(f, "    {\"id\":\"%s\",\"path\":\"%s\",\"selection\":%d,\"scroll\":%d,\"nds_banner_mode\":%d,\"loader_title_id\":\"%s\",\"loader_media\":\"%s\",\"card_launcher_title_id\":\"%s\",\"card_launcher_media\":\"%s\"}%s\n",
            id_esc, path_esc, ts->selection, ts->scroll, ts->nds_banner_mode, loader_id_esc, loader_media_esc, card_id_esc, card_media_esc, (i + 1 == state->count) ? "" : ",");
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
