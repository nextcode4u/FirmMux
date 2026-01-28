#include "fmux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <3ds/os.h>
#include <3ds/ndsp/ndsp.h>
#include <3ds/services/apt.h>
#include <3ds/services/am.h>
#include <3ds/services/ptmu.h>
#include <3ds/services/ac.h>
#include <3ds/services/cfgu.h>
#include <3ds/services/mcuhwc.h>
#include "dikbutt.h"
#include "comfortaa_bold_bcfnt.h"
#include "stb_image.h"

static OptionItem g_options[MAX_OPTIONS];
static int g_option_count = 0;
static bool g_nds_banners = false;
static int g_launcher_cycle = 0;
static bool g_launcher_ready = false;
static u64 g_launcher_tid = 0;
static FS_MediaType g_launcher_media = MEDIATYPE_SD;
static int g_card_launcher_cycle = 0;
static bool g_card_launcher_ready = false;
static u64 g_card_launcher_tid = 0;
static FS_MediaType g_card_launcher_media = MEDIATYPE_SD;
static int g_select_hits = 0;
static int g_easter_timer = 0;
static u8* g_easter_rgba = NULL;
static unsigned g_easter_w = 0, g_easter_h = 0;
static bool g_easter_loaded = false;
static bool g_select_last = false;
static bool g_exit_requested = false;
static bool g_exit_after_status = false;
static u64 g_title_preview_tid = 0;
static bool g_title_preview_valid = false;
static u8 g_title_preview_rgba[48 * 48 * 4];
static IconTexture g_title_preview_icon;
static char g_hb_preview_path[512];
static char g_hb_preview_title[128];
static bool g_hb_preview_valid = false;
static u8 g_hb_preview_rgba[48 * 48 * 4];
static IconTexture g_hb_preview_icon;
static u16 g_hb_preview_raw[48 * 48];
static const float g_preview_offset_x = 0.0f;
static const float g_preview_offset_y = 0.0f;
static C2D_Font g_font;
static bool g_time_24 = true;

static TargetRuntime g_runtimes[MAX_TARGETS];
static Config g_cfg;
static State g_state;
static Theme g_theme;
static RetroRules g_retro;
static EmuConfig g_emu;
static int g_list_item_h = 20;
static int g_line_spacing = 26;
static int g_status_h = 16;
static int g_options_mode = 0;
static char g_theme_names[MAX_THEMES][32];
static int g_theme_name_count = 0;
static OptionItem g_theme_options[MAX_THEMES + 1];
static int g_theme_option_count = 0;
static OptionItem g_emu_options[MAX_SYSTEMS + 1];
static int g_emu_option_count = 0;
static OptionItem g_emu_detail_options[4];
static int g_emu_detail_count = 0;
static int g_emu_detail_index = -1;
static OptionItem g_retro_info_options[8];
static int g_retro_info_count = 0;
static OptionItem g_top_bg_options[MAX_BACKGROUNDS + 1];
static OptionItem g_bottom_bg_options[MAX_BACKGROUNDS + 1];
static int g_top_bg_option_count = 0;
static int g_bottom_bg_option_count = 0;
static OptionItem g_bg_vis_options[16];
static int g_bg_vis_option_count = 0;
static char g_top_bg_names[MAX_BACKGROUNDS][64];
static char g_bottom_bg_names[MAX_BACKGROUNDS][64];
static int g_top_bg_count = 0;
static int g_bottom_bg_count = 0;
static int g_top_bg_index = 0;
static int g_bottom_bg_index = 0;
static IconTexture g_top_bg_tex;
static IconTexture g_bottom_bg_tex;
static Target g_base_targets[MAX_TARGETS];
static int g_base_target_count = 0;

enum {
    OPT_MODE_MAIN = 0,
    OPT_MODE_THEME = 1,
    OPT_MODE_TOP_BG = 2,
    OPT_MODE_BOTTOM_BG = 3,
    OPT_MODE_BG_VIS = 4,
    OPT_MODE_EMULATORS = 5,
    OPT_MODE_EMULATOR_DETAIL = 6,
    OPT_MODE_RETRO_INFO = 7
};

static int clamp_pct(int v);
static void refresh_options_menu(const Config* cfg);

static C2D_TextBuf g_textbuf;
static C3D_RenderTarget* g_top;
static C3D_RenderTarget* g_bottom;
static bool g_card_twl_present = false;
static bool g_card_ctr_present = false;
static bool g_card_twl_ready = false;
static bool g_card_twl_has_icon = false;
static char g_card_twl_title[128];
static u8 g_card_twl_rgba[32 * 32 * 4];
static bool decode_twl_card_banner(const u8* data, size_t size, char* title_out, size_t title_size, u8* rgba_out);

static bool parse_title_id(const char* s, u64* out) {
    if (!s || !s[0]) return false;
    char* end = NULL;
    u64 v = strtoull(s, &end, 16);
    if (end == s) return false;
    *out = v;
    return true;
}

static FS_MediaType media_from_string(const char* s) {
    if (!s || !s[0]) return MEDIATYPE_SD;
    if (!strcasecmp(s, "nand")) return MEDIATYPE_NAND;
    if (!strcasecmp(s, "gamecard")) return MEDIATYPE_GAME_CARD;
    return MEDIATYPE_SD;
}

static const char* media_to_string(FS_MediaType m) {
    switch (m) {
        case MEDIATYPE_NAND: return "nand";
        case MEDIATYPE_GAME_CARD: return "gamecard";
        default: return "sd";
    }
}

typedef struct {
    u64 tid;
    FS_MediaType media;
    char product[16];
} LauncherCandidate;

static void apply_theme_from_state_or_config(const Config* cfg, const State* state) {
    const char* name = NULL;
    if (state && state->theme[0]) name = state->theme;
    else if (cfg && cfg->theme[0]) name = cfg->theme;
    else name = "default";
    load_theme(&g_theme, name);
    g_list_item_h = g_theme.list_item_h > 0 ? g_theme.list_item_h : 20;
    g_line_spacing = g_theme.line_spacing > 0 ? g_theme.line_spacing : 26;
    g_status_h = g_theme.status_h > 0 ? g_theme.status_h : 16;
}

static bool is_dir_path(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static void theme_add_name(const char* name) {
    if (!name || !name[0]) return;
    if (g_theme_name_count >= MAX_THEMES) return;
    for (int i = 0; i < g_theme_name_count; i++) {
        if (!strcasecmp(g_theme_names[i], name)) return;
    }
    copy_str(g_theme_names[g_theme_name_count], sizeof(g_theme_names[g_theme_name_count]), name);
    g_theme_name_count++;
}

static void scan_themes(void) {
    g_theme_name_count = 0;
    theme_add_name("default");
    DIR* dir = opendir("sdmc:/3ds/FirmMux/themes");
    if (!dir) return;
    struct dirent* ent;
    while ((ent = readdir(dir))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        char full[256];
        snprintf(full, sizeof(full), "sdmc:/3ds/FirmMux/themes/%s", ent->d_name);
        if (!is_dir_path(full)) continue;
        theme_add_name(ent->d_name);
    }
    closedir(dir);
    for (int i = 1; i + 1 < g_theme_name_count; i++) {
        for (int j = i + 1; j < g_theme_name_count; j++) {
            if (strcasecmp(g_theme_names[i], g_theme_names[j]) > 0) {
                char tmp[32];
                copy_str(tmp, sizeof(tmp), g_theme_names[i]);
                copy_str(g_theme_names[i], sizeof(g_theme_names[i]), g_theme_names[j]);
                copy_str(g_theme_names[j], sizeof(g_theme_names[j]), tmp);
            }
        }
    }
}

static void build_theme_options(const char* current) {
    g_theme_option_count = 0;
    OptionItem* o = &g_theme_options[g_theme_option_count++];
    snprintf(o->label, sizeof(o->label), "Back");
    o->action = OPTION_ACTION_NONE;
    for (int i = 0; i < g_theme_name_count; i++) {
        if (g_theme_option_count >= MAX_THEMES + 1) break;
        o = &g_theme_options[g_theme_option_count++];
        if (current && !strcasecmp(current, g_theme_names[i])) {
            snprintf(o->label, sizeof(o->label), "%s (current)", g_theme_names[i]);
        } else {
            snprintf(o->label, sizeof(o->label), "%s", g_theme_names[i]);
        }
        o->action = OPTION_ACTION_NONE;
    }
}

static bool is_png_name(const char* name) {
    if (!name) return false;
    const char* dot = strrchr(name, '.');
    if (!dot) return false;
    return strcasecmp(dot, ".png") == 0;
}

static void bg_add_name(char names[MAX_BACKGROUNDS][64], int* count, const char* name) {
    if (!names || !count || !name || !name[0]) return;
    if (*count >= MAX_BACKGROUNDS) return;
    for (int i = 0; i < *count; i++) {
        if (!strcasecmp(names[i], name)) return;
    }
    copy_str(names[*count], sizeof(names[*count]), name);
    (*count)++;
}

static void bg_sort_names(char names[MAX_BACKGROUNDS][64], int count) {
    for (int i = 1; i + 1 < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcasecmp(names[i], names[j]) > 0) {
                char tmp[64];
                copy_str(tmp, sizeof(tmp), names[i]);
                copy_str(names[i], sizeof(names[i]), names[j]);
                copy_str(names[j], sizeof(names[j]), tmp);
            }
        }
    }
}

static int scan_background_dir(const char* dir, char names[MAX_BACKGROUNDS][64]) {
    int count = 0;
    names[count++][0] = 0;
    DIR* d = opendir(dir);
    if (!d) return count;
    struct dirent* ent;
    while ((ent = readdir(d))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        if (!is_png_name(ent->d_name)) continue;
        bg_add_name(names, &count, ent->d_name);
    }
    closedir(d);
    bg_sort_names(names, count);
    return count;
}

static int find_background_index(char names[MAX_BACKGROUNDS][64], int count, const char* want) {
    if (!want || !want[0]) return 0;
    for (int i = 1; i < count; i++) {
        if (!strcasecmp(names[i], want)) return i;
    }
    return 0;
}

static void background_label(const char* name, char* out, size_t out_size) {
    if (!name || !name[0]) {
        copy_str(out, out_size, "None");
        return;
    }
    base_name_no_ext(name, out, out_size);
    if (!out[0]) copy_str(out, out_size, name);
}

static bool load_background_tex(const char* dir, const char* name, IconTexture* icon) {
    if (!icon) return false;
    if (!name || !name[0]) {
        icon_free(icon);
        return false;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    u8* file = NULL;
    size_t fsize = 0;
    if (!read_file(path, &file, &fsize) || !file || fsize == 0) {
        if (file) free(file);
        icon_free(icon);
        return false;
    }
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load_from_memory(file, (int)fsize, &w, &h, &comp, 4);
    free(file);
    if (!data || w <= 0 || h <= 0) {
        if (data) stbi_image_free(data);
        icon_free(icon);
        return false;
    }
    int pixels = w * h;
    for (int i = 0; i < pixels; i++) data[i * 4 + 3] = 255;
    bool ok = icon_from_rgba(icon, data, w, h);
    stbi_image_free(data);
    return ok;
}

static void scan_backgrounds(State* state) {
    g_top_bg_count = scan_background_dir(BACKGROUNDS_TOP_DIR, g_top_bg_names);
    g_bottom_bg_count = scan_background_dir(BACKGROUNDS_BOTTOM_DIR, g_bottom_bg_names);
    g_top_bg_index = find_background_index(g_top_bg_names, g_top_bg_count, state ? state->top_background : NULL);
    g_bottom_bg_index = find_background_index(g_bottom_bg_names, g_bottom_bg_count, state ? state->bottom_background : NULL);
    if (state) {
        copy_str(state->top_background, sizeof(state->top_background), g_top_bg_names[g_top_bg_index]);
        copy_str(state->bottom_background, sizeof(state->bottom_background), g_bottom_bg_names[g_bottom_bg_index]);
    }
    load_background_tex(BACKGROUNDS_TOP_DIR, g_top_bg_names[g_top_bg_index], &g_top_bg_tex);
    load_background_tex(BACKGROUNDS_BOTTOM_DIR, g_bottom_bg_names[g_bottom_bg_index], &g_bottom_bg_tex);
}

static void build_background_options(bool top, State* state) {
    scan_backgrounds(state);
    OptionItem* list = top ? g_top_bg_options : g_bottom_bg_options;
    int* out_count = top ? &g_top_bg_option_count : &g_bottom_bg_option_count;
    char (*names)[64] = top ? g_top_bg_names : g_bottom_bg_names;
    int count = top ? g_top_bg_count : g_bottom_bg_count;
    const char* current = top ? state->top_background : state->bottom_background;

    *out_count = 0;
    OptionItem* o = &list[(*out_count)++];
    snprintf(o->label, sizeof(o->label), "Back");
    o->action = OPTION_ACTION_NONE;

    for (int i = 0; i < count && *out_count < MAX_BACKGROUNDS + 1; i++) {
        o = &list[(*out_count)++];
        char label[96];
        background_label(names[i], label, sizeof(label));
        bool is_current = false;
        if (current && current[0]) {
            is_current = (strcasecmp(current, names[i]) == 0);
        } else {
            is_current = (i == 0);
        }
        if (is_current) snprintf(o->label, sizeof(o->label), "%s (current)", label);
        else snprintf(o->label, sizeof(o->label), "%s", label);
        o->action = OPTION_ACTION_NONE;
    }
}

static void set_background_from_index(bool top, int idx, State* state, char* status_message, size_t status_size) {
    char (*names)[64] = top ? g_top_bg_names : g_bottom_bg_names;
    int count = top ? g_top_bg_count : g_bottom_bg_count;
    if (idx < 0 || idx >= count) return;

    if (top) {
        g_top_bg_index = idx;
        if (state) copy_str(state->top_background, sizeof(state->top_background), names[idx]);
        load_background_tex(BACKGROUNDS_TOP_DIR, names[idx], &g_top_bg_tex);
    } else {
        g_bottom_bg_index = idx;
        if (state) copy_str(state->bottom_background, sizeof(state->bottom_background), names[idx]);
        load_background_tex(BACKGROUNDS_BOTTOM_DIR, names[idx], &g_bottom_bg_tex);
    }

    char label[96];
    background_label(names[idx], label, sizeof(label));
    snprintf(status_message, status_size, "%s background: %s", top ? "Top" : "Bottom", label);
}

static const int g_bg_vis_values[] = { 0, 10, 20, 25, 30, 40, 50, 60, 70, 80, 90, 100 };
static const int g_bg_vis_value_count = (int)(sizeof(g_bg_vis_values) / sizeof(g_bg_vis_values[0]));

static int bg_vis_index_from_value(int value) {
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    int best = 0;
    int best_diff = 1000;
    for (int i = 0; i < g_bg_vis_value_count; i++) {
        int diff = g_bg_vis_values[i] - value;
        if (diff < 0) diff = -diff;
        if (diff < best_diff) {
            best_diff = diff;
            best = i;
        }
    }
    return best;
}

static void build_bg_visibility_options(int current) {
    g_bg_vis_option_count = 0;
    OptionItem* o = &g_bg_vis_options[g_bg_vis_option_count++];
    snprintf(o->label, sizeof(o->label), "Back");
    o->action = OPTION_ACTION_NONE;

    int cur_idx = bg_vis_index_from_value(current);
    for (int i = 0; i < g_bg_vis_value_count && g_bg_vis_option_count < (int)(sizeof(g_bg_vis_options) / sizeof(g_bg_vis_options[0])); i++) {
        o = &g_bg_vis_options[g_bg_vis_option_count++];
        int v = g_bg_vis_values[i];
        if (i == cur_idx) snprintf(o->label, sizeof(o->label), "%d%% (current)", v);
        else snprintf(o->label, sizeof(o->label), "%d%%", v);
        o->action = OPTION_ACTION_NONE;
    }
}

static void set_bg_visibility_from_index(int idx, State* state, char* status_message, size_t status_size) {
    if (!state) return;
    if (idx < 0 || idx >= g_bg_vis_value_count) return;
    state->background_visibility = g_bg_vis_values[idx];
    snprintf(status_message, status_size, "Background visibility: %d%%", state->background_visibility);
}

static void draw_theme_image(const IconTexture* icon, float x, float y, float scale) {
    if (!icon || !icon->loaded) return;
    C2D_DrawImageAt(icon->image, x, y, 0.0f, NULL, scale, scale);
}

static void draw_theme_image_scaled(const IconTexture* icon, float x, float y, float w, float h) {
    if (!icon || !icon->loaded) return;
    float iw = icon->subtex.width > 0 ? icon->subtex.width : 1.0f;
    float ih = icon->subtex.height > 0 ? icon->subtex.height : 1.0f;
    float sx = w / iw;
    float sy = h / ih;
    C2D_DrawImageAt(icon->image, x, y, 0.0f, NULL, sx, sy);
}

static u32 scale_alpha(u32 c, int num, int den) {
    u32 a = (c >> 24) & 0xFF;
    a = (a * (u32)num) / (u32)den;
    return (c & 0x00FFFFFF) | (a << 24);
}

static int clamp_pct(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

static int overlay_pct(void) {
    int vis = clamp_pct(g_state.background_visibility);
    return 100 - vis;
}

static u32 overlay_color(u32 c, bool has_bg) {
    if (!has_bg) return c;
    return scale_alpha(c, overlay_pct(), 100);
}

static u8 overlay_alpha(bool has_bg) {
    if (!has_bg) return 255;
    int pct = overlay_pct();
    return (u8)((pct * 255) / 100);
}

static void draw_theme_image_scaled_alpha(const IconTexture* icon, float x, float y, float w, float h, u8 alpha) {
    if (!icon || !icon->loaded) return;
    float iw = icon->subtex.width > 0 ? icon->subtex.width : 1.0f;
    float ih = icon->subtex.height > 0 ? icon->subtex.height : 1.0f;
    float sx = w / iw;
    float sy = h / ih;
    if (alpha >= 255) {
        C2D_DrawImageAt(icon->image, x, y, 0.0f, NULL, sx, sy);
        return;
    }
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, C2D_Color32(255, 255, 255, alpha), 0.0f);
    C2D_DrawImageAt(icon->image, x, y, 0.0f, &tint, sx, sy);
}

static float align_offset_from_center(float center_norm, float box_h) {
    return (0.5f - center_norm) * box_h;
}

static bool product_matches(const char* prod, const char* want) {
    if (!prod || !want || !want[0]) return false;
    char buf[16];
    size_t len = strlen(prod);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, prod, len);
    buf[len] = 0;
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\0')) {
        buf[len - 1] = 0;
        len--;
    }
    return strcasecmp(buf, want) == 0;
}

