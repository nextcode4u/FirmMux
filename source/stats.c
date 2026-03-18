#include "fmux.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define STATS_MAX_BYTES (1024 * 1024)

static void stats_set_defaults(StatsData* stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
}

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

static bool parse_stats(const char* text, StatsData* stats) {
    if (!text || !stats) return false;
    stats_set_defaults(stats);
    const char* lp = strstr(text, "\"last_played\":");
    if (lp) {
        const char* start = strchr(lp, '{');
        const char* end = start ? strchr(start, '}') : NULL;
        if (start && end) {
            char block[768];
            size_t len = (size_t)(end - start + 1);
            if (len >= sizeof(block)) len = sizeof(block) - 1;
            memcpy(block, start, len);
            block[len] = 0;
            if (json_find_int(block, "kind", &stats->last_played.kind)
                && json_find_string(block, "key", stats->last_played.key, sizeof(stats->last_played.key))
                && json_find_string(block, "label", stats->last_played.label, sizeof(stats->last_played.label))) {
                stats->has_last_played = true;
            }
        }
    }

    const char* p = strstr(text, "\"favorites\":");
    if (!p) return true;
    p = strchr(p, '[');
    if (!p) return true;
    p++;
    while (*p && stats->favorite_count < STATS_MAX_FAVORITES) {
        if (*p == '{') {
            const char* end = strchr(p, '}');
            if (!end) break;
            char block[768];
            size_t len = (size_t)(end - p + 1);
            if (len >= sizeof(block)) len = sizeof(block) - 1;
            memcpy(block, p, len);
            block[len] = 0;
            StatsEntry* fav = &stats->favorites[stats->favorite_count];
            if (json_find_int(block, "kind", &fav->kind)
                && json_find_string(block, "key", fav->key, sizeof(fav->key))
                && json_find_string(block, "label", fav->label, sizeof(fav->label))) {
                stats->favorite_count++;
            }
            p = end + 1;
        } else {
            p++;
        }
    }
    return true;
}

static void json_escape(const char* in, char* out, size_t out_size) {
    size_t oi = 0;
    for (size_t i = 0; in && in[i] && oi + 2 < out_size; i++) {
        if (in[i] == '\"' || in[i] == '\\') {
            out[oi++] = '\\';
            out[oi++] = in[i];
        } else {
            out[oi++] = in[i];
        }
    }
    out[oi] = 0;
}

bool load_stats_data(StatsData* stats) {
    if (!stats) return false;
    ensure_dirs();
    if (!file_exists(STATS_PATH)) {
        stats_set_defaults(stats);
        return true;
    }
    FILE* f = fopen(STATS_PATH, "rb");
    if (!f) {
        stats_set_defaults(stats);
        return true;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        stats_set_defaults(stats);
        return true;
    }
    long size = ftell(f);
    if (size < 0 || size > STATS_MAX_BYTES) {
        fclose(f);
        stats_set_defaults(stats);
        return true;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        stats_set_defaults(stats);
        return true;
    }
    char* data = (char*)malloc((size_t)size + 1);
    if (!data) {
        fclose(f);
        stats_set_defaults(stats);
        return true;
    }
    size_t got = 0;
    if (size > 0) {
        got = fread(data, 1, (size_t)size, f);
        if (got != (size_t)size) {
            free(data);
            fclose(f);
            stats_set_defaults(stats);
            return true;
        }
    }
    data[size] = 0;
    fclose(f);
    bool ok = parse_stats(data, stats);
    free(data);
    if (!ok) stats_set_defaults(stats);
    return true;
}