static int find_launcher_candidates(LauncherCandidate* out, int max) {
    if (!out || max <= 0) return 0;
    int found = 0;
    if (R_FAILED(amInit())) return 0;
    FS_MediaType medias[2] = { MEDIATYPE_SD, MEDIATYPE_NAND };
    for (int mi = 0; mi < 2 && found < max; mi++) {
        FS_MediaType m = medias[mi];
        u32 count = 0;
        if (R_FAILED(AM_GetTitleCount(m, &count)) || count == 0) continue;
        u64* list = (u64*)malloc(sizeof(u64) * count);
        if (!list) continue;
        u32 read = 0;
        if (R_FAILED(AM_GetTitleList(&read, m, count, list))) { free(list); continue; }
        for (u32 i = 0; i < read && found < max; i++) {
            char prod[16] = {0};
            if (R_FAILED(AM_GetTitleProductCode(m, list[i], prod))) continue;
            if (!product_matches(prod, "CTR-P-FMBP")) continue;
            LauncherCandidate* c = &out[found++];
            c->tid = list[i];
            c->media = m;
            snprintf(c->product, sizeof(c->product), "%s", prod);
        }
        free(list);
    }
    amExit();
    return found;
}

static int find_card_launcher_candidates(LauncherCandidate* out, int max) {
    if (!out || max <= 0) return 0;
    int found = 0;
    if (R_FAILED(amInit())) return 0;
    FS_MediaType medias[2] = { MEDIATYPE_SD, MEDIATYPE_NAND };
    for (int mi = 0; mi < 2 && found < max; mi++) {
        FS_MediaType m = medias[mi];
        u32 count = 0;
        if (R_FAILED(AM_GetTitleCount(m, &count)) || count == 0) continue;
        u64* list = (u64*)malloc(sizeof(u64) * count);
        if (!list) continue;
        u32 read = 0;
        if (R_FAILED(AM_GetTitleList(&read, m, count, list))) { free(list); continue; }
        for (u32 i = 0; i < read && found < max; i++) {
            char prod[16] = {0};
            if (R_FAILED(AM_GetTitleProductCode(m, list[i], prod))) continue;
            if (!product_matches(prod, "NTR Launcher")) continue;
            LauncherCandidate* c = &out[found++];
            c->tid = list[i];
            c->media = m;
            snprintf(c->product, sizeof(c->product), "%s", prod);
        }
        free(list);
    }
    amExit();
    return found;
}

static void auto_set_launcher(Config* cfg, State* state, bool* state_dirty, char* status_message, size_t status_size, int* status_timer) {
    int nds_idx = -1;
    for (int i = 0; i < cfg->target_count; i++) {
        if (!strcmp(cfg->targets[i].type, "rom_browser")) { nds_idx = i; break; }
    }
    if (nds_idx < 0) return;
    Target* targ = &cfg->targets[nds_idx];
    TargetState* ts = get_target_state(state, targ->id);
    bool have_loader = (targ->loader_title_id[0] != 0) || (ts && ts->loader_title_id[0] != 0);
    if (have_loader) return;
    LauncherCandidate cands[4];
    int found = find_launcher_candidates(cands, 4);
    if (found <= 0) return;
    LauncherCandidate* c = &cands[0];
    char tid_hex[32];
    snprintf(tid_hex, sizeof(tid_hex), "%016llX", (unsigned long long)c->tid);
    copy_str(targ->loader_title_id, sizeof(targ->loader_title_id), tid_hex);
    copy_str(targ->loader_media, sizeof(targ->loader_media), media_to_string(c->media));
    if (ts) {
        copy_str(ts->loader_title_id, sizeof(ts->loader_title_id), tid_hex);
        copy_str(ts->loader_media, sizeof(ts->loader_media), media_to_string(c->media));
    }
    g_launcher_ready = true;
    g_launcher_tid = c->tid;
    g_launcher_media = c->media;
    if (state_dirty) *state_dirty = true;
    if (status_message && status_size > 0) {
        snprintf(status_message, status_size, "Launcher set");
        if (status_timer) *status_timer = 90;
    }
}

static void auto_set_card_launcher(Config* cfg, State* state, bool* state_dirty, char* status_message, size_t status_size, int* status_timer) {
    int nds_idx = -1;
    for (int i = 0; i < cfg->target_count; i++) {
        if (!strcmp(cfg->targets[i].type, "rom_browser")) { nds_idx = i; break; }
    }
    if (nds_idx < 0) return;
    Target* targ = &cfg->targets[nds_idx];
    TargetState* ts = get_target_state(state, targ->id);
    bool have_loader = (targ->card_launcher_title_id[0] != 0) || (ts && ts->card_launcher_title_id[0] != 0);
    if (have_loader) return;
    LauncherCandidate cands[4];
    int found = find_card_launcher_candidates(cands, 4);
    if (found <= 0) return;
    LauncherCandidate* c = &cands[0];
    char tid_hex[32];
    snprintf(tid_hex, sizeof(tid_hex), "%016llX", (unsigned long long)c->tid);
    copy_str(targ->card_launcher_title_id, sizeof(targ->card_launcher_title_id), tid_hex);
    copy_str(targ->card_launcher_media, sizeof(targ->card_launcher_media), media_to_string(c->media));
    if (ts) {
        copy_str(ts->card_launcher_title_id, sizeof(ts->card_launcher_title_id), tid_hex);
        copy_str(ts->card_launcher_media, sizeof(ts->card_launcher_media), media_to_string(c->media));
    }
    g_card_launcher_ready = true;
    g_card_launcher_tid = c->tid;
    g_card_launcher_media = c->media;
    if (state_dirty) *state_dirty = true;
    if (status_message && status_size > 0) {
        snprintf(status_message, status_size, "NTR launcher set");
        if (status_timer) *status_timer = 90;
    }
}

bool launch_title_id(u64 title_id, FS_MediaType media, char* status_message, size_t status_size) {
    if (status_message && status_size > 0) status_message[0] = 0;
    Result rc = APT_PrepareToDoApplicationJump(0, title_id, media);
    if (R_FAILED(rc)) {
        if (status_message && status_size > 0) snprintf(status_message, status_size, "Launcher missing (TitleID %016llX)", (unsigned long long)title_id);
        return false;
    }
    u8 param[0x300];
    u8 hmac[0x20];
    memset(param, 0, sizeof(param));
    memset(hmac, 0, sizeof(hmac));
    rc = APT_DoApplicationJump(param, sizeof(param), hmac);
    if (R_FAILED(rc)) {
        if (status_message && status_size > 0) snprintf(status_message, status_size, "Launch failed %lx", (unsigned long)rc);
        return false;
    }
    return true;
}

static int find_target_index(const Config* cfg, const char* id) {
    for (int i = 0; i < cfg->target_count; i++) {
        if (!strcmp(cfg->targets[i].id, id)) return i;
    }
    return -1;
}

static bool launch_nds_loader(const Target* target, const char* sd_path, char* status_message, size_t status_size) {
    u64 tid = 0;
    FS_MediaType media = MEDIATYPE_SD;
    if (target && target->loader_title_id[0]) parse_title_id(target->loader_title_id, &tid);
    if (target && target->loader_media[0]) media = media_from_string(target->loader_media);
    if (!tid && g_launcher_ready) {
        tid = g_launcher_tid;
        media = g_launcher_media;
    }
    if (!tid) {
        if (status_message && status_size > 0) snprintf(status_message, status_size, "Launcher not set");
        return false;
    }
    char norm[512];
    snprintf(norm, sizeof(norm), "%s", sd_path ? sd_path : "");
    normalize_path_to_sd_colon(norm, sizeof(norm));
    if (!write_launch_txt_for_nds(norm)) {
        snprintf(status_message, status_size, "launch.txt failed");
        return false;
    }
    if (launch_title_id(tid, media, status_message, status_size)) return true;
    FS_MediaType alt = (media == MEDIATYPE_NAND) ? MEDIATYPE_SD : MEDIATYPE_NAND;
    if (launch_title_id(tid, alt, status_message, status_size)) return true;
    if (status_message && status_size > 0) snprintf(status_message, status_size, "Install FirmMuxBootstrapLauncher (ID %016llX)", (unsigned long long)tid);
    return false;
}

static bool launch_card_launcher(const Target* target, char* status_message, size_t status_size) {
    u64 tid = 0;
    FS_MediaType media = MEDIATYPE_SD;
    if (target && target->card_launcher_title_id[0]) parse_title_id(target->card_launcher_title_id, &tid);
    if (target && target->card_launcher_media[0]) media = media_from_string(target->card_launcher_media);
    if (!tid && g_card_launcher_ready) {
        tid = g_card_launcher_tid;
        media = g_card_launcher_media;
    }
    if (!tid) {
        if (status_message && status_size > 0) snprintf(status_message, status_size, "Card launcher not set");
        return false;
    }
    if (launch_title_id(tid, media, status_message, status_size)) return true;
    FS_MediaType alt = (media == MEDIATYPE_NAND) ? MEDIATYPE_SD : MEDIATYPE_NAND;
    if (launch_title_id(tid, alt, status_message, status_size)) return true;
    if (status_message && status_size > 0) snprintf(status_message, status_size, "Install card launcher (ID %016llX)", (unsigned long long)tid);
    return false;
}

static void draw_text(float x, float y, float scale, u32 color, const char* str) {
    C2D_Text text;
    if (g_font) C2D_TextFontParse(&text, g_font, g_textbuf, str);
    else C2D_TextParse(&text, g_textbuf, str);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, color);
}

static void draw_text_centered_bias(float x, float y, float scale, u32 color, float box_h, const char* str, float bias) {
    C2D_Text text;
    if (g_font) C2D_TextFontParse(&text, g_font, g_textbuf, str);
    else C2D_TextParse(&text, g_textbuf, str);
    C2D_TextOptimize(&text);
    float w = 0.0f;
    float h = 0.0f;
    C2D_TextGetDimensions(&text, scale, scale, &w, &h);
    float ty = y + (box_h - h) * 0.5f + bias;
    C2D_DrawText(&text, C2D_WithColor, x, ty, 0.0f, scale, scale, color);
}

static void draw_text_centered(float x, float y, float scale, u32 color, float box_h, const char* str) {
    draw_text_centered_bias(x, y, scale, color, box_h, str, -3.0f);
}

static float text_width(float scale, const char* str) {
    C2D_Text text;
    if (g_font) C2D_TextFontParse(&text, g_font, g_textbuf, str);
    else C2D_TextParse(&text, g_textbuf, str);
    float w = 0.0f;
    C2D_TextGetDimensions(&text, scale, scale, &w, NULL);
    return w;
}

static bool show_nds_card(const Target* target, const TargetState* ts) {
    if (!target || !ts) return false;
    if (strcmp(target->type, "rom_browser") != 0) return false;
    if (!g_card_twl_present) return false;
    char cur[256];
    char root[256];
    snprintf(cur, sizeof(cur), "%s", ts->path);
    snprintf(root, sizeof(root), "%s", target->root[0] ? target->root : "/roms/nds/");
    normalize_path(cur);
    normalize_path(root);
    return strcasecmp(cur, root) == 0;
}

static void update_card_status(void) {
    static u64 last_ms = 0;
    u64 now = osGetTime();
    if (last_ms && now - last_ms < 1000) return;
    last_ms = now;
    bool inserted = false;
    FS_CardType type = CARD_CTR;
    bool old_ctr = g_card_ctr_present;
    bool old_twl = g_card_twl_present;
    g_card_ctr_present = false;
    g_card_twl_present = false;
    if (R_SUCCEEDED(FSUSER_CardSlotIsInserted(&inserted)) && inserted && R_SUCCEEDED(FSUSER_GetCardType(&type))) {
        g_card_ctr_present = (type == CARD_CTR);
        g_card_twl_present = (type == CARD_TWL);
    }
    if (g_card_ctr_present != old_ctr) titles_mark_dirty();
    if (g_card_twl_present != old_twl) {
        g_card_twl_ready = false;
        g_card_twl_has_icon = false;
        g_card_twl_title[0] = 0;
    }
    if (g_card_twl_present && !g_card_twl_ready) {
        u8 banner[0x23C0];
        if (R_SUCCEEDED(FSUSER_GetLegacyBannerData(MEDIATYPE_GAME_CARD, 0, banner))) {
            if (decode_twl_card_banner(banner, sizeof(banner), g_card_twl_title, sizeof(g_card_twl_title), g_card_twl_rgba)) {
                g_card_twl_has_icon = true;
            }
        }
        if (g_card_twl_title[0] == 0) copy_str(g_card_twl_title, sizeof(g_card_twl_title), "Game Card");
        g_card_twl_ready = true;
    }
}

static void load_time_format(void) {
    u8 fmt = 0;
    if (R_SUCCEEDED(CFGU_GetConfigInfoBlk2(1, 0x000A, &fmt))) g_time_24 = (fmt == 0);
    else g_time_24 = true;
}

static u8* downscale_rgba_nearest(const u8* src, unsigned sw, unsigned sh, unsigned max, unsigned* out_w, unsigned* out_h) {
    if (!src || sw == 0 || sh == 0) return NULL;
    unsigned dw = sw;
    unsigned dh = sh;
    if (dw > max || dh > max) {
        float scale = (float)max / (float)(dw > dh ? dw : dh);
        dw = (unsigned)(sw * scale);
        dh = (unsigned)(sh * scale);
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
    }
    u8* dst = (u8*)malloc(dw * dh * 4);
    if (!dst) return NULL;
    for (unsigned y = 0; y < dh; y++) {
        unsigned sy = (y * sh) / dh;
        for (unsigned x = 0; x < dw; x++) {
            unsigned sx = (x * sw) / dw;
            const u8* p = src + (sy * sw + sx) * 4;
            u8* d = dst + (y * dw + x) * 4;
            d[0] = p[0];
            d[1] = p[1];
            d[2] = p[2];
            d[3] = p[3];
        }
    }
    if (out_w) *out_w = dw;
    if (out_h) *out_h = dh;
    return dst;
}

static void smdh_icon_to_rgba_tiled(const u16* src, u8* out) {
    for (int ty = 0; ty < 6; ty++) {
        for (int tx = 0; tx < 6; tx++) {
            const u16* tile = src + (ty * 6 + tx) * 64;
            for (int py = 0; py < 8; py++) {
                for (int px = 0; px < 8; px++) {
                    int dx = tx * 8 + px;
                    int dy = ty * 8 + py;
                    u16 pix = tile[py * 8 + px];
                    u8 r = ((pix >> 11) & 0x1F) << 3;
                    u8 g = ((pix >> 5) & 0x3F) << 2;
                    u8 b = (pix & 0x1F) << 3;
                    u8* d = out + (dy * 48 + dx) * 4;
                    d[0] = r;
                    d[1] = g;
                    d[2] = b;
                    d[3] = 255;
                }
            }
        }
    }
}

static void rgb565_linear_to_rgba(const u16* src, u8* out) {
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            u16 pix = src[y * 48 + x];
            u8 r = ((pix >> 11) & 0x1F) << 3;
            u8 g = ((pix >> 5) & 0x3F) << 2;
            u8 b = (pix & 0x1F) << 3;
            u8* d = out + (y * 48 + x) * 4;
            d[0] = r;
            d[1] = g;
            d[2] = b;
            d[3] = 255;
        }
    }
}

static void rgb555_to_rgba_card(u16 c, u8* out, bool transparent) {
    if (transparent) {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    u8 r = (c & 0x1F) << 3;
    u8 g = ((c >> 5) & 0x1F) << 3;
    u8 b = ((c >> 10) & 0x1F) << 3;
    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = 255;
}

static bool decode_twl_card_banner(const u8* data, size_t size, char* title_out, size_t title_size, u8* rgba_out) {
    if (!data || size < 0x840 || !rgba_out) return false;
    const u8* icon = data + 0x20;
    const u8* palette = data + 0x220;
    const u8* title_en = data + 0x240;
    if (title_out && title_size > 0) {
        size_t len = title_size - 1;
        size_t out = 0;
        for (size_t i = 0; i + 1 < 0x100 && out < len; i += 2) {
            u16 ch = title_en[i] | (title_en[i + 1] << 8);
            if (ch == 0 || ch == '\n' || ch == '\r') break;
            if (ch < 128) title_out[out++] = (char)ch;
        }
        title_out[out] = 0;
    }
    u16 pal[16];
    for (int i = 0; i < 16; i++) pal[i] = palette[i * 2] | (palette[i * 2 + 1] << 8);
    for (int i = 0; i < 32 * 32 * 4; i++) rgba_out[i] = 0;
    for (int tile = 0; tile < 16; ++tile) {
        for (int pixel = 0; pixel < 32; ++pixel) {
            u8 a_byte = icon[(tile << 5) + pixel];
            int px = ((tile & 3) << 3) + ((pixel << 1) & 7);
            int py = ((tile >> 2) << 3) + (pixel >> 2);
            u8 idx1 = (a_byte & 0xf0) >> 4;
            u8 idx2 = (a_byte & 0x0f);
            int p1 = ((py * 32) + (px + 1)) * 4;
            int p0 = ((py * 32) + (px + 0)) * 4;
            rgb555_to_rgba_card(pal[idx2], &rgba_out[p0], idx2 == 0);
            rgb555_to_rgba_card(pal[idx1], &rgba_out[p1], idx1 == 0);
        }
    }
    return true;
}

static void smdh_icon_to_rgba_morton(const u16* src, u8* out) {
    for (int ty = 0; ty < 6; ty++) {
        for (int tx = 0; tx < 6; tx++) {
            const u16* tile = src + (ty * 6 + tx) * 64;
            for (int i = 0; i < 64; i++) {
                int x = (i & 1) | ((i >> 2) & 2) | ((i >> 4) & 4);
                int y = ((i >> 1) & 1) | ((i >> 3) & 2) | ((i >> 5) & 4);
                int dx = tx * 8 + x;
                int dy = ty * 8 + y;
                u16 pix = tile[i];
                u8 r = ((pix >> 11) & 0x1F) << 3;
                u8 g = ((pix >> 5) & 0x3F) << 2;
                u8 b = (pix & 0x1F) << 3;
                u8* d = out + (dy * 48 + dx) * 4;
                d[0] = r;
                d[1] = g;
                d[2] = b;
                d[3] = 255;
            }
        }
    }
}

static bool icon_from_smdh_raw(IconTexture* icon, const u16* raw) {
    if (!raw) return false;
    icon_free(icon);
    if (!C3D_TexInit(&icon->tex, 64, 64, GPU_RGB565)) return false;
    C3D_TexSetFilter(&icon->tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&icon->tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    u16* dest = (u16*)icon->tex.data + (64 - 48) * 64;
    const u16* src = raw;
    for (int y = 0; y < 48; y += 8) {
        memcpy(dest, src, 48 * 8 * sizeof(u16));
        src += 48 * 8;
        dest += 64 * 8;
    }
    GSPGPU_FlushDataCache(icon->tex.data, icon->tex.size);
    icon->image.tex = &icon->tex;
    icon->image.subtex = &icon->subtex;
    icon->subtex.width = 48;
    icon->subtex.height = 48;
    icon->subtex.left = 0.0f;
    icon->subtex.top = 48.0f / 64.0f;
    icon->subtex.right = 48.0f / 64.0f;
    icon->subtex.bottom = 0.0f;
    icon->loaded = true;
    return true;
}

static bool update_title_preview_rgba(const TitleInfo3ds* tinfo) {
    if (!tinfo || !tinfo->has_icon) {
        g_title_preview_valid = false;
        return false;
    }
    if (g_title_preview_valid && g_title_preview_tid == tinfo->titleId) {
        if (tinfo->icon_linear) return true;
        if (g_title_preview_icon.loaded) return true;
    }
    g_title_preview_tid = tinfo->titleId;
    if (tinfo->icon_linear) {
        rgb565_linear_to_rgba(tinfo->icon_raw, g_title_preview_rgba);
        g_title_preview_valid = true;
        icon_free(&g_title_preview_icon);
        return true;
    }
    smdh_icon_to_rgba_tiled(tinfo->icon_raw, g_title_preview_rgba);
    g_title_preview_valid = true;
    icon_from_smdh_raw(&g_title_preview_icon, tinfo->icon_raw);
    return true;
}

static void clear_hb_preview(void) {
    g_hb_preview_path[0] = 0;
    g_hb_preview_title[0] = 0;
    g_hb_preview_valid = false;
    icon_free(&g_hb_preview_icon);
}

static bool update_homebrew_preview(const char* sd_path) {
    if (!sd_path || !sd_path[0]) { clear_hb_preview(); return false; }
    if (!strcmp(sd_path, g_hb_preview_path) && g_hb_preview_icon.loaded) return g_hb_preview_valid;
    clear_hb_preview();
    copy_str(g_hb_preview_path, sizeof(g_hb_preview_path), sd_path);
    if (!homebrew_load_meta(sd_path, g_hb_preview_title, sizeof(g_hb_preview_title), g_hb_preview_raw, 48 * 48)) {
        g_hb_preview_title[0] = 0;
        return false;
    }
    smdh_icon_to_rgba_tiled(g_hb_preview_raw, g_hb_preview_rgba);
    g_hb_preview_valid = true;
    icon_from_smdh_raw(&g_hb_preview_icon, g_hb_preview_raw);
    return true;
}

static void draw_rgba_icon(float x, float y, float scale, const u8* rgba, int w, int h) {
    if (!rgba) return;
    float step = scale;
    for (int iy = 0; iy < h; iy++) {
        for (int ix = 0; ix < w; ix++) {
            const u8* p = rgba + (iy * w + ix) * 4;
            if (p[3] == 0) continue;
            u32 c = C2D_Color32(p[0], p[1], p[2], p[3]);
            C2D_DrawRectSolid(x + ix * step, y + iy * step, 0.0f, step, step, c);
        }
    }
}

static void draw_wrap_text(float x, float y, float scale, u32 color, float max_w, const char* str) {
    char buf[128];
    copy_str(buf, sizeof(buf), str);
    char* token = buf;
    float line_y = y;
    while (*token) {
        char line[128];
        int line_len = 0;
        float width = 0;
        char* p = token;
        char* last_space = NULL;
        int last_space_len = 0;
        while (*p) {
            if (*p == ' ') { last_space = p; last_space_len = line_len; }
            line[line_len++] = *p++;
            line[line_len] = 0;
            C2D_Text text;
            if (g_font) C2D_TextFontParse(&text, g_font, g_textbuf, line);
            else C2D_TextParse(&text, g_textbuf, line);
            C2D_TextGetDimensions(&text, scale, scale, &width, NULL);
            if (width > max_w) break;
        }
        if (width > max_w && last_space) {
            line_len = last_space_len;
            token = last_space + 1;
        } else {
            token += line_len;
        }
        line[line_len] = 0;
        draw_text(x, line_y, scale, color, line);
        while (*token == ' ') token++;
        line_y += 14 * scale;
    }
}

static void draw_wrap_text_limited(float x, float y, float scale, u32 color, float max_w, int max_lines, const char* str) {
    char buf[128];
    copy_str(buf, sizeof(buf), str);
    char* token = buf;
    float line_y = y;
    int lines = 0;
    while (*token && lines < max_lines) {
        char line[128];
        int line_len = 0;
        float width = 0;
        char* p = token;
        char* last_space = NULL;
        int last_space_len = 0;
        while (*p) {
            if (*p == ' ') { last_space = p; last_space_len = line_len; }
            line[line_len++] = *p++;
            line[line_len] = 0;
            C2D_Text text;
            if (g_font) C2D_TextFontParse(&text, g_font, g_textbuf, line);
            else C2D_TextParse(&text, g_textbuf, line);
            C2D_TextGetDimensions(&text, scale, scale, &width, NULL);
            if (width > max_w) break;
        }
        if (width > max_w && last_space) {
            line_len = last_space_len;
            token = last_space + 1;
        } else {
            token += line_len;
        }
        line[line_len] = 0;
        draw_text(x, line_y, scale, color, line);
        while (*token == ' ') token++;
        line_y += g_line_spacing * scale;
        lines++;
    }
}

static void draw_rect(float x, float y, float w, float h, u32 color) {
    C2D_DrawRectSolid(x, y, 0.0f, w, h, color);
}

#define STATUS_H 16

typedef struct {
    u64 last_ms;
    u64 sd_total;
    u64 sd_free;
    u64 nand_total;
    u64 nand_free;
    int cpu_mhz;
    bool cpu_ok;
    u32 am_counts[3];
    u32 am_app_counts[3];
    bool am_ok;
    bool am_app_ok;
    u8 model;
    u8 region;
    u8 language;
    u8 slider_vol;
    u8 battery_mv;
    bool model_ok;
    bool region_ok;
    bool language_ok;
    bool slider_ok;
    bool battery_ok;
    bool sd_speed_ok;
    bool nand_speed_ok;
    FS_SdMmcSpeedInfo sd_speed;
    FS_SdMmcSpeedInfo nand_speed;
    bool sd_ok;
    bool nand_ok;
} SystemInfoCache;

static SystemInfoCache g_sysinfo;

static bool get_sd_usage(u64* total, u64* free) {
    FS_ArchiveResource res;
    if (R_FAILED(FSUSER_GetSdmcArchiveResource(&res))) return false;
    u64 cluster_bytes = (u64)res.clusterSize;
    *total = cluster_bytes * res.totalClusters;
    *free = cluster_bytes * res.freeClusters;
    return true;
}

static bool get_nand_usage(u64* total, u64* free) {
    FS_ArchiveResource res;
    if (R_FAILED(FSUSER_GetNandArchiveResource(&res))) return false;
    u64 cluster_bytes = (u64)res.clusterSize;
    *total = cluster_bytes * res.totalClusters;
    *free = cluster_bytes * res.freeClusters;
    return true;
}

static void format_bytes(u64 bytes, char* out, size_t out_size) {
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
        snprintf(out, out_size, "%.1f GB", gb);
    } else if (bytes >= 1024ULL * 1024ULL) {
        double mb = (double)bytes / (1024.0 * 1024.0);
        snprintf(out, out_size, "%.1f MB", mb);
    } else if (bytes >= 1024ULL) {
        double kb = (double)bytes / 1024.0;
        snprintf(out, out_size, "%.1f KB", kb);
    } else {
        snprintf(out, out_size, "%llu B", (unsigned long long)bytes);
    }
}

static void update_system_info_cache(void) {
    u64 now = osGetTime();
    if (g_sysinfo.last_ms && now - g_sysinfo.last_ms < 1000) return;
    g_sysinfo.last_ms = now;
    g_sysinfo.sd_ok = get_sd_usage(&g_sysinfo.sd_total, &g_sysinfo.sd_free);
    g_sysinfo.nand_ok = get_nand_usage(&g_sysinfo.nand_total, &g_sysinfo.nand_free);
    g_sysinfo.cpu_ok = true;
    g_sysinfo.cpu_mhz = 268;
    bool is_new = false;
    if (R_SUCCEEDED(APT_CheckNew3DS(&is_new)) && is_new) {
        u32 limit = 0;
        if (R_SUCCEEDED(APT_GetAppCpuTimeLimit(&limit)) && limit >= 80) g_sysinfo.cpu_mhz = 804;
    }

    g_sysinfo.model_ok = R_SUCCEEDED(CFGU_GetSystemModel(&g_sysinfo.model));
    g_sysinfo.region_ok = R_SUCCEEDED(CFGU_SecureInfoGetRegion(&g_sysinfo.region));
    g_sysinfo.language_ok = R_SUCCEEDED(CFGU_GetSystemLanguage(&g_sysinfo.language));
    g_sysinfo.slider_ok = R_SUCCEEDED(MCUHWC_GetSoundSliderLevel(&g_sysinfo.slider_vol));
    g_sysinfo.battery_ok = R_SUCCEEDED(MCUHWC_GetBatteryVoltage(&g_sysinfo.battery_mv));
    g_sysinfo.sd_speed_ok = R_SUCCEEDED(FSUSER_GetSdmcSpeedInfo(&g_sysinfo.sd_speed));
    g_sysinfo.nand_speed_ok = R_SUCCEEDED(FSUSER_GetNandSpeedInfo(&g_sysinfo.nand_speed));

    g_sysinfo.am_ok = false;
    g_sysinfo.am_app_ok = false;
    if (R_SUCCEEDED(amInit())) {
        g_sysinfo.am_ok = true;
        AM_InitializeExternalTitleDatabase(false);
        for (int i = 0; i < 3; i++) g_sysinfo.am_counts[i] = 0;
        FS_MediaType medias[3] = { MEDIATYPE_SD, MEDIATYPE_NAND, MEDIATYPE_GAME_CARD };
        for (int i = 0; i < 3; i++) {
            u32 count = 0;
            if (R_SUCCEEDED(AM_GetTitleCount(medias[i], &count))) g_sysinfo.am_counts[i] = count;
        }
        amExit();
    }
    if (R_SUCCEEDED(amAppInit())) {
        g_sysinfo.am_app_ok = true;
        AM_InitializeExternalTitleDatabase(false);
        for (int i = 0; i < 3; i++) g_sysinfo.am_app_counts[i] = 0;
        FS_MediaType medias[3] = { MEDIATYPE_SD, MEDIATYPE_NAND, MEDIATYPE_GAME_CARD };
        for (int i = 0; i < 3; i++) {
            u32 count = 0;
            if (R_SUCCEEDED(AM_GetTitleCount(medias[i], &count))) g_sysinfo.am_app_counts[i] = count;
        }
        amExit();
    }
}

static void draw_system_info(float x, float y) {
    update_system_info_cache();
    float line_h = 14.0f;
    u32 color = g_theme.text_primary;
    u32 sub = g_theme.text_secondary;
    draw_text(x, y, 0.5f, color, "System Info");
    y += line_h;

    char line[128];
    const char* model_str = "--";
    if (g_sysinfo.model_ok) {
        switch (g_sysinfo.model) {
            case CFG_MODEL_3DS: model_str = "3DS"; break;
            case CFG_MODEL_3DSXL: model_str = "3DS XL"; break;
            case CFG_MODEL_2DS: model_str = "2DS"; break;
            case CFG_MODEL_N3DS: model_str = "New 3DS"; break;
            case CFG_MODEL_N3DSXL: model_str = "New 3DS XL"; break;
            case CFG_MODEL_N2DSXL: model_str = "New 2DS XL"; break;
            default: model_str = "Unknown"; break;
        }
    }
    const char* region_str = "--";
    if (g_sysinfo.region_ok) {
        switch (g_sysinfo.region) {
            case CFG_REGION_JPN: region_str = "JPN"; break;
            case CFG_REGION_USA: region_str = "USA"; break;
            case CFG_REGION_EUR: region_str = "EUR"; break;
            case CFG_REGION_AUS: region_str = "AUS"; break;
            case CFG_REGION_CHN: region_str = "CHN"; break;
            case CFG_REGION_KOR: region_str = "KOR"; break;
            case CFG_REGION_TWN: region_str = "TWN"; break;
            default: region_str = "UNK"; break;
        }
    }
    const char* lang_str = "--";
    if (g_sysinfo.language_ok) {
        switch (g_sysinfo.language) {
            case CFG_LANGUAGE_JP: lang_str = "JP"; break;
            case CFG_LANGUAGE_EN: lang_str = "EN"; break;
            case CFG_LANGUAGE_FR: lang_str = "FR"; break;
            case CFG_LANGUAGE_DE: lang_str = "DE"; break;
            case CFG_LANGUAGE_IT: lang_str = "IT"; break;
            case CFG_LANGUAGE_ES: lang_str = "ES"; break;
            case CFG_LANGUAGE_ZH: lang_str = "ZH"; break;
            case CFG_LANGUAGE_KO: lang_str = "KO"; break;
            case CFG_LANGUAGE_NL: lang_str = "NL"; break;
            case CFG_LANGUAGE_PT: lang_str = "PT"; break;
            case CFG_LANGUAGE_RU: lang_str = "RU"; break;
            case CFG_LANGUAGE_TW: lang_str = "TW"; break;
            default: lang_str = "UNK"; break;
        }
    }
    snprintf(line, sizeof(line), "Model: %s  Region: %s  Lang: %s", model_str, region_str, lang_str);
    draw_text(x, y, 0.5f, sub, line);
    y += line_h;

    char used_buf[32];
    char free_buf[32];
    if (g_sysinfo.sd_ok) {
        u64 sd_used = g_sysinfo.sd_total - g_sysinfo.sd_free;
        format_bytes(sd_used, used_buf, sizeof(used_buf));
        format_bytes(g_sysinfo.sd_free, free_buf, sizeof(free_buf));
        snprintf(line, sizeof(line), "SD used: %s", used_buf);
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
        snprintf(line, sizeof(line), "SD free: %s", free_buf);
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
    } else {
        draw_text(x, y, 0.5f, sub, "SD: unavailable");
        y += line_h;
    }

    if (g_sysinfo.nand_ok) {
        u64 nand_used = g_sysinfo.nand_total - g_sysinfo.nand_free;
        format_bytes(nand_used, used_buf, sizeof(used_buf));
        format_bytes(g_sysinfo.nand_free, free_buf, sizeof(free_buf));
        snprintf(line, sizeof(line), "NAND used: %s", used_buf);
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
        snprintf(line, sizeof(line), "NAND free: %s", free_buf);
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
    } else {
        draw_text(x, y, 0.5f, sub, "NAND: unavailable");
        y += line_h;
    }

    if (g_sysinfo.cpu_ok) {
        snprintf(line, sizeof(line), "CPU: %d MHz", g_sysinfo.cpu_mhz);
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
    }
    if (g_sysinfo.slider_ok) {
        int vol_raw = g_sysinfo.slider_vol;
        if (vol_raw > 0x3F) vol_raw = 0x3F;
        int vol = (int)((vol_raw / 63.0f) * 100.0f + 0.5f);
        snprintf(line, sizeof(line), "Volume: %d%%", vol);
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
    }

    if (g_sysinfo.battery_ok) {
        int mv = (int)g_sysinfo.battery_mv * 10;
        snprintf(line, sizeof(line), "Battery: %d mV", mv);
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
    }

    if (g_sysinfo.sd_speed_ok) {
        snprintf(line, sizeof(line), "SD HS: %s  CLK: %s", g_sysinfo.sd_speed.highSpeedModeEnabled ? "On" : "Off", g_sysinfo.sd_speed.usesHighestClockRate ? "/2" : "/1");
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
    }
    if (g_sysinfo.nand_speed_ok) {
        snprintf(line, sizeof(line), "NAND HS: %s  CLK: %s", g_sysinfo.nand_speed.highSpeedModeEnabled ? "On" : "Off", g_sysinfo.nand_speed.usesHighestClockRate ? "/2" : "/1");
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
    }

    if (g_sysinfo.am_ok) {
        snprintf(line, sizeof(line), "AM SD/NAND/GC: %lu/%lu/%lu",
                 (unsigned long)g_sysinfo.am_counts[0],
                 (unsigned long)g_sysinfo.am_counts[1],
                 (unsigned long)g_sysinfo.am_counts[2]);
        draw_text(x, y, 0.5f, sub, line);
        y += line_h;
    }
    if (g_sysinfo.am_app_ok) {
        snprintf(line, sizeof(line), "AMapp SD/NAND/GC: %lu/%lu/%lu",
                 (unsigned long)g_sysinfo.am_app_counts[0],
                 (unsigned long)g_sysinfo.am_app_counts[1],
                 (unsigned long)g_sysinfo.am_app_counts[2]);
        draw_text(x, y, 0.5f, sub, line);
    }
}

static void draw_status_bar(void) {
    float y = TOP_H - g_status_h;
    u32 status_bg = overlay_color(g_theme.status_bg, g_top_bg_tex.loaded);
    draw_rect(0, y, TOP_W, g_status_h, status_bg);

    char timebuf[16];
    time_t now = time(NULL);
    struct tm* tmv = localtime(&now);
    if (tmv) {
        if (g_time_24) {
            snprintf(timebuf, sizeof(timebuf), "%02d:%02d", tmv->tm_hour, tmv->tm_min);
        } else {
            int hour = tmv->tm_hour % 12;
            if (hour == 0) hour = 12;
            snprintf(timebuf, sizeof(timebuf), "%d:%02d %s", hour, tmv->tm_min, (tmv->tm_hour >= 12) ? "PM" : "AM");
        }
    } else {
        copy_str(timebuf, sizeof(timebuf), "--:--");
    }
    draw_text_centered(8, y + g_theme.status_text_offset_y, 0.55f, g_theme.status_text, g_status_h, timebuf);

    u8 batt = 0;
    u8 charging = 0;
    if (R_FAILED(PTMU_GetBatteryLevel(&batt))) batt = 0;
    if (R_FAILED(PTMU_GetBatteryChargeState(&charging))) charging = 0;
    if (batt > 5) batt = 5;
    int percent = (int)batt * 20;

    char battbuf[8];
    snprintf(battbuf, sizeof(battbuf), "%d%%", percent);
    float batt_text_w = text_width(0.5f, battbuf);

    int wifi_bars = 0;
    u32 wifi_status = 0;
    if (R_SUCCEEDED(ACU_GetStatus(&wifi_status)) && wifi_status == 3) wifi_bars = 3;

    float batt_w = 22.0f;
    float batt_h = 8.0f;
    float batt_x = TOP_W - 8 - batt_w;
    float batt_y = y + (g_status_h - batt_h) * 0.5f;
    float percent_x = batt_x - 4 - batt_text_w;
    draw_text_centered(percent_x, y + g_theme.status_text_offset_y, 0.5f, g_theme.status_text, g_status_h, battbuf);

    float wifi_w = 19.0f;
    float wifi_x = percent_x - 6 - wifi_w;
    float wifi_y = y + (g_status_h - 8.0f) * 0.5f;
    u32 wifi_color = g_theme.status_icon;
    for (int i = 0; i < 3; i++) {
        float bw = 5.0f;
        float bh = 4.0f + i * 2.0f;
        float bx = wifi_x + i * 7.0f;
        float by = wifi_y + (8.0f - bh);
        u32 c = (i < wifi_bars) ? wifi_color : g_theme.status_dim;
        draw_rect(bx, by, bw, bh, c);
    }

    u32 outline = g_theme.status_icon;
    draw_rect(batt_x, batt_y, batt_w, batt_h, outline);
    draw_rect(batt_x + batt_w, batt_y + 2, 2, batt_h - 4, outline);
    float fill_w = (batt_w - 4) * (batt / 5.0f);
    u32 fill = g_theme.status_icon;
    draw_rect(batt_x + 2, batt_y + 2, fill_w, batt_h - 4, fill);
    if (charging) {
        u32 bolt = g_theme.status_bolt;
        float bx = batt_x + (batt_w - 6) * 0.5f;
        float by = batt_y - 1;
        draw_rect(bx + 2, by + 0, 2, 4, bolt);
        draw_rect(bx + 0, by + 3, 2, 4, bolt);
        draw_rect(bx + 2, by + 5, 2, 3, bolt);
    }
}

static void draw_help_bar(const char* label) {
    u32 help_line = overlay_color(g_theme.help_line, g_bottom_bg_tex.loaded);
    u32 help_bg = overlay_color(g_theme.help_bg, g_bottom_bg_tex.loaded);
    draw_rect(0, BOTTOM_H - HELP_BAR_H - 2, BOTTOM_W, 1, help_line);
    draw_rect(0, BOTTOM_H - HELP_BAR_H - 1, BOTTOM_W, 1, help_line);
    draw_rect(0, BOTTOM_H - HELP_BAR_H, BOTTOM_W, HELP_BAR_H, help_bg);
    draw_text(6, BOTTOM_H - HELP_BAR_H + 2 + g_theme.help_text_offset_y, 0.6f, g_theme.help_text, label);
}


static void clamp_scroll_list(int* scroll, int selection, int visible, int count) {
    if (count <= 0) { *scroll = 0; return; }
    if (selection < *scroll) *scroll = selection;
    if (selection >= *scroll + visible) *scroll = selection - visible + 1;
    if (*scroll < 0) *scroll = 0;
    if (*scroll > count - visible) *scroll = count - visible;
    if (*scroll < 0) *scroll = 0;
}

static void clamp_scroll_grid(int* scroll, int selection, int visible_rows, int total_rows) {
    if (total_rows <= 0) { *scroll = 0; return; }
    int sel_row = selection / GRID_COLS;
    if (sel_row < *scroll) *scroll = sel_row;
    if (sel_row >= *scroll + visible_rows) *scroll = sel_row - visible_rows + 1;
    if (*scroll < 0) *scroll = 0;
    if (*scroll > total_rows - visible_rows) *scroll = total_rows - visible_rows;
    if (*scroll < 0) *scroll = 0;
}

static void preload_nds_page(const TargetState* ts, const DirCache* cache, int visible, int budget, int card_offset) {
    int done = 0;
    for (int i = 0; i < visible && done < budget; i++) {
        int list_idx = ts->scroll + i;
        int idx = list_idx - card_offset;
        if (idx < 0) continue;
        if (idx >= cache->count) break;
        if (cache->entries[idx].is_dir) continue;
        if (!is_nds_name(cache->entries[idx].name)) continue;
        char joined[512];
        path_join(ts->path, cache->entries[idx].name, joined, sizeof(joined));
        char sdpath[512];
        make_sd_path(joined, sdpath, sizeof(sdpath));
        NdsCacheEntry* nds = nds_cache_entry(sdpath);
        if (nds && !nds->ready && !nds->in_progress && !nds->dirty) {
            build_nds_entry(sdpath);
            done++;
        }
    }
}

static bool is_emulator_target(const Target* target) {
    return target && !strcmp(target->type, "retroarch_system");
}

static void sd_path_to_internal_root(const char* sd_path, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!sd_path || !sd_path[0]) return;
    char tmp[512];
    copy_str(tmp, sizeof(tmp), sd_path);
    normalize_path_sd(tmp, sizeof(tmp));
    const char* p = tmp;
    if (!strncasecmp(tmp, "sd:/", 4)) p = tmp + 4;
    else if (!strncasecmp(tmp, "sdmc:/", 6)) p = tmp + 6;
    while (*p == '/') p++;
    if (*p) snprintf(out, out_size, "/%s", p);
    else snprintf(out, out_size, "/");
    normalize_path(out);
}