bool save_stats_data(const StatsData* stats) {
    if (!stats) return false;
    ensure_dirs();
    FILE* f = fopen(STATS_TMP_PATH, "w");
    if (!f) return false;
    fprintf(f, "{\n");
    if (stats->has_last_played) {
        char key_esc[STATS_KEY_SIZE * 2];
        char label_esc[STATS_LABEL_SIZE * 2];
        json_escape(stats->last_played.key, key_esc, sizeof(key_esc));
        json_escape(stats->last_played.label, label_esc, sizeof(label_esc));
        fprintf(f, "  \"last_played\":{\"kind\":%d,\"key\":\"%s\",\"label\":\"%s\"},\n",
            stats->last_played.kind, key_esc, label_esc);
    } else {
        fprintf(f, "  \"last_played\":null,\n");
    }
    fprintf(f, "  \"favorites\":[\n");
    for (int i = 0; i < stats->favorite_count; i++) {
        char key_esc[STATS_KEY_SIZE * 2];
        char label_esc[STATS_LABEL_SIZE * 2];
        json_escape(stats->favorites[i].key, key_esc, sizeof(key_esc));
        json_escape(stats->favorites[i].label, label_esc, sizeof(label_esc));
        fprintf(f, "    {\"kind\":%d,\"key\":\"%s\",\"label\":\"%s\"}%s\n",
            stats->favorites[i].kind, key_esc, label_esc, (i + 1 == stats->favorite_count) ? "" : ",");
    }
    fprintf(f, "  ]\n}\n");
    fflush(f);
    fclose(f);
    if (file_exists(STATS_BAK_PATH)) remove(STATS_BAK_PATH);
    if (file_exists(STATS_PATH)) rename(STATS_PATH, STATS_BAK_PATH);
    if (rename(STATS_TMP_PATH, STATS_PATH) != 0) {
        if (file_exists(STATS_BAK_PATH) && !file_exists(STATS_PATH)) rename(STATS_BAK_PATH, STATS_PATH);
        remove(STATS_TMP_PATH);
        return false;
    }
    return true;
}

int stats_favorite_count(const StatsData* stats) {
    return stats ? stats->favorite_count : 0;
}

const StatsEntry* stats_get_favorite(const StatsData* stats, int idx) {
    if (!stats || idx < 0 || idx >= stats->favorite_count) return NULL;
    return &stats->favorites[idx];
}

const StatsEntry* stats_get_last_played(const StatsData* stats) {
    if (!stats || !stats->has_last_played) return NULL;
    return &stats->last_played;
}

int stats_find_favorite(const StatsData* stats, int kind, const char* key) {
    if (!stats || !key || !key[0]) return -1;
    for (int i = 0; i < stats->favorite_count; i++) {
        if (stats->favorites[i].kind == kind && !strcmp(stats->favorites[i].key, key)) return i;
    }
    return -1;
}

bool stats_is_favorite(const StatsData* stats, int kind, const char* key) {
    return stats_find_favorite(stats, kind, key) >= 0;
}

bool stats_add_favorite(StatsData* stats, int kind, const char* key, const char* label) {
    if (!stats || !key || !key[0] || !label || !label[0]) return false;
    if (stats_find_favorite(stats, kind, key) >= 0) return true;
    if (stats->favorite_count >= STATS_MAX_FAVORITES) return false;
    StatsEntry* fav = &stats->favorites[stats->favorite_count++];
    memset(fav, 0, sizeof(*fav));
    fav->kind = kind;
    copy_str(fav->key, sizeof(fav->key), key);
    copy_str(fav->label, sizeof(fav->label), label);
    return true;
}

bool stats_remove_favorite(StatsData* stats, int idx) {
    if (!stats || idx < 0 || idx >= stats->favorite_count) return false;
    for (int i = idx; i + 1 < stats->favorite_count; i++) stats->favorites[i] = stats->favorites[i + 1];
    memset(&stats->favorites[stats->favorite_count - 1], 0, sizeof(stats->favorites[stats->favorite_count - 1]));
    stats->favorite_count--;
    return true;
}

void stats_set_last_played(StatsData* stats, int kind, const char* key, const char* label) {
    if (!stats || !key || !key[0] || !label || !label[0]) return;
    memset(&stats->last_played, 0, sizeof(stats->last_played));
    stats->last_played.kind = kind;
    copy_str(stats->last_played.key, sizeof(stats->last_played.key), key);
    copy_str(stats->last_played.label, sizeof(stats->last_played.label), label);
    stats->has_last_played = true;
}

int stats_entry_kind(const StatsEntry* entry) { return entry ? entry->kind : 0; }
const char* stats_entry_key(const StatsEntry* entry) { return entry ? entry->key : ""; }
const char* stats_entry_label(const StatsEntry* entry) { return entry ? entry->label : ""; }