static bool target_root_exists(const Target* target) {
    if (!target || !target->root[0]) return true;
    char sdmc[512];
    make_sd_path(target->root, sdmc, sizeof(sdmc));
    return is_dir_path(sdmc);
}

static void capture_base_targets(const Config* cfg) {
    g_base_target_count = 0;
    if (!cfg) return;
    for (int i = 0; i < cfg->target_count && g_base_target_count < MAX_TARGETS; i++) {
        const Target* t = &cfg->targets[i];
        if (is_emulator_target(t)) continue;
        g_base_targets[g_base_target_count++] = *t;
    }
}

static bool is_nds_base_target(const Target* t) {
    if (!t) return false;
    if (strcmp(t->type, "rom_browser") != 0) return false;
    if (!strcasecmp(t->id, "nds")) return true;
    return strstr(t->root, "/roms/nds") != NULL;
}

static void reorder_base_targets(Config* cfg) {
    if (!cfg || cfg->target_count <= 1) return;
    Target reordered[MAX_TARGETS];
    bool used[MAX_TARGETS];
    memset(used, 0, sizeof(used));
    int count = 0;

    for (int pass = 0; pass < 4; pass++) {
        for (int i = 0; i < cfg->target_count && count < MAX_TARGETS; i++) {
            if (used[i]) continue;
            const Target* t = &cfg->targets[i];
            bool take = false;
            if (pass == 0 && !strcmp(t->type, "system_menu")) take = true;
            else if (pass == 1 && !strcmp(t->type, "installed_titles")) take = true;
            else if (pass == 2 && is_nds_base_target(t)) take = true;
            else if (pass == 3 && !strcmp(t->type, "homebrew_browser")) take = true;
            if (!take) continue;
            reordered[count++] = *t;
            used[i] = true;
            break;
        }
    }

    for (int i = 0; i < cfg->target_count && count < MAX_TARGETS; i++) {
        if (used[i]) continue;
        reordered[count++] = cfg->targets[i];
        used[i] = true;
    }

    cfg->target_count = count;
    for (int i = 0; i < count; i++) cfg->targets[i] = reordered[i];
}

static void apply_emulator_targets(Config* cfg) {
    if (!cfg) return;
    if (g_base_target_count == 0) capture_base_targets(cfg);
    cfg->target_count = g_base_target_count;
    for (int i = 0; i < g_base_target_count; i++) cfg->targets[i] = g_base_targets[i];
    reorder_base_targets(cfg);

    for (int i = 0; i < g_emu.count && cfg->target_count < MAX_TARGETS; i++) {
        const EmuSystem* sys = &g_emu.systems[i];
        if (!sys->enabled) continue;
        Target* t = &cfg->targets[cfg->target_count];
        memset(t, 0, sizeof(*t));
        copy_str(t->id, sizeof(t->id), sys->key);
        copy_str(t->type, sizeof(t->type), "retroarch_system");

        char internal_root[256];
        sd_path_to_internal_root(sys->rom_folder, internal_root, sizeof(internal_root));
        if (!internal_root[0]) snprintf(internal_root, sizeof(internal_root), "/roms/%s", sys->key);
        copy_str(t->root, sizeof(t->root), internal_root);

        bool missing = !target_root_exists(t);
        if (missing) snprintf(t->label, sizeof(t->label), "%s (Missing folder)", sys->display_name);
        else copy_str(t->label, sizeof(t->label), sys->display_name);

        char exts[MAX_EXTENSIONS][16];
        int ext_count = retro_extensions_for_system(&g_retro, sys->key, exts);
        t->ext_count = 0;
        for (int e = 0; e < ext_count && t->ext_count < MAX_EXTENSIONS; e++) {
            if (!exts[e][0]) continue;
            snprintf(t->extensions[t->ext_count], sizeof(t->extensions[t->ext_count]), ".%s", exts[e]);
            t->ext_count++;
        }

        cfg->target_count++;
    }
}

static TargetState* state_find(State* state, const char* id) {
    if (!state || !id) return NULL;
    for (int i = 0; i < state->count; i++) {
        if (!strcmp(state->entries[i].id, id)) return &state->entries[i];
    }
    return NULL;
}

static void prune_state_targets(State* state, const Config* cfg) {
    if (!state || !cfg) return;
    TargetState next[MAX_TARGETS];
    int next_count = 0;
    for (int i = 0; i < cfg->target_count && next_count < MAX_TARGETS; i++) {
        const Target* t = &cfg->targets[i];
        TargetState* existing = state_find(state, t->id);
        if (existing) {
            next[next_count++] = *existing;
        } else {
            TargetState fresh;
            memset(&fresh, 0, sizeof(fresh));
            copy_str(fresh.id, sizeof(fresh.id), t->id);
            if (t->root[0]) copy_str(fresh.path, sizeof(fresh.path), t->root);
            next[next_count++] = fresh;
        }
    }
    memcpy(state->entries, next, sizeof(next));
    state->count = next_count;
}

static TargetState* ensure_target_state(State* state, const Config* cfg, const Target* target) {
    if (!state || !cfg || !target) return NULL;
    TargetState* ts = get_target_state(state, target->id);
    if (ts) return ts;
    prune_state_targets(state, cfg);
    return get_target_state(state, target->id);
}

static bool emulator_folder_exists(const EmuSystem* sys) {
    if (!sys || !sys->rom_folder[0]) return false;
    char internal[256];
    sd_path_to_internal_root(sys->rom_folder, internal, sizeof(internal));
    if (!internal[0]) return false;
    char sdmc[512];
    make_sd_path(internal, sdmc, sizeof(sdmc));
    return is_dir_path(sdmc);
}

static void build_emulator_options(void) {
    g_emu_option_count = 0;
    OptionItem* o = &g_emu_options[g_emu_option_count++];
    snprintf(o->label, sizeof(o->label), "Back");
    o->action = OPTION_ACTION_NONE;
    for (int i = 0; i < g_emu.count && g_emu_option_count < MAX_SYSTEMS + 1; i++) {
        const EmuSystem* sys = &g_emu.systems[i];
        o = &g_emu_options[g_emu_option_count++];
        snprintf(o->label, sizeof(o->label), "%s: %s", sys->display_name, sys->enabled ? "On" : "Off");
        o->action = OPTION_ACTION_NONE;
    }
}

static void build_emulator_detail_options(int index) {
    g_emu_detail_index = index;
    g_emu_detail_count = 0;
    OptionItem* o = &g_emu_detail_options[g_emu_detail_count++];
    snprintf(o->label, sizeof(o->label), "Back");
    o->action = OPTION_ACTION_NONE;
    if (index < 0 || index >= g_emu.count) return;
    EmuSystem* sys = &g_emu.systems[index];
    o = &g_emu_detail_options[g_emu_detail_count++];
    snprintf(o->label, sizeof(o->label), "Enabled: %s", sys->enabled ? "On" : "Off");
    o->action = OPTION_ACTION_NONE;
    o = &g_emu_detail_options[g_emu_detail_count++];
    bool missing = !emulator_folder_exists(sys);
    if (missing) snprintf(o->label, sizeof(o->label), "ROM folder: %s (Missing)", sys->rom_folder);
    else snprintf(o->label, sizeof(o->label), "ROM folder: %s", sys->rom_folder);
    o->action = OPTION_ACTION_NONE;
}

static void emu_cycle_rom_folder(EmuSystem* sys) {
    if (!sys) return;
    const char* keys[MAX_SYSTEMS];
    int key_count = emu_known_system_keys(keys, MAX_SYSTEMS);
    if (key_count <= 0) return;
    int cur = -1;
    char norm[512];
    copy_str(norm, sizeof(norm), sys->rom_folder);
    normalize_path_sd(norm, sizeof(norm));
    for (int i = 0; i < key_count; i++) {
        char want[64];
        snprintf(want, sizeof(want), "sd:/roms/%s", keys[i]);
        if (!strcasecmp(norm, want)) { cur = i; break; }
    }
    int next = (cur + 1 + key_count) % key_count;
    snprintf(sys->rom_folder, sizeof(sys->rom_folder), "sd:/roms/%s", keys[next]);
}

static void build_retro_info_options(void) {
    g_retro_info_count = 0;
    OptionItem* o = &g_retro_info_options[g_retro_info_count++];
    snprintf(o->label, sizeof(o->label), "Back");
    o->action = OPTION_ACTION_NONE;
    const char* lines[] = {
        "Expected: sd:/3ds/FirmMux/emulators/retroarch.3dsx",
        "Cores/system: sd:/retroarch/",
        "Required BIOS: Atari800 BIOS ROMs",
        "Required BIOS: blueMSX Databases+Machines",
        "Required BIOS: PCE CD syscard*.pce",
        "Recommended: gba_bios.bin (GBA)",
        "Recommended: real BIOS (PS1)"
    };
    const int line_count = (int)(sizeof(lines) / sizeof(lines[0]));
    for (int i = 0; i < line_count && g_retro_info_count < (int)(sizeof(g_retro_info_options) / sizeof(g_retro_info_options[0])); i++) {
        o = &g_retro_info_options[g_retro_info_count++];
        copy_str(o->label, sizeof(o->label), lines[i]);
        o->action = OPTION_ACTION_NONE;
    }
}

static void file_ext_lower(const char* name, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = 0;
    if (!name) return;
    const char* dot = strrchr(name, '.');
    if (!dot || !dot[1]) return;
    dot++;
    size_t n = strlen(dot);
    if (n >= out_size) n = out_size - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)dot[i]);
    out[n] = 0;
}

static bool retro_launch_selected(const Target* target, TargetState* ts, const FileEntry* fe, const char* joined, State* state, char* status_message, size_t status_size, int* status_timer, bool* state_dirty) {
    if (!target || !ts || !fe || fe->is_dir || !joined) return false;
    char sdmc_path[512];
    make_sd_path(joined, sdmc_path, sizeof(sdmc_path));
    char rom_sd[512];
    copy_str(rom_sd, sizeof(rom_sd), sdmc_path);
    normalize_path_sd(rom_sd, sizeof(rom_sd));

    char system_key[16];
    if (!emu_resolve_system(&g_emu, rom_sd, target->id, system_key, sizeof(system_key))) {
        copy_str(system_key, sizeof(system_key), target->id);
    }

    char ext_lower[16];
    file_ext_lower(fe->name, ext_lower, sizeof(ext_lower));
    bool matched_rule = false;
    const char* core = retro_resolve_core(&g_retro, system_key, ext_lower, &matched_rule);

    retro_log_line("rom=%s", rom_sd);
    retro_log_line("system=%s ext=%s core=%s matched=%d", system_key, ext_lower, core ? core : "(none)", matched_rule ? 1 : 0);

    if (!retro_retroarch_exists(&g_retro)) {
        snprintf(status_message, status_size, "RetroArch not found");
        retro_log_line("retroarch missing: %s", g_retro.retroarch_entry);
        if (status_timer) *status_timer = 120;
        return false;
    }

    bool known = false;
    bool available = false;
    retro_core_available(core, &known, &available);
    if (known && !available) {
        snprintf(status_message, status_size, "Required RetroArch core not available");
        retro_log_line("core missing: %s", core);
        if (status_timer) *status_timer = 120;
        return false;
    }
    if (!known) {
        retro_log_line("core availability unknown: %s", core);
        if (status_message && status_size > 0 && status_message[0] == 0) {
            snprintf(status_message, status_size, "Core availability unknown");
            if (status_timer) *status_timer = 90;
        }
    }

    if (!retro_write_launch(&g_retro, rom_sd, core, status_message, status_size)) {
        retro_log_line("launch.json write failed");
        if (status_timer) *status_timer = 120;
        return false;
    }
    retro_log_line("launch.json written");

    bool can_chainload = state && state->retro_chainload_enabled && retro_chainload_available();
    if (state && !state->retro_chainload_enabled) retro_log_line("chainload disabled");
    if (can_chainload) {
        if (retro_chainload(g_retro.retroarch_entry, status_message, status_size)) {
            retro_log_line("launch mode: chainload");
            save_emulators(&g_emu);
            save_state(state);
            if (state_dirty) *state_dirty = false;
            if (status_message && status_size > 0) snprintf(status_message, status_size, "Launching RetroArch...");
            if (status_timer) *status_timer = 60;
            g_exit_requested = true;
            return true;
        }
        retro_log_line("chainload failed");
        if (status_message[0] == 0) snprintf(status_message, status_size, "RetroArch launch failed");
        if (status_timer) *status_timer = 120;
        return false;
    }

    retro_log_line("launch mode: hbmenu");
    save_emulators(&g_emu);
    save_state(state);
    if (state_dirty) *state_dirty = false;
    g_exit_after_status = true;
    if (!known) snprintf(status_message, status_size, "Core unknown. Launch RetroArch from hbmenu");
    else snprintf(status_message, status_size, "Launch RetroArch from hbmenu to start the game");
    if (status_timer) *status_timer = 180;
    return true;
}

static void rebuild_targets_from_backend(Config* cfg, State* state, int* current_target, bool* state_dirty, char* status_message, size_t status_size, int* status_timer) {
    bool regen_rules = false;
    bool regen_emu = false;
    load_or_create_retro_rules(&g_retro, &regen_rules);
    load_or_create_emulators(&g_emu, &regen_emu);
    if (regen_rules) retro_log_line("retroarch_rules.json regenerated");
    if (regen_emu) retro_log_line("emulators.json regenerated");
    capture_base_targets(cfg);
    apply_emulator_targets(cfg);
    prune_state_targets(state, cfg);
    refresh_options_menu(cfg);
    if (regen_rules || regen_emu) {
        snprintf(status_message, status_size, "Retro defaults regenerated");
        if (status_timer) *status_timer = 120;
    }
    if (current_target) {
        int idx = find_target_index(cfg, state->last_target);
        if (idx < 0) {
            idx = 0;
            if (cfg->target_count > 0) copy_str(state->last_target, sizeof(state->last_target), cfg->targets[0].id);
        }
        *current_target = idx;
        if (cfg->target_count > 0 && idx >= 0 && idx < cfg->target_count) {
            Target* cur = &cfg->targets[idx];
            TargetState* ts = ensure_target_state(state, cfg, cur);
            if (ts) g_nds_banners = ts->nds_banner_mode != 0;
        }
    }
    if (state_dirty) *state_dirty = true;
}

static void refresh_options_menu(const Config* cfg) {
    scan_backgrounds(&g_state);
    g_option_count = 0;
    OptionItem* o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Rebuild NDS cache");
    o->action = OPTION_ACTION_REBUILD_NDS_CACHE;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Clear cache");
    o->action = OPTION_ACTION_CLEAR_CACHE;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Reload config");
    o->action = OPTION_ACTION_RELOAD_CONFIG;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Debug log: %s", debug_log_enabled() ? "On" : "Off");
    o->action = OPTION_ACTION_TOGGLE_DEBUG;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "NDS banners: %s", g_nds_banners ? "Title data" : "Sprite");
    o->action = OPTION_ACTION_TOGGLE_NDS_BANNERS;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Select NDS launcher");
    o->action = OPTION_ACTION_SELECT_LAUNCHER;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Themes...");
    o->action = OPTION_ACTION_THEME_MENU;

    char bg_label[96];
    background_label(g_top_bg_names[g_top_bg_index], bg_label, sizeof(bg_label));
    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Top background: %s", bg_label);
    o->action = OPTION_ACTION_TOP_BACKGROUND;

    background_label(g_bottom_bg_names[g_bottom_bg_index], bg_label, sizeof(bg_label));
    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Bottom background: %s", bg_label);
    o->action = OPTION_ACTION_BOTTOM_BACKGROUND;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Background visibility: %d%%", clamp_pct(g_state.background_visibility));
    o->action = OPTION_ACTION_BG_VISIBILITY;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "RetroArch log: %s", g_state.retro_log_enabled ? "On" : "Off");
    o->action = OPTION_ACTION_RETRO_LOG_TOGGLE;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "RetroArch chainload: %s", g_state.retro_chainload_enabled ? "On" : "Off");
    o->action = OPTION_ACTION_RETRO_CHAINLOAD_TOGGLE;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "RetroArch backend requirements");
    o->action = OPTION_ACTION_RETRO_INFO;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Emulators...");
    o->action = OPTION_ACTION_EMULATORS_MENU;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Select NTR launcher");
    o->action = OPTION_ACTION_SELECT_CARD_LAUNCHER;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Autoboot: Enabled");
    o->action = OPTION_ACTION_AUTOBOOT_STATUS;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "About: FirmMux");
    o->action = OPTION_ACTION_ABOUT;
}

static void handle_option_action(int idx, Config* cfg, State* state, int* current_target, bool* state_dirty, char* status_message, size_t status_size, int* status_timer, int* options_mode, int* options_selection, int* options_scroll) {
    if (idx < 0 || idx >= g_option_count) return;
    OptionAction action = g_options[idx].action;
    if (action == OPTION_ACTION_REBUILD_NDS_CACHE) {
        clear_dir_recursive(CACHE_NDS_DIR, false);
        mkdir(CACHE_NDS_DIR, 0777);
        g_nds_cache.count = 0;
        snprintf(status_message, status_size, "Rebuilding NDS cache");
    } else if (action == OPTION_ACTION_CLEAR_CACHE) {
        clear_dir_recursive(CACHE_DIR, false);
        ensure_dirs();
        g_nds_cache.count = 0;
        snprintf(status_message, status_size, "Cache cleared");
    } else if (action == OPTION_ACTION_RELOAD_CONFIG) {
        Config newcfg;
        if (load_or_create_config(&newcfg)) {
            *cfg = newcfg;
            g_base_target_count = 0;
            rebuild_targets_from_backend(cfg, state, current_target, state_dirty, status_message, status_size, status_timer);
            apply_theme_from_state_or_config(cfg, state);
            if (state_dirty) *state_dirty = true;
            snprintf(status_message, status_size, "Config reloaded");
        } else {
            snprintf(status_message, status_size, "Reload failed");
        }
    } else if (action == OPTION_ACTION_TOGGLE_DEBUG) {
        bool on = !debug_log_enabled();
        debug_set_enabled(on);
        refresh_options_menu(cfg);
        snprintf(status_message, status_size, "Debug log %s", on ? "On" : "Off");
    } else if (action == OPTION_ACTION_TOGGLE_NDS_BANNERS) {
        g_nds_banners = !g_nds_banners;
        refresh_options_menu(cfg);
        snprintf(status_message, status_size, "NDS banners %s", g_nds_banners ? "Title data" : "Sprite");
        TargetState* cur = get_target_state(state, cfg->targets[*current_target].id);
        if (cur) cur->nds_banner_mode = g_nds_banners ? 1 : 0;
        if (state_dirty) *state_dirty = true;
    } else if (action == OPTION_ACTION_SELECT_LAUNCHER) {
        LauncherCandidate cands[8];
        int found = find_launcher_candidates(cands, 8);
        if (found <= 0) {
            snprintf(status_message, status_size, "Launcher not found (CTR-P-FMBP)");
        } else {
            int nds_target = *current_target;
            for (int i = 0; i < cfg->target_count; i++) {
                if (!strcmp(cfg->targets[i].type, "rom_browser")) { nds_target = i; break; }
            }
            int pick = g_launcher_cycle % found;
            g_launcher_cycle++;
            LauncherCandidate* c = &cands[pick];
            char tid_hex[32];
            snprintf(tid_hex, sizeof(tid_hex), "%016llX", (unsigned long long)c->tid);
            TargetState* cur = get_target_state(state, cfg->targets[nds_target].id);
            if (cur) {
                copy_str(cur->loader_title_id, sizeof(cur->loader_title_id), tid_hex);
                copy_str(cur->loader_media, sizeof(cur->loader_media), media_to_string(c->media));
                if (state_dirty) *state_dirty = true;
            }
            Target* targ = &cfg->targets[nds_target];
            copy_str(targ->loader_title_id, sizeof(targ->loader_title_id), tid_hex);
            copy_str(targ->loader_media, sizeof(targ->loader_media), media_to_string(c->media));
            g_launcher_ready = true;
            g_launcher_tid = c->tid;
            g_launcher_media = c->media;
            snprintf(status_message, status_size, "Launcher set");
        }
    } else if (action == OPTION_ACTION_SELECT_CARD_LAUNCHER) {
        LauncherCandidate cands[8];
        int found = find_card_launcher_candidates(cands, 8);
        if (found <= 0) {
            snprintf(status_message, status_size, "NTR Launcher not found");
        } else {
            int nds_target = *current_target;
            for (int i = 0; i < cfg->target_count; i++) {
                if (!strcmp(cfg->targets[i].type, "rom_browser")) { nds_target = i; break; }
            }
            int pick = g_card_launcher_cycle % found;
            g_card_launcher_cycle++;
            LauncherCandidate* c = &cands[pick];
            char tid_hex[32];
            snprintf(tid_hex, sizeof(tid_hex), "%016llX", (unsigned long long)c->tid);
            TargetState* cur = get_target_state(state, cfg->targets[nds_target].id);
            if (cur) {
                copy_str(cur->card_launcher_title_id, sizeof(cur->card_launcher_title_id), tid_hex);
                copy_str(cur->card_launcher_media, sizeof(cur->card_launcher_media), media_to_string(c->media));
                if (state_dirty) *state_dirty = true;
            }
            Target* targ = &cfg->targets[nds_target];
            copy_str(targ->card_launcher_title_id, sizeof(targ->card_launcher_title_id), tid_hex);
            copy_str(targ->card_launcher_media, sizeof(targ->card_launcher_media), media_to_string(c->media));
            g_card_launcher_ready = true;
            g_card_launcher_tid = c->tid;
            g_card_launcher_media = c->media;
            snprintf(status_message, status_size, "NTR launcher set");
        }
    } else if (action == OPTION_ACTION_THEME_MENU) {
        scan_themes();
        const char* cur = state->theme[0] ? state->theme : (cfg->theme[0] ? cfg->theme : "default");
        build_theme_options(cur);
        if (options_mode) *options_mode = OPT_MODE_THEME;
        if (options_selection) *options_selection = 0;
        if (options_scroll) *options_scroll = 0;
    } else if (action == OPTION_ACTION_TOP_BACKGROUND) {
        build_background_options(true, state);
        if (options_mode) *options_mode = OPT_MODE_TOP_BG;
        if (options_selection) *options_selection = 0;
        if (options_scroll) *options_scroll = 0;
    } else if (action == OPTION_ACTION_BOTTOM_BACKGROUND) {
        build_background_options(false, state);
        if (options_mode) *options_mode = OPT_MODE_BOTTOM_BG;
        if (options_selection) *options_selection = 0;
        if (options_scroll) *options_scroll = 0;
    } else if (action == OPTION_ACTION_BG_VISIBILITY) {
        build_bg_visibility_options(state->background_visibility);
        if (options_mode) *options_mode = OPT_MODE_BG_VIS;
        if (options_selection) *options_selection = 0;
        if (options_scroll) *options_scroll = 0;
    } else if (action == OPTION_ACTION_RETRO_LOG_TOGGLE) {
        state->retro_log_enabled = !state->retro_log_enabled;
        retro_log_set_enabled(state->retro_log_enabled);
        refresh_options_menu(cfg);
        snprintf(status_message, status_size, "RetroArch log %s", state->retro_log_enabled ? "On" : "Off");
        if (state_dirty) *state_dirty = true;
    } else if (action == OPTION_ACTION_RETRO_CHAINLOAD_TOGGLE) {
        state->retro_chainload_enabled = !state->retro_chainload_enabled;
        refresh_options_menu(cfg);
        snprintf(status_message, status_size, "RetroArch chainload %s", state->retro_chainload_enabled ? "On" : "Off");
        if (state_dirty) *state_dirty = true;
    } else if (action == OPTION_ACTION_RETRO_INFO) {
        build_retro_info_options();
        if (options_mode) *options_mode = OPT_MODE_RETRO_INFO;
        if (options_selection) *options_selection = 0;
        if (options_scroll) *options_scroll = 0;
    } else if (action == OPTION_ACTION_EMULATORS_MENU) {
        build_emulator_options();
        if (options_mode) *options_mode = OPT_MODE_EMULATORS;
        if (options_selection) *options_selection = 0;
        if (options_scroll) *options_scroll = 0;
    } else if (action == OPTION_ACTION_AUTOBOOT_STATUS) {
        snprintf(status_message, status_size, "Autoboot enabled");
    } else if (action == OPTION_ACTION_ABOUT) {
        snprintf(status_message, status_size, "FirmMux Phase 2");
    }
    if (status_timer && status_message && status_message[0]) *status_timer = 90;
}

int main(int argc, char** argv) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(8192);
    C2D_Prepare();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    audio_init();
    ptmuInit();
    acInit();
    cfguInit();
    load_time_format();
    mcuHwcInit();
    g_font = C2D_FontLoadFromMem(comfortaa_bold_bcfnt, comfortaa_bold_bcfnt_size);
    if (g_font) C2D_FontSetFilter(g_font, GPU_LINEAR, GPU_LINEAR);

    g_textbuf = C2D_TextBufNew(4096);
    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    hidScanInput();
    if (hidKeysHeld() & KEY_B) {
    icon_free(&g_title_preview_icon);
    icon_free(&g_hb_preview_icon);
    if (g_font) C2D_FontFree(g_font);
    C2D_TextBufDelete(g_textbuf);
    C2D_Fini();
    C3D_Fini();
    mcuHwcExit();
    cfguExit();
    ptmuExit();
    acExit();
    ndspExit();
    fsExit();
    gfxExit();
    return 0;
}

    Config* cfg = &g_cfg;
    if (!load_or_create_config(cfg)) {
        gfxExit();
        return 1;
    }
    for (int i = 0; i < cfg->target_count; i++) {
        Target* t = &cfg->targets[i];
        if (t->id[0] == 0) {
            snprintf(t->id, sizeof(t->id), "%s", t->type[0] ? t->type : "target");
        }
        for (int j = 0; j < i; j++) {
            if (!strcmp(cfg->targets[j].id, t->id)) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%s_%d", t->id, i);
                copy_str(t->id, sizeof(t->id), buf);
                break;
            }
        }
        if (!strcmp(t->type, "rom_browser") && t->root[0] == 0) {
            snprintf(t->root, sizeof(t->root), "/roms/nds/");
        }
        if (!strcmp(t->type, "homebrew_browser") && t->root[0] == 0) {
            snprintf(t->root, sizeof(t->root), "/3ds/");
        }
        normalize_path(t->root);
        if (!strcmp(t->type, "rom_browser") && t->ext_count == 0) {
            if (!strcmp(t->id, "nds") || strstr(t->root, "/roms/nds")) {
                snprintf(t->extensions[0], sizeof(t->extensions[0]), ".nds");
                snprintf(t->extensions[1], sizeof(t->extensions[1]), ".dsi");
                t->ext_count = 2;
            }
        }
    }

    State* state = &g_state;
    if (!load_state(state)) {
        gfxExit();
        return 1;
    }

    if (!cfg->remember_last_position) {
        char saved_theme[32];
        char saved_top_bg[64];
        char saved_bottom_bg[64];
        int saved_vis = state->background_visibility;
        bool saved_retro_log = state->retro_log_enabled;
        bool saved_chainload = state->retro_chainload_enabled;
        copy_str(saved_theme, sizeof(saved_theme), state->theme);
        copy_str(saved_top_bg, sizeof(saved_top_bg), state->top_background);
        copy_str(saved_bottom_bg, sizeof(saved_bottom_bg), state->bottom_background);
        memset(state, 0, sizeof(*state));
        copy_str(state->last_target, sizeof(state->last_target), cfg->default_target);
        if (saved_theme[0]) copy_str(state->theme, sizeof(state->theme), saved_theme);
        if (saved_top_bg[0]) copy_str(state->top_background, sizeof(state->top_background), saved_top_bg);
        if (saved_bottom_bg[0]) copy_str(state->bottom_background, sizeof(state->bottom_background), saved_bottom_bg);
        state->background_visibility = saved_vis;
        state->retro_log_enabled = saved_retro_log;
        state->retro_chainload_enabled = saved_chainload;
    }

    if (state->background_visibility < 0 || state->background_visibility > 100) {
        state->background_visibility = 50;
    }

    retro_log_set_enabled(state->retro_log_enabled);

    char status_message[64] = {0};
    int status_timer = 0;
    bool state_dirty = false;

    int current_target = 0;
    g_base_target_count = 0;
    rebuild_targets_from_backend(cfg, state, &current_target, &state_dirty, status_message, sizeof(status_message), &status_timer);

    apply_theme_from_state_or_config(cfg, state);

    if (state->last_target[0] == 0) copy_str(state->last_target, sizeof(state->last_target), cfg->default_target);
    current_target = find_target_index(cfg, state->last_target);
    if (current_target < 0) {
        current_target = 0;
        if (cfg->target_count > 0) copy_str(state->last_target, sizeof(state->last_target), cfg->targets[0].id);
    }

    memset(g_runtimes, 0, sizeof(g_runtimes));

    for (int i = 0; i < cfg->target_count; i++) {
        Target* t = &cfg->targets[i];
        TargetState* ts = ensure_target_state(state, cfg, t);
        if (!ts) continue;
        if (ts && ts->path[0]) normalize_path(ts->path);
        if (ts && ts->path[0] == 0 && t->root[0]) {
            snprintf(ts->path, sizeof(ts->path), "%s", t->root);
            normalize_path(ts->path);
        }
        if (ts && !strcmp(t->type, "rom_browser")) {
            if (ts->path[0] == 0 || !path_has_prefix(ts->path, t->root)) {
                snprintf(ts->path, sizeof(ts->path), "%s", t->root[0] ? t->root : "/roms/nds/");
                normalize_path(ts->path);
                ts->selection = 0;
                ts->scroll = 0;
            }
        }
        if (ts && !strcmp(t->type, "homebrew_browser")) {
            if (ts->path[0] == 0 || !path_has_prefix(ts->path, t->root)) {
                snprintf(ts->path, sizeof(ts->path), "%s", t->root[0] ? t->root : "/3ds/");
                normalize_path(ts->path);
                ts->selection = 0;
                ts->scroll = 0;
            }
        }
        if (ts && is_emulator_target(t)) {
            if (ts->path[0] == 0 || !path_has_prefix(ts->path, t->root)) {
                snprintf(ts->path, sizeof(ts->path), "%s", t->root[0] ? t->root : "/");
                normalize_path(ts->path);
                ts->selection = 0;
                ts->scroll = 0;
            }
        }
        if (ts && ts->loader_title_id[0]) {
            copy_str(t->loader_title_id, sizeof(t->loader_title_id), ts->loader_title_id);
        }
        if (ts && ts->loader_media[0]) {
            copy_str(t->loader_media, sizeof(t->loader_media), ts->loader_media);
        }
        if (ts && ts->card_launcher_title_id[0]) {
            copy_str(t->card_launcher_title_id, sizeof(t->card_launcher_title_id), ts->card_launcher_title_id);
        }
        if (ts && ts->card_launcher_media[0]) {
            copy_str(t->card_launcher_media, sizeof(t->card_launcher_media), ts->card_launcher_media);
        }
    }

    bool options_open = false;
    int options_selection = 0;
    int options_scroll = 0;
    int hold_up = 0;
    int hold_down = 0;
    refresh_options_menu(cfg);
    u64 last_save_ms = osGetTime();
    int move_cooldown = 0;

    TargetState* cur_ts = ensure_target_state(state, cfg, &cfg->targets[current_target]);
    if (cur_ts) g_nds_banners = cur_ts->nds_banner_mode != 0;
    auto_set_launcher(cfg, state, &state_dirty, status_message, sizeof(status_message), &status_timer);
    auto_set_card_launcher(cfg, state, &state_dirty, status_message, sizeof(status_message), &status_timer);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        if (kHeld & KEY_UP) hold_up++; else hold_up = 0;
        if (kHeld & KEY_DOWN) hold_down++; else hold_down = 0;
        bool rep_up = (kDown & KEY_UP) || (hold_up > 10 && (hold_up % 2 == 0));
        bool rep_down = (kDown & KEY_DOWN) || (hold_down > 10 && (hold_down % 2 == 0));
        if (move_cooldown > 0) move_cooldown--;

        if (kDown & KEY_START) {
            options_open = !options_open;
            if (options_open) {
                g_options_mode = OPT_MODE_MAIN;
                refresh_options_menu(cfg);
            }
            audio_play(SOUND_TOGGLE);
        }

        if (cfg->target_count <= 0) {
            g_exit_requested = true;
            break;
        }
        if (current_target < 0 || current_target >= cfg->target_count) current_target = 0;

        Target* target = &cfg->targets[current_target];
        TargetState* ts = ensure_target_state(state, cfg, target);
        if (!ts) {
            snprintf(status_message, sizeof(status_message), "State full");
            status_timer = 120;
            g_exit_requested = true;
            break;
        }
        bool emu_root_exists = !is_emulator_target(target) || target_root_exists(target);

        if (!options_open) {
            if (kDown & KEY_L) {
                current_target = (current_target - 1 + cfg->target_count) % cfg->target_count;
                target = &cfg->targets[current_target];
                ts = ensure_target_state(state, cfg, target);
                if (!ts) {
                    snprintf(status_message, sizeof(status_message), "State full");
                    status_timer = 120;
                    g_exit_requested = true;
                    break;
                }
                snprintf(state->last_target, sizeof(state->last_target), "%s", target->id);
                if (ts) g_nds_banners = ts->nds_banner_mode != 0;
                state_dirty = true;
                audio_play(SOUND_MOVE);
            } else if (kDown & KEY_R) {
                current_target = (current_target + 1) % cfg->target_count;
                target = &cfg->targets[current_target];
                ts = ensure_target_state(state, cfg, target);
                if (!ts) {
                    snprintf(status_message, sizeof(status_message), "State full");
                    status_timer = 120;
                    g_exit_requested = true;
                    break;
                }
                snprintf(state->last_target, sizeof(state->last_target), "%s", target->id);
                if (ts) g_nds_banners = ts->nds_banner_mode != 0;
                state_dirty = true;
                audio_play(SOUND_MOVE);
            }
        }

        if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser") || is_emulator_target(target)) {
            DirCache* cache = &g_runtimes[current_target].cache;
            if (is_emulator_target(target) && !emu_root_exists) {
                cache->count = 0;
                cache->valid = true;
                copy_str(cache->path, sizeof(cache->path), ts->path);
                g_runtimes[current_target].root_missing = true;
            } else if (is_emulator_target(target)) {
                if (g_runtimes[current_target].root_missing || !cache_matches(cache, ts->path)) {
                    build_dir_cache(target, ts, cache);
                }
                g_runtimes[current_target].root_missing = false;
            } else if (!cache_matches(cache, ts->path)) {
                build_dir_cache(target, ts, cache);
            }
        }

        update_card_status();

        if (options_open) {
            int visible = (BOTTOM_H - HELP_BAR_H - 10) / g_list_item_h;
            int active_count = g_option_count;
            if (g_options_mode == OPT_MODE_THEME) active_count = g_theme_option_count;
            else if (g_options_mode == OPT_MODE_TOP_BG) active_count = g_top_bg_option_count;
            else if (g_options_mode == OPT_MODE_BOTTOM_BG) active_count = g_bottom_bg_option_count;
            else if (g_options_mode == OPT_MODE_BG_VIS) active_count = g_bg_vis_option_count;
            else if (g_options_mode == OPT_MODE_EMULATORS) active_count = g_emu_option_count;
            else if (g_options_mode == OPT_MODE_EMULATOR_DETAIL) active_count = g_emu_detail_count;
            else if (g_options_mode == OPT_MODE_RETRO_INFO) active_count = g_retro_info_count;
            if (active_count <= 0) active_count = 1;
            int prev = options_selection;
            if (rep_up) options_selection--;
            if (rep_down) options_selection++;
            if (options_selection < 0) options_selection = 0;
            if (options_selection >= active_count) options_selection = active_count - 1;
            if (kDown & KEY_L) options_selection -= visible;
            if (kDown & KEY_R) options_selection += visible;
            if (options_selection < 0) options_selection = 0;
            if (options_selection >= active_count) options_selection = active_count - 1;
            clamp_scroll_list(&options_scroll, options_selection, visible, active_count);
            if (options_selection != prev) audio_play(SOUND_MOVE);
            if (kDown & KEY_A) {
                if (g_options_mode == OPT_MODE_MAIN) {
                    handle_option_action(options_selection, cfg, state, &current_target, &state_dirty, status_message, sizeof(status_message), &status_timer, &g_options_mode, &options_selection, &options_scroll);
                    audio_play(SOUND_SELECT);
                } else if (g_options_mode == OPT_MODE_THEME) {
                    if (options_selection == 0) {
                        g_options_mode = OPT_MODE_MAIN;
                        options_selection = 0;
                        options_scroll = 0;
                        refresh_options_menu(cfg);
                        audio_play(SOUND_BACK);
                    } else {
                        int theme_idx = options_selection - 1;
                        if (theme_idx >= 0 && theme_idx < g_theme_name_count) {
                            const char* name = g_theme_names[theme_idx];
                            if (load_theme(&g_theme, name)) {
                                g_list_item_h = g_theme.list_item_h > 0 ? g_theme.list_item_h : 20;
                                g_line_spacing = g_theme.line_spacing > 0 ? g_theme.line_spacing : 26;
                                g_status_h = g_theme.status_h > 0 ? g_theme.status_h : 16;
                                copy_str(cfg->theme, sizeof(cfg->theme), name);
                                copy_str(state->theme, sizeof(state->theme), name);
                                state_dirty = true;
                                build_theme_options(name);
                                snprintf(status_message, sizeof(status_message), "Theme: %s", name);
                            } else {
                                snprintf(status_message, sizeof(status_message), "Theme load failed");
                            }
                            status_timer = 90;
                        }
                        audio_play(SOUND_SELECT);
                    }
                } else if (g_options_mode == OPT_MODE_TOP_BG || g_options_mode == OPT_MODE_BOTTOM_BG) {
                    bool top = (g_options_mode == OPT_MODE_TOP_BG);
                    if (options_selection == 0) {
                        g_options_mode = OPT_MODE_MAIN;
                        options_selection = 0;
                        options_scroll = 0;
                        refresh_options_menu(cfg);
                        audio_play(SOUND_BACK);
                    } else {
                        int idx = options_selection - 1;
                        set_background_from_index(top, idx, state, status_message, sizeof(status_message));
                        refresh_options_menu(cfg);
                        build_background_options(top, state);
                        options_selection = idx + 1;
                        clamp_scroll_list(&options_scroll, options_selection, visible, top ? g_top_bg_option_count : g_bottom_bg_option_count);
                        state_dirty = true;
                        status_timer = 90;
                        audio_play(SOUND_SELECT);
                    }
                } else if (g_options_mode == OPT_MODE_BG_VIS) {
                    if (options_selection == 0) {
                        g_options_mode = OPT_MODE_MAIN;
                        options_selection = 0;
                        options_scroll = 0;
                        refresh_options_menu(cfg);
                        audio_play(SOUND_BACK);
                    } else {
                        int idx = options_selection - 1;
                        set_bg_visibility_from_index(idx, state, status_message, sizeof(status_message));
                        refresh_options_menu(cfg);
                        build_bg_visibility_options(state->background_visibility);
                        options_selection = idx + 1;
                        clamp_scroll_list(&options_scroll, options_selection, visible, g_bg_vis_option_count);
                        state_dirty = true;
                        status_timer = 90;
                        audio_play(SOUND_SELECT);
                    }
                } else if (g_options_mode == OPT_MODE_EMULATORS) {
                    if (options_selection == 0) {
                        g_options_mode = OPT_MODE_MAIN;
                        options_selection = 0;
                        options_scroll = 0;
                        refresh_options_menu(cfg);
                        audio_play(SOUND_BACK);
                    } else {
                        int sys_idx = options_selection - 1;
                        build_emulator_detail_options(sys_idx);
                        g_options_mode = OPT_MODE_EMULATOR_DETAIL;
                        options_selection = 0;
                        options_scroll = 0;
                        audio_play(SOUND_SELECT);
                    }
                } else if (g_options_mode == OPT_MODE_EMULATOR_DETAIL) {
                    if (g_emu_detail_index < 0 || g_emu_detail_index >= g_emu.count) {
                        g_options_mode = OPT_MODE_EMULATORS;
                        build_emulator_options();
                        options_selection = 0;
                        options_scroll = 0;
                        audio_play(SOUND_BACK);
                    } else if (options_selection == 0) {
                        g_options_mode = OPT_MODE_EMULATORS;
                        build_emulator_options();
                        options_selection = g_emu_detail_index + 1;
                        options_scroll = 0;
                        clamp_scroll_list(&options_scroll, options_selection, visible, g_emu_option_count);
                        audio_play(SOUND_BACK);
                    } else if (options_selection == 1) {
                        EmuSystem* sys = &g_emu.systems[g_emu_detail_index];
                        sys->enabled = !sys->enabled;
                        save_emulators(&g_emu);
                        rebuild_targets_from_backend(cfg, state, &current_target, &state_dirty, status_message, sizeof(status_message), &status_timer);
                        build_emulator_options();
                        build_emulator_detail_options(g_emu_detail_index);
                        g_options_mode = OPT_MODE_EMULATOR_DETAIL;
                        options_selection = 1;
                        options_scroll = 0;
                        snprintf(status_message, sizeof(status_message), "%s %s", sys->display_name, sys->enabled ? "enabled" : "disabled");
                        status_timer = 90;
                        audio_play(SOUND_TOGGLE);
                    } else if (options_selection == 2) {
                        EmuSystem* sys = &g_emu.systems[g_emu_detail_index];
                        emu_cycle_rom_folder(sys);
                        save_emulators(&g_emu);
                        rebuild_targets_from_backend(cfg, state, &current_target, &state_dirty, status_message, sizeof(status_message), &status_timer);
                        build_emulator_options();
                        build_emulator_detail_options(g_emu_detail_index);
                        g_options_mode = OPT_MODE_EMULATOR_DETAIL;
                        options_selection = 2;
                        options_scroll = 0;
                        snprintf(status_message, sizeof(status_message), "ROM folder: %s", sys->rom_folder);
                        status_timer = 90;
                        audio_play(SOUND_SELECT);
                    }
                } else if (g_options_mode == OPT_MODE_RETRO_INFO) {
                    if (options_selection == 0) {
                        g_options_mode = OPT_MODE_MAIN;
                        options_selection = 0;
                        options_scroll = 0;
                        refresh_options_menu(cfg);
                        audio_play(SOUND_BACK);
                    }
                }
            }
            if (kDown & KEY_B) {
                if (g_options_mode == OPT_MODE_EMULATOR_DETAIL) {
                    g_options_mode = OPT_MODE_EMULATORS;
                    build_emulator_options();
                    options_selection = g_emu_detail_index + 1;
                    options_scroll = 0;
                    clamp_scroll_list(&options_scroll, options_selection, visible, g_emu_option_count);
                    audio_play(SOUND_BACK);
                } else if (g_options_mode != OPT_MODE_MAIN) {
                    g_options_mode = OPT_MODE_MAIN;
                    options_selection = 0;
                    options_scroll = 0;
                    refresh_options_menu(cfg);
                    audio_play(SOUND_BACK);
                } else {
                    options_open = false;
                    audio_play(SOUND_BACK);
                }
            }
        } else {
            if (!strcmp(target->type, "system_menu")) {
                ensure_titles_loaded(cfg);
                int total = title_count_system() + 1;
                int visible = (BOTTOM_H - HELP_BAR_H - 8) / g_list_item_h;
                int prev = ts->selection;
                if (rep_up) ts->selection--;
                if (rep_down) ts->selection++;
                if (ts->selection < 0) ts->selection = 0;
                if (ts->selection >= total && total > 0) ts->selection = total - 1;
                clamp_scroll_list(&ts->scroll, ts->selection, visible, total);
                if (rep_up || rep_down) state_dirty = true;
                if (ts->selection != prev) audio_play(SOUND_MOVE);
                if (kDown & KEY_A) {
                    if (ts->selection == 0) {
                        snprintf(status_message, sizeof(status_message), "Exiting...");
                        break;
                    } else {
                        TitleInfo3ds* tinfo = title_system_at(ts->selection - 1);
                        if (tinfo) {
                            if (launch_title_id(tinfo->titleId, tinfo->media, status_message, sizeof(status_message))) {
                                snprintf(status_message, sizeof(status_message), "Launching...");
                            } else if (status_message[0] == 0) {
                                snprintf(status_message, sizeof(status_message), "Launch failed");
                            }
                        }
                    }
                    status_timer = 60;
                    audio_play(SOUND_SELECT);
                }
            } else if (!strcmp(target->type, "installed_titles")) {
                ensure_titles_loaded(cfg);
                int total = title_count_user();
                int visible = (BOTTOM_H - HELP_BAR_H - 8) / g_list_item_h;
                int prev = ts->selection;
                if (rep_up) ts->selection--;
                if (rep_down) ts->selection++;
                if (ts->selection < 0) ts->selection = 0;
                if (ts->selection >= total && total > 0) ts->selection = total - 1;
                clamp_scroll_list(&ts->scroll, ts->selection, visible, total);
                if (rep_up || rep_down) state_dirty = true;
                if (ts->selection != prev) audio_play(SOUND_MOVE);
                if (kDown & KEY_A) {
                    TitleInfo3ds* tinfo = title_user_at(ts->selection);
                    if (tinfo) {
                        if (launch_title_id(tinfo->titleId, tinfo->media, status_message, sizeof(status_message))) {
                            snprintf(status_message, sizeof(status_message), "Launching...");
                        } else {
                            if (status_message[0] == 0) snprintf(status_message, sizeof(status_message), "Launch failed");
                        }
                        status_timer = 60;
                        audio_play(SOUND_SELECT);
                    }
                }
            } else if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser") || is_emulator_target(target)) {
            bool is_rom = !strcmp(target->type, "rom_browser");
            bool is_hb = !strcmp(target->type, "homebrew_browser");
            bool is_emu = is_emulator_target(target);
            DirCache* cache = &g_runtimes[current_target].cache;
            int visible = (BOTTOM_H - HELP_BAR_H - 8) / g_list_item_h;
            bool show_card = is_rom ? show_nds_card(target, ts) : false;
            int card_offset = (is_rom && show_card) ? 1 : 0;
            int total = (is_emu && !emu_root_exists) ? 0 : (cache->count + card_offset);
            int prev = ts->selection;
            bool moving = rep_up || rep_down;
            int step = 1;
            if (moving && (hold_up > 24 || hold_down > 24)) step = 2;
            if (rep_up) ts->selection -= step;
            if (rep_down) ts->selection += step;
                if (moving) move_cooldown = 8;
                if (ts->selection < 0) ts->selection = 0;
                if (total <= 0) ts->selection = 0;
                if (ts->selection >= total && total > 0) ts->selection = total - 1;
                clamp_scroll_list(&ts->scroll, ts->selection, visible, total);
                if (rep_up || rep_down) state_dirty = true;
                if (ts->selection != prev) audio_play(SOUND_MOVE);
                if (is_rom && !moving && move_cooldown == 0 && g_nds_banners) preload_nds_page(ts, cache, visible, NDS_PRELOAD_BUDGET, card_offset);
                if (is_rom && !moving && move_cooldown == 0 && g_nds_banners && cache->count > 0) {
                    int entry_idx = ts->selection - card_offset;
                    if (entry_idx >= 0 && entry_idx < cache->count && !cache->entries[entry_idx].is_dir) {
                    char joined[512];
                    path_join(ts->path, cache->entries[entry_idx].name, joined, sizeof(joined));
                    char sdpath[512];
                    make_sd_path(joined, sdpath, sizeof(sdpath));
                    if (is_nds_name(cache->entries[entry_idx].name)) build_nds_entry(sdpath);
                    }
                }
                if (kDown & KEY_B) {
                    char root[256];
                    snprintf(root, sizeof(root), "%s", target->root[0] ? target->root : "/");
                    normalize_path(root);
                    if (strcmp(ts->path, root)) {
                        path_parent(ts->path);
                        ts->selection = 0;
                        ts->scroll = 0;
                        build_dir_cache(target, ts, cache);
                        preload_nds_page(ts, cache, visible, NDS_PRELOAD_BUDGET, card_offset);
                        state_dirty = true;
                        audio_play(SOUND_BACK);
                    }
                }
                if (kDown & KEY_A && total > 0) {
                    if (is_rom && show_card && ts->selection == 0) {
                        if (launch_card_launcher(target, status_message, sizeof(status_message))) {
                            snprintf(status_message, sizeof(status_message), "Launching...");
                        } else if (status_message[0] == 0) {
                            snprintf(status_message, sizeof(status_message), "Launch failed");
                        }
                        audio_play(SOUND_SELECT);
                        status_timer = 60;
                    } else {
                        int entry_idx = ts->selection - card_offset;
                        if (entry_idx < 0) entry_idx = 0;
                        if (entry_idx >= cache->count) entry_idx = cache->count - 1;
                        FileEntry* fe = &cache->entries[entry_idx];
                        char joined[512];
                        path_join(ts->path, fe->name, joined, sizeof(joined));
                        if (fe->is_dir) {
                            copy_str(ts->path, sizeof(ts->path), joined);
                            ts->selection = 0;
                            ts->scroll = 0;
                            build_dir_cache(target, ts, cache);
                            state_dirty = true;
                            snprintf(status_message, sizeof(status_message), "Opening...");
                            audio_play(SOUND_OPEN);
                        } else {
                            if (is_rom && is_nds_name(fe->name)) {
                                char sdpath[512];
                                make_sd_path(joined, sdpath, sizeof(sdpath));
                                launch_nds_loader(target, sdpath, status_message, sizeof(status_message));
                            } else if (is_hb && is_3dsx_name(fe->name)) {
                                char sdpath[512];
                                make_sd_path(joined, sdpath, sizeof(sdpath));
                                if (homebrew_launch_3dsx(sdpath, status_message, sizeof(status_message))) {
                                    snprintf(status_message, sizeof(status_message), "Launching...");
                                    save_state(state);
                                    g_exit_requested = true;
                                } else if (status_message[0] == 0) {
                                    snprintf(status_message, sizeof(status_message), "Launch failed");
                                }
                            } else if (is_emu) {
                                retro_launch_selected(target, ts, fe, joined, state, status_message, sizeof(status_message), &status_timer, &state_dirty);
                            } else {
                                snprintf(status_message, sizeof(status_message), "Unsupported");
                            }
                            audio_play(SOUND_SELECT);
                        }
                        if (status_timer < 60) status_timer = 60;
                    }
                }
            }
        }

        if (g_exit_requested) break;
        if (status_timer > 0) status_timer--;
        if (status_timer == 0) {
            status_message[0] = 0;
            if (g_exit_after_status) break;
        }
        if (g_easter_timer > 0) g_easter_timer--;

        if (state_dirty) {
            u64 now = osGetTime();
            if (now - last_save_ms > 1000) {
                save_state(state);
                last_save_ms = now;
                state_dirty = false;
            }
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(g_top, g_theme.top_bg);
        C2D_TargetClear(g_bottom, g_theme.bottom_bg);

        C2D_SceneBegin(g_top);
        C2D_TextBufClear(g_textbuf);
        if (g_top_bg_tex.loaded) {
            draw_theme_image_scaled(&g_top_bg_tex, 0.0f, 0.0f, TOP_W, TOP_H);
        }

        bool top_has_bg = g_top_bg_tex.loaded;
        u32 panel_left = overlay_color(g_theme.panel_left, top_has_bg);
        u32 panel_right = overlay_color(g_theme.panel_right, top_has_bg);
        draw_rect(0, 0, PREVIEW_W, TOP_H, panel_left);
        draw_rect(PREVIEW_W, 0, TARGET_LIST_W, TOP_H, panel_right);

        const char* preview_title = "";
        const TitleInfo3ds* preview_tinfo = NULL;
        bool show_system_info = false;
        char preview_buf[64];
        if (!strcmp(target->type, "system_menu")) {
            if (ts->selection == 0) {
                preview_title = "Return to HOME";
                show_system_info = true;
            } else {
                TitleInfo3ds* tinfo = title_system_at(ts->selection - 1);
                if (tinfo) {
                    preview_title = tinfo->name;
                    preview_tinfo = tinfo;
                } else {
                    preview_title = "System Menu";
                }
            }
        } else if (!strcmp(target->type, "installed_titles")) {
            TitleInfo3ds* tinfo = title_user_at(ts->selection);
            if (tinfo) {
                preview_title = tinfo->name;
                preview_tinfo = tinfo;
            } else {
                snprintf(preview_buf, sizeof(preview_buf), "Title %03d", ts->selection + 1);
                preview_title = preview_buf;
            }
        } else if (!strcmp(target->type, "homebrew_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            if (cache->count > 0) {
                char joined[512];
                path_join(ts->path, cache->entries[ts->selection].name, joined, sizeof(joined));
                bool is_file = !cache->entries[ts->selection].is_dir;
                if (is_file && is_3dsx_name(cache->entries[ts->selection].name)) {
                    char sdpath[512];
                    make_sd_path(joined, sdpath, sizeof(sdpath));
                    update_homebrew_preview(sdpath);
                    if (g_hb_preview_title[0]) preview_title = g_hb_preview_title;
                    else preview_title = cache->entries[ts->selection].name;
                } else {
                    clear_hb_preview();
                    preview_title = cache->entries[ts->selection].name;
                }
            } else {
                preview_title = "Empty";
            }
        } else if (!strcmp(target->type, "rom_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            bool show_card = show_nds_card(target, ts);
            int card_offset = show_card ? 1 : 0;
            int total = cache->count + card_offset;
            if (total > 0) {
                if (show_card && ts->selection == 0) {
                    preview_title = g_card_twl_title[0] ? g_card_twl_title : "Game Card";
                } else {
                char joined[512];
                int entry_idx = ts->selection - card_offset;
                if (entry_idx < 0) entry_idx = 0;
                if (entry_idx >= cache->count) entry_idx = cache->count - 1;
                path_join(ts->path, cache->entries[entry_idx].name, joined, sizeof(joined));
                char sdpath[512];
                make_sd_path(joined, sdpath, sizeof(sdpath));
                bool is_file = !cache->entries[entry_idx].is_dir;
                bool is_nds = is_file && is_nds_name(cache->entries[entry_idx].name);
                NdsCacheEntry* nds = (is_nds ? nds_cache_entry(sdpath) : NULL);
                if (g_nds_banners && nds && nds->title[0]) preview_title = nds->title;
                if (preview_title[0] == 0) preview_title = cache->entries[entry_idx].name;
                }
            } else {
                preview_title = "Empty";
            }
        } else if (is_emulator_target(target)) {
            DirCache* cache = &g_runtimes[current_target].cache;
            if (!emu_root_exists) {
                preview_title = "Missing folder";
            } else if (cache->count <= 0) {
                preview_title = "No games found";
            } else {
                int entry_idx = ts->selection;
                if (entry_idx < 0) entry_idx = 0;
                if (entry_idx >= cache->count) entry_idx = cache->count - 1;
                FileEntry* fe = &cache->entries[entry_idx];
                if (fe->is_dir) {
                    preview_title = fe->name;
                } else {
                    base_name_no_ext(fe->name, preview_buf, sizeof(preview_buf));
                    if (preview_buf[0] == 0) copy_str(preview_buf, sizeof(preview_buf), fe->name);
                    preview_title = preview_buf;
                }
            }
        }

        float text_scale = 0.7f;
        int max_lines = 3;
        if (show_system_info) max_lines = 1;
        draw_wrap_text_limited(8, 8, text_scale, g_theme.text_primary, PREVIEW_W - 16, max_lines, preview_title);
        float banner_y = TOP_H - 96 - 20;
        bool drew_icon = false;
        if (show_system_info) {
            float info_y = 8 + ((float)g_line_spacing * text_scale * max_lines) + 8.0f;
            draw_system_info(8, info_y);
        } else {
            u32 preview_bg = overlay_color(g_theme.preview_bg, top_has_bg);
            draw_rect(8, banner_y, 96, 96, preview_bg);
            if (preview_tinfo) {
                char tidbuf[32];
                snprintf(tidbuf, sizeof(tidbuf), "TID: %016llX", (unsigned long long)preview_tinfo->titleId);
                draw_text(8, banner_y - 14, 0.45f, g_theme.text_secondary, tidbuf);
            }
        }
        if (!show_system_info && !strcmp(target->type, "homebrew_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            if (cache->count > 0) {
                FileEntry* fe = &cache->entries[ts->selection];
                if (!fe->is_dir && is_3dsx_name(fe->name) && g_hb_preview_valid) {
                    float scale = 2.0f;
                    float px = 8 + (96.0f - 48.0f * scale) * 0.5f;
                    float py = banner_y + (96.0f - 48.0f * scale) * 0.5f;
                    if (g_hb_preview_icon.loaded) {
                        C2D_DrawImageAt(g_hb_preview_icon.image, px + g_preview_offset_x, py + g_preview_offset_y, 0.0f, NULL, scale, scale);
                    } else {
                        draw_rgba_icon(8, banner_y, 2.0f, g_hb_preview_rgba, 48, 48);
                    }
                    drew_icon = true;
                }
            }
        } else if (!show_system_info && !strcmp(target->type, "rom_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            bool show_card = show_nds_card(target, ts);
            if (cache->count > 0 || show_card) {
                int card_offset = show_card ? 1 : 0;
                if (show_card && ts->selection == 0) {
                    if (g_card_twl_has_icon) {
                        draw_rgba_icon(8, banner_y, 3.0f, g_card_twl_rgba, 32, 32);
                        drew_icon = true;
                    }
                } else {
                    int entry_idx = ts->selection - card_offset;
                    if (entry_idx < 0) entry_idx = 0;
                    if (entry_idx >= cache->count) entry_idx = cache->count - 1;
                    char joined[512];
                    path_join(ts->path, cache->entries[entry_idx].name, joined, sizeof(joined));
                    char sdpath[512];
                    make_sd_path(joined, sdpath, sizeof(sdpath));
                    bool is_file = !cache->entries[entry_idx].is_dir;
                    bool is_nds = is_file && is_nds_name(cache->entries[entry_idx].name);
                    NdsCacheEntry* nds = is_nds ? nds_cache_entry(sdpath) : NULL;
                    if (g_nds_banners && nds && nds->has_rgba) {
                        draw_rgba_icon(8, banner_y, 3.0f, nds->rgba, 32, 32);
                        drew_icon = true;
                    } else if (!g_nds_banners) {
                        if (nds && nds->has_rgba) {
                            draw_rgba_icon(8, banner_y, 3.0f, nds->rgba, 32, 32);
                            drew_icon = true;
                        } else if (move_cooldown == 0 && is_nds) {
                            build_nds_entry(sdpath);
                        }
                        if (!drew_icon) {
                            if (g_theme.sprite_loaded && g_theme.sprite_w > 0 && g_theme.sprite_h > 0) {
                                int maxd = g_theme.sprite_w > g_theme.sprite_h ? g_theme.sprite_w : g_theme.sprite_h;
                                float scale = 96.0f / (float)maxd;
                                float px = 8 + (96.0f - g_theme.sprite_w * scale) * 0.5f;
                                float py = banner_y + (96.0f - g_theme.sprite_h * scale) * 0.5f;
                                draw_theme_image(&g_theme.sprite_tex, px, py, scale);
                            } else {
                                u32 col1 = hash_color(cache->entries[entry_idx].name);
                                u32 col2 = hash_color(cache->entries[entry_idx].name + 1);
                                u8 tmp[32 * 32 * 4];
                                make_sprite(tmp, col1, col2);
                                draw_rgba_icon(8, banner_y, 3.0f, tmp, 32, 32);
                            }
                            drew_icon = true;
                        }
                    }
                }
            }
        } else if (!show_system_info && !strcmp(target->type, "installed_titles")) {
            TitleInfo3ds* tinfo = title_user_at(ts->selection);
            if (update_title_preview_rgba(tinfo)) {
                float scale = 2.0f;
                float px = 8 + (96.0f - 48.0f * scale) * 0.5f;
                float py = banner_y + (96.0f - 48.0f * scale) * 0.5f;
                if (g_title_preview_icon.loaded) {
                    C2D_DrawImageAt(g_title_preview_icon.image, px + g_preview_offset_x, py + g_preview_offset_y, 0.0f, NULL, scale, scale);
                } else {
                    draw_rgba_icon(8, banner_y, 2.0f, g_title_preview_rgba, 48, 48);
                }
                drew_icon = true;
            }
        } else if (!show_system_info && !strcmp(target->type, "system_menu")) {
            if (ts->selection > 0) {
                TitleInfo3ds* tinfo = title_system_at(ts->selection - 1);
                if (update_title_preview_rgba(tinfo)) {
                    float scale = 2.0f;
                    float px = 8 + (96.0f - 48.0f * scale) * 0.5f;
                    float py = banner_y + (96.0f - 48.0f * scale) * 0.5f;
                    if (g_title_preview_icon.loaded) {
                        C2D_DrawImageAt(g_title_preview_icon.image, px + g_preview_offset_x, py + g_preview_offset_y, 0.0f, NULL, scale, scale);
                    } else {
                        draw_rgba_icon(8, banner_y, 2.0f, g_title_preview_rgba, 48, 48);
                    }
                    drew_icon = true;
                }
            }
        }
        if (!show_system_info && g_theme.preview_frame_loaded) {
            float off = align_offset_from_center(g_theme.preview_frame_center_y, 96.0f);
            u8 alpha = overlay_alpha(top_has_bg);
            draw_theme_image_scaled_alpha(&g_theme.preview_frame_tex, 8.0f, banner_y + off, 96.0f, 96.0f, alpha);
        }
        if (!show_system_info && !drew_icon) {
            draw_text(12, banner_y + 36, 0.6f, g_theme.text_muted, "Preview");
        }
        if (g_easter_timer > 0 && g_easter_loaded && g_easter_rgba) {
            float max_w = PREVIEW_W - 16;
            float max_h = TOP_H - 40;
            float scale_x = max_w / (float)g_easter_w;
            float scale_y = max_h / (float)g_easter_h;
            float scale = scale_x < scale_y ? scale_x : scale_y;
            if (scale > 2.0f) scale = 2.0f;
            float draw_w = g_easter_w * scale;
            float draw_h = g_easter_h * scale;
            float ex = (PREVIEW_W - draw_w) * 0.5f;
            float ey = (TOP_H - draw_h) * 0.5f;
            draw_rgba_icon(ex, ey, scale, g_easter_rgba, g_easter_w, g_easter_h);
        }

        int target_visible = (TOP_H - g_status_h) / g_list_item_h;
        int target_scroll = 0;
        clamp_scroll_list(&target_scroll, current_target, target_visible, cfg->target_count);
        u8 tab_alpha = overlay_alpha(top_has_bg);
        for (int i = 0; i < target_visible; i++) {
            int idx = target_scroll + i;
            if (idx >= cfg->target_count) break;
            int y = i * g_list_item_h + 2;
            int row_y = y + 1;
            int row_h = g_list_item_h - 2;
            bool sel = (idx == current_target);
            if (sel && g_theme.tab_sel_loaded) {
                float off = align_offset_from_center(g_theme.tab_sel_center_y, row_h);
                draw_theme_image_scaled_alpha(&g_theme.tab_sel_tex, PREVIEW_W + 2, row_y + g_theme.tab_item_offset_y + off, TARGET_LIST_W - 4, row_h, tab_alpha);
            } else if (!sel && g_theme.tab_item_loaded) {
                float off = align_offset_from_center(g_theme.tab_item_center_y, row_h);
                draw_theme_image_scaled_alpha(&g_theme.tab_item_tex, PREVIEW_W + 2, row_y + g_theme.tab_item_offset_y + off, TARGET_LIST_W - 4, row_h, tab_alpha);
            } else {
                u32 color = overlay_color(sel ? g_theme.tab_sel : g_theme.tab_bg, top_has_bg);
                draw_rect(PREVIEW_W + 2, row_y, TARGET_LIST_W - 4, row_h, color);
            }
            float tab_bias = -2.0f;
            if (sel && g_theme.tab_sel_loaded) tab_bias = align_offset_from_center(g_theme.tab_sel_center_y, row_h);
            else if (!sel && g_theme.tab_item_loaded) tab_bias = align_offset_from_center(g_theme.tab_item_center_y, row_h);
            draw_text_centered_bias(PREVIEW_W + 6, row_y + g_theme.tab_text_offset_y, 0.7f, g_theme.tab_text, row_h, cfg->targets[idx].label, tab_bias);
        }

        draw_status_bar();

        C2D_SceneBegin(g_bottom);
        C2D_TextBufClear(g_textbuf);
        if (g_bottom_bg_tex.loaded) {
            draw_theme_image_scaled(&g_bottom_bg_tex, 0.0f, 0.0f, BOTTOM_W, BOTTOM_H);
        }
        bool bottom_has_bg = g_bottom_bg_tex.loaded;
        u8 bottom_alpha = overlay_alpha(bottom_has_bg);

        if (options_open) {
            OptionItem* list = g_options;
            int count = g_option_count;
            const char* header = "Options";
            if (g_options_mode == OPT_MODE_THEME) {
                list = g_theme_options;
                count = g_theme_option_count;
                header = "Themes";
            } else if (g_options_mode == OPT_MODE_TOP_BG) {
                list = g_top_bg_options;
                count = g_top_bg_option_count;
                header = "Top background";
            } else if (g_options_mode == OPT_MODE_BOTTOM_BG) {
                list = g_bottom_bg_options;
                count = g_bottom_bg_option_count;
                header = "Bottom background";
            } else if (g_options_mode == OPT_MODE_BG_VIS) {
                list = g_bg_vis_options;
                count = g_bg_vis_option_count;
                header = "Background visibility";
            } else if (g_options_mode == OPT_MODE_EMULATORS) {
                list = g_emu_options;
                count = g_emu_option_count;
                header = "Emulators";
            } else if (g_options_mode == OPT_MODE_EMULATOR_DETAIL) {
                list = g_emu_detail_options;
                count = g_emu_detail_count;
                header = "Emulator";
                if (g_emu_detail_index >= 0 && g_emu_detail_index < g_emu.count) {
                    header = g_emu.systems[g_emu_detail_index].display_name;
                }
            } else if (g_options_mode == OPT_MODE_RETRO_INFO) {
                list = g_retro_info_options;
                count = g_retro_info_count;
                header = "RetroArch backend requirements";
            }
            draw_rect(0, 0, BOTTOM_W, BOTTOM_H, overlay_color(g_theme.overlay_bg, bottom_has_bg));
            draw_text(8, 6, 0.7f, g_theme.option_header, header);
            int visible = (BOTTOM_H - HELP_BAR_H - 20) / g_list_item_h;
            for (int i = 0; i < visible; i++) {
                int idx = options_scroll + i;
                if (idx >= count) break;
                int y = 24 + i * g_list_item_h;
                bool sel = (idx == options_selection);
                if (sel && g_theme.option_sel_loaded) {
                    float off = align_offset_from_center(g_theme.option_sel_center_y, g_list_item_h);
                    draw_theme_image_scaled_alpha(&g_theme.option_sel_tex, 6, y + g_theme.option_item_offset_y + off, BOTTOM_W - 12, g_list_item_h, bottom_alpha);
                } else if (!sel && g_theme.option_item_loaded) {
                    float off = align_offset_from_center(g_theme.option_item_center_y, g_list_item_h);
                    draw_theme_image_scaled_alpha(&g_theme.option_item_tex, 6, y + g_theme.option_item_offset_y + off, BOTTOM_W - 12, g_list_item_h, bottom_alpha);
                } else {
                    u32 color = overlay_color(sel ? g_theme.option_sel : g_theme.option_bg, bottom_has_bg);
                    draw_rect(6, y, BOTTOM_W - 12, g_list_item_h, color);
                }
                float opt_bias = -2.0f;
                if (sel && g_theme.option_sel_loaded) opt_bias = align_offset_from_center(g_theme.option_sel_center_y, g_list_item_h);
                else if (!sel && g_theme.option_item_loaded) opt_bias = align_offset_from_center(g_theme.option_item_center_y, g_list_item_h);
                draw_text_centered_bias(10, y + g_theme.option_text_offset_y, 0.6f, g_theme.option_text, g_list_item_h, list[idx].label, opt_bias);
            }
            if (kDown & KEY_SELECT) {
                if (!g_select_last) g_select_hits++;
                g_select_last = true;
            } else {
                g_select_last = false;
            }
            if (g_select_hits >= 3) {
                g_select_hits = 0;
                if (!g_easter_loaded) {
                    u8* raw = NULL;
                    unsigned rw = 0, rh = 0;
                    decode_jpeg_rgba(dikbutt_jpg, dikbutt_jpg_len, &raw, &rw, &rh);
                    if (raw) {
                        if (g_easter_rgba) free(g_easter_rgba);
                        g_easter_rgba = downscale_rgba_nearest(raw, rw, rh, 64, &g_easter_w, &g_easter_h);
                        free(raw);
                        g_easter_loaded = g_easter_rgba != NULL;
                    }
                }
                if (g_easter_loaded) g_easter_timer = 180;
            }
        } else if (!strcmp(target->type, "installed_titles")) {
            ensure_titles_loaded(cfg);
            int total = title_count_user();
            int visible = (BOTTOM_H - HELP_BAR_H - 8) / g_list_item_h;
            for (int i = 0; i < visible; i++) {
                int idx = ts->scroll + i;
                if (idx >= total) break;
                int y = 6 + i * g_list_item_h;
                bool sel = (idx == ts->selection);
                if (sel && g_theme.list_sel_loaded) {
                    float off = align_offset_from_center(g_theme.list_sel_center_y, g_list_item_h);
                    draw_theme_image_scaled_alpha(&g_theme.list_sel_tex, 6, y + g_theme.list_item_offset_y + off, BOTTOM_W - 12, g_list_item_h, bottom_alpha);
                } else if (!sel && g_theme.list_item_loaded) {
                    float off = align_offset_from_center(g_theme.list_item_center_y, g_list_item_h);
                    draw_theme_image_scaled_alpha(&g_theme.list_item_tex, 6, y + g_theme.list_item_offset_y + off, BOTTOM_W - 12, g_list_item_h, bottom_alpha);
                } else {
                    u32 color = overlay_color(sel ? g_theme.list_sel : g_theme.list_bg, bottom_has_bg);
                    draw_rect(6, y, BOTTOM_W - 12, g_list_item_h, color);
                }
                char shortname[56];
                TitleInfo3ds* t = title_user_at(idx);
                if (!t) continue;
                copy_str(shortname, sizeof(shortname), t->name);
                if (strlen(shortname) > 30) {
                    shortname[27] = '.';
                    shortname[28] = '.';
                    shortname[29] = '.';
                    shortname[30] = 0;
                }
                float list_bias = -2.0f;
                if (sel && g_theme.list_sel_loaded) list_bias = align_offset_from_center(g_theme.list_sel_center_y, g_list_item_h);
                else if (!sel && g_theme.list_item_loaded) list_bias = align_offset_from_center(g_theme.list_item_center_y, g_list_item_h);
                draw_text_centered_bias(12, y + g_theme.list_text_offset_y, 0.6f, g_theme.list_text, g_list_item_h, shortname, list_bias);
            }
        } else if (!strcmp(target->type, "system_menu")) {
            ensure_titles_loaded(cfg);
            int total = title_count_system() + 1;
            int visible = (BOTTOM_H - HELP_BAR_H - 8) / g_list_item_h;
            for (int i = 0; i < visible; i++) {
                int idx = ts->scroll + i;
                if (idx >= total) break;
                int y = 6 + i * g_list_item_h;
                bool sel = (idx == ts->selection);
                if (sel && g_theme.list_sel_loaded) {
                    float off = align_offset_from_center(g_theme.list_sel_center_y, g_list_item_h);
                    draw_theme_image_scaled_alpha(&g_theme.list_sel_tex, 6, y + g_theme.list_item_offset_y + off, BOTTOM_W - 12, g_list_item_h, bottom_alpha);
                } else if (!sel && g_theme.list_item_loaded) {
                    float off = align_offset_from_center(g_theme.list_item_center_y, g_list_item_h);
                    draw_theme_image_scaled_alpha(&g_theme.list_item_tex, 6, y + g_theme.list_item_offset_y + off, BOTTOM_W - 12, g_list_item_h, bottom_alpha);
                } else {
                    u32 color = overlay_color(sel ? g_theme.list_sel : g_theme.list_bg, bottom_has_bg);
                    draw_rect(6, y, BOTTOM_W - 12, g_list_item_h, color);
                }
                if (idx == 0) {
                    float list_bias = -2.0f;
                    if (sel && g_theme.list_sel_loaded) list_bias = align_offset_from_center(g_theme.list_sel_center_y, g_list_item_h);
                    else if (!sel && g_theme.list_item_loaded) list_bias = align_offset_from_center(g_theme.list_item_center_y, g_list_item_h);
                    draw_text_centered_bias(12, y + g_theme.list_text_offset_y, 0.6f, g_theme.list_text, g_list_item_h, "Return to HOME", list_bias);
                } else {
                    TitleInfo3ds* t = title_system_at(idx - 1);
                    if (!t) continue;
                    char shortname[56];
                    copy_str(shortname, sizeof(shortname), t->name);
                    if (strlen(shortname) > 30) {
                        shortname[27] = '.';
                        shortname[28] = '.';
                        shortname[29] = '.';
                        shortname[30] = 0;
                    }
                    float list_bias = -2.0f;
                    if (sel && g_theme.list_sel_loaded) list_bias = align_offset_from_center(g_theme.list_sel_center_y, g_list_item_h);
                    else if (!sel && g_theme.list_item_loaded) list_bias = align_offset_from_center(g_theme.list_item_center_y, g_list_item_h);
                    draw_text_centered_bias(12, y + g_theme.list_text_offset_y, 0.6f, g_theme.list_text, g_list_item_h, shortname, list_bias);
                }
            }
        } else if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser") || is_emulator_target(target)) {
            bool is_rom = !strcmp(target->type, "rom_browser");
            bool is_emu = is_emulator_target(target);
            bool root_ok = !is_emu || emu_root_exists;
            DirCache* cache = &g_runtimes[current_target].cache;
            int visible = (BOTTOM_H - HELP_BAR_H - 8) / g_list_item_h;
            bool show_card = is_rom ? show_nds_card(target, ts) : false;
            int card_offset = (is_rom && show_card) ? 1 : 0;
            int total = root_ok ? (cache->count + card_offset) : 0;
            int list_x = 6;
            int list_y = 6;
            int list_w = BOTTOM_W - 12;
            int list_h = BOTTOM_H - HELP_BAR_H - 8;
            u32 border = overlay_color(g_theme.help_line, bottom_has_bg);
            draw_rect(list_x, list_y, list_w, 1, border);
            draw_rect(list_x, list_y + list_h - 1, list_w, 1, border);
            draw_rect(list_x, list_y, 1, list_h, border);
            draw_rect(list_x + list_w - 1, list_y, 1, list_h, border);
            if (!root_ok) {
                draw_text(18, list_y + list_h * 0.5f - 8.0f, 0.7f, g_theme.text_secondary, "Missing folder");
            } else if (total <= 0) {
                draw_text(18, list_y + list_h * 0.5f - 8.0f, 0.7f, g_theme.text_secondary, "No games found");
            } else {
            for (int i = 0; i < visible; i++) {
                int idx = ts->scroll + i;
                if (idx >= total) break;
                int y = 6 + i * g_list_item_h;
                bool sel = (idx == ts->selection);
                if (sel && g_theme.list_sel_loaded) {
                    float off = align_offset_from_center(g_theme.list_sel_center_y, g_list_item_h);
                    draw_theme_image_scaled_alpha(&g_theme.list_sel_tex, 6, y + g_theme.list_item_offset_y + off, BOTTOM_W - 12, g_list_item_h, bottom_alpha);
                } else if (!sel && g_theme.list_item_loaded) {
                    float off = align_offset_from_center(g_theme.list_item_center_y, g_list_item_h);
                    draw_theme_image_scaled_alpha(&g_theme.list_item_tex, 6, y + g_theme.list_item_offset_y + off, BOTTOM_W - 12, g_list_item_h, bottom_alpha);
                } else {
                    u32 color = overlay_color(sel ? g_theme.list_sel : g_theme.list_bg, bottom_has_bg);
                    draw_rect(6, y, BOTTOM_W - 12, g_list_item_h, color);
                }
                char label_buf[256];
                if (card_offset && idx == 0) {
                    if (g_card_twl_title[0]) copy_str(label_buf, sizeof(label_buf), g_card_twl_title);
                    else copy_str(label_buf, sizeof(label_buf), "Game Card");
                } else {
                    int entry_idx = idx - card_offset;
                    if (entry_idx < 0 || entry_idx >= cache->count) continue;
                    if (cache->entries[entry_idx].is_dir) {
                        snprintf(label_buf, sizeof(label_buf), "%s/", cache->entries[entry_idx].name);
                    } else {
                        base_name_no_ext(cache->entries[entry_idx].name, label_buf, sizeof(label_buf));
                        if (label_buf[0] == 0) copy_str(label_buf, sizeof(label_buf), cache->entries[entry_idx].name);
                    }
                }
                float text_x = 10.0f;
                float list_bias = -2.0f;
                if (sel && g_theme.list_sel_loaded) list_bias = align_offset_from_center(g_theme.list_sel_center_y, g_list_item_h);
                else if (!sel && g_theme.list_item_loaded) list_bias = align_offset_from_center(g_theme.list_item_center_y, g_list_item_h);
                draw_text_centered_bias(text_x, y + g_theme.list_text_offset_y, 0.6f, g_theme.list_text, g_list_item_h, label_buf, list_bias);
            }
            }
        } else {
            draw_text(8, 10, 0.7f, g_theme.text_primary, "System Menu");
            draw_text(8, 30, 0.6f, g_theme.text_muted, "Press A to exit");
        }

        if (cfg->help_bar) {
            const char* help = "A Launch   B Back   X Sort   Y Search";
            if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser") || is_emulator_target(target)) {
                bool is_rom = !strcmp(target->type, "rom_browser");
                bool is_emu = is_emulator_target(target);
                bool root_ok = !is_emu || emu_root_exists;
                DirCache* cache = &g_runtimes[current_target].cache;
                bool show_card = is_rom ? show_nds_card(target, ts) : false;
                int card_offset = (is_rom && show_card) ? 1 : 0;
                int total = root_ok ? (cache->count + card_offset) : 0;
                if (total > 0) {
                    int entry_idx = ts->selection - card_offset;
                    if (entry_idx >= 0 && entry_idx < cache->count && cache->entries[entry_idx].is_dir) {
                        help = "A Open   B Back   X Sort   Y Search";
                    }
                }
            }
            draw_help_bar(help);
        }

        if (status_message[0]) {
            draw_rect(60, 90, 200, 40, g_theme.toast_bg);
            draw_text(90, 104, 0.6f, g_theme.toast_text, status_message);
        }

        C3D_FrameEnd(0);
    }

    save_state(state);

    icon_free(&g_top_bg_tex);
    icon_free(&g_bottom_bg_tex);
    icon_free(&g_title_preview_icon);
    icon_free(&g_hb_preview_icon);
    if (g_font) C2D_FontFree(g_font);
    C2D_TextBufDelete(g_textbuf);
    C2D_Fini();
    C3D_Fini();
    ndspExit();
    fsExit();
    gfxExit();
    return 0;
}
