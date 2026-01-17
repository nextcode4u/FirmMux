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
#include <3ds/ndsp/ndsp.h>
#include <3ds/services/apt.h>

static OptionItem g_options[MAX_OPTIONS];
static int g_option_count = 0;
static bool g_nds_banners = false;
static int g_launcher_cycle = 0;

static TargetRuntime g_runtimes[MAX_TARGETS];
static Config g_cfg;
static State g_state;

static C2D_TextBuf g_textbuf;
static C3D_RenderTarget* g_top;
static C3D_RenderTarget* g_bottom;

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
            if (strcmp(prod, "CTR-H-FMUX") != 0) continue;
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
    if (state_dirty) *state_dirty = true;
    if (status_message && status_size > 0) {
        snprintf(status_message, status_size, "Launcher set");
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
    (void)target;
    char norm[512];
    snprintf(norm, sizeof(norm), "%s", sd_path ? sd_path : "");
    normalize_path_to_sd_colon(norm, sizeof(norm));
    if (!write_launch_txt_for_nds(norm)) {
        snprintf(status_message, status_size, "launch.txt failed");
        return false;
    }
    u64 tid = FMUX_BOOTSTRAP_TITLEID;
    FS_MediaType media = MEDIATYPE_SD;
    if (target && target->loader_title_id[0]) parse_title_id(target->loader_title_id, &tid);
    if (target && target->loader_media[0]) media = media_from_string(target->loader_media);
    if (launch_title_id(tid, media, status_message, status_size)) return true;
    FS_MediaType alt = (media == MEDIATYPE_NAND) ? MEDIATYPE_SD : MEDIATYPE_NAND;
    if (launch_title_id(tid, alt, status_message, status_size)) return true;
    if (status_message && status_size > 0) snprintf(status_message, status_size, "Install FirmMuxBootstrapLauncher (ID 00040000FF401000)");
    return false;
}

static void draw_text(float x, float y, float scale, u32 color, const char* str) {
    C2D_Text text;
    C2D_TextParse(&text, g_textbuf, str);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, color);
}

static void draw_rgba_icon(float x, float y, float scale, const u8* rgba) {
    if (!rgba) return;
    float step = scale;
    for (int iy = 0; iy < 32; iy++) {
        for (int ix = 0; ix < 32; ix++) {
            const u8* p = rgba + (iy * 32 + ix) * 4;
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
            C2D_TextParse(&text, g_textbuf, line);
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
            C2D_TextParse(&text, g_textbuf, line);
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
        line_y += 26 * scale;
        lines++;
    }
}

static void draw_rect(float x, float y, float w, float h, u32 color) {
    C2D_DrawRectSolid(x, y, 0.0f, w, h, color);
}

static void draw_help_bar(const char* label) {
    draw_rect(0, BOTTOM_H - HELP_BAR_H, BOTTOM_W, HELP_BAR_H, C2D_Color32(20, 20, 20, 255));
    draw_text(6, BOTTOM_H - HELP_BAR_H + 2, 0.5f, C2D_Color32(220, 220, 220, 255), label);
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

static void preload_nds_page(const TargetState* ts, const DirCache* cache, int visible, int budget) {
    int done = 0;
    for (int i = 0; i < visible && done < budget; i++) {
        int idx = ts->scroll + i;
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

static void refresh_options_menu(const Config* cfg) {
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

    bool show_sys = false;
    for (int i = 0; i < cfg->target_count; i++) {
        if (!strcmp(cfg->targets[i].type, "installed_titles")) {
            show_sys = cfg->targets[i].show_system_titles;
            break;
        }
    }
    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "Show system titles: %s", show_sys ? "On" : "Off");
    o->action = OPTION_ACTION_TOGGLE_SYSTEM;

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
    snprintf(o->label, sizeof(o->label), "Autoboot: Enabled");
    o->action = OPTION_ACTION_AUTOBOOT_STATUS;

    o = &g_options[g_option_count++];
    snprintf(o->label, sizeof(o->label), "About: FirmMux");
    o->action = OPTION_ACTION_ABOUT;
}

static void handle_option_action(int idx, Config* cfg, State* state, int* current_target, bool* state_dirty, char* status_message, size_t status_size, int* status_timer) {
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
            refresh_options_menu(cfg);
            int idx_target = find_target_index(cfg, state->last_target);
            if (idx_target < 0) {
                idx_target = 0;
                snprintf(state->last_target, sizeof(state->last_target), "%s", cfg->targets[0].id);
            }
            *current_target = idx_target;
            if (state_dirty) *state_dirty = true;
            snprintf(status_message, status_size, "Config reloaded");
        } else {
            snprintf(status_message, status_size, "Reload failed");
        }
    } else if (action == OPTION_ACTION_TOGGLE_SYSTEM) {
        bool new_state = false;
        for (int i = 0; i < cfg->target_count; i++) {
            if (!strcmp(cfg->targets[i].type, "installed_titles")) {
                cfg->targets[i].show_system_titles = !cfg->targets[i].show_system_titles;
                new_state = cfg->targets[i].show_system_titles;
                break;
            }
        }
        refresh_options_menu(cfg);
        snprintf(status_message, status_size, "System titles %s", new_state ? "On" : "Off");
        g_title_catalog.count = 0;
        if (state_dirty) *state_dirty = true;
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
            snprintf(status_message, status_size, "Launcher not found (CTR-H-FMUX)");
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
            snprintf(status_message, status_size, "Launcher set");
        }
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
    audio_init();

    g_textbuf = C2D_TextBufNew(4096);
    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    hidScanInput();
    if (hidKeysHeld() & KEY_B) {
    C2D_TextBufDelete(g_textbuf);
    C2D_Fini();
    C3D_Fini();
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
        if (!strcmp(t->type, "rom_browser") && t->root[0] == 0) {
            snprintf(t->root, sizeof(t->root), "/roms/nds/");
        }
        if (!strcmp(t->type, "homebrew_browser") && t->root[0] == 0) {
            snprintf(t->root, sizeof(t->root), "/3ds/");
        }
        normalize_path(t->root);
    }

    State* state = &g_state;
    if (!load_state(state)) {
        gfxExit();
        return 1;
    }

    if (!cfg->remember_last_position) {
        memset(state, 0, sizeof(*state));
        copy_str(state->last_target, sizeof(state->last_target), cfg->default_target);
    }

    if (state->last_target[0] == 0) copy_str(state->last_target, sizeof(state->last_target), cfg->default_target);
    int current_target = find_target_index(cfg, state->last_target);
    if (current_target < 0) {
        current_target = 0;
        copy_str(state->last_target, sizeof(state->last_target), cfg->targets[0].id);
    }

    memset(g_runtimes, 0, sizeof(g_runtimes));

    for (int i = 0; i < cfg->target_count; i++) {
        Target* t = &cfg->targets[i];
        TargetState* ts = get_target_state(state, t->id);
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
        if (ts && ts->loader_title_id[0]) {
            copy_str(t->loader_title_id, sizeof(t->loader_title_id), ts->loader_title_id);
        }
        if (ts && ts->loader_media[0]) {
            copy_str(t->loader_media, sizeof(t->loader_media), ts->loader_media);
        }
    }

    bool options_open = false;
    int options_selection = 0;
    int options_scroll = 0;
    int hold_up = 0;
    int hold_down = 0;
    refresh_options_menu(cfg);

    char status_message[64] = {0};
    int status_timer = 0;
    bool state_dirty = false;
    u64 last_save_ms = osGetTime();
    int move_cooldown = 0;

    TargetState* cur_ts = get_target_state(state, cfg->targets[current_target].id);
    if (cur_ts) g_nds_banners = cur_ts->nds_banner_mode != 0;
    auto_set_launcher(cfg, state, &state_dirty, status_message, sizeof(status_message), &status_timer);

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
            audio_play(SOUND_TOGGLE);
        }

        Target* target = &cfg->targets[current_target];
        TargetState* ts = get_target_state(state, target->id);

        if (!options_open) {
            if (kDown & KEY_L) {
                current_target = (current_target - 1 + cfg->target_count) % cfg->target_count;
                target = &cfg->targets[current_target];
                ts = get_target_state(state, target->id);
                snprintf(state->last_target, sizeof(state->last_target), "%s", target->id);
                if (ts) g_nds_banners = ts->nds_banner_mode != 0;
                state_dirty = true;
                audio_play(SOUND_MOVE);
            } else if (kDown & KEY_R) {
                current_target = (current_target + 1) % cfg->target_count;
                target = &cfg->targets[current_target];
                ts = get_target_state(state, target->id);
                snprintf(state->last_target, sizeof(state->last_target), "%s", target->id);
                if (ts) g_nds_banners = ts->nds_banner_mode != 0;
                state_dirty = true;
                audio_play(SOUND_MOVE);
            }
        }

        if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            if (!cache_matches(cache, ts->path)) build_dir_cache(target, ts, cache);
        }

        if (options_open) {
            int visible = (BOTTOM_H - HELP_BAR_H - 10) / LIST_ITEM_H;
            int prev = options_selection;
            if (rep_up) options_selection--;
            if (rep_down) options_selection++;
            if (options_selection < 0) options_selection = 0;
            if (options_selection >= g_option_count) options_selection = g_option_count - 1;
            if (kDown & KEY_L) options_selection -= visible;
            if (kDown & KEY_R) options_selection += visible;
            if (options_selection < 0) options_selection = 0;
            if (options_selection >= g_option_count) options_selection = g_option_count - 1;
            clamp_scroll_list(&options_scroll, options_selection, visible, g_option_count);
            if (options_selection != prev) audio_play(SOUND_MOVE);
            if (kDown & KEY_A) { handle_option_action(options_selection, cfg, state, &current_target, &state_dirty, status_message, sizeof(status_message), &status_timer); audio_play(SOUND_SELECT); }
            if (kDown & KEY_B) { options_open = false; audio_play(SOUND_BACK); }
        } else {
            if (!strcmp(target->type, "system_menu")) {
                if (kDown & KEY_A) {
                    snprintf(status_message, sizeof(status_message), "Exiting...");
                    status_timer = 60;
                    audio_play(SOUND_SELECT);
                    break;
                }
            } else if (!strcmp(target->type, "installed_titles")) {
                ensure_titles_loaded(cfg);
                int total = g_title_catalog.count;
                if (total <= 0) total = 1;
                int total_rows = (total + GRID_COLS - 1) / GRID_COLS;
                int visible_rows = (BOTTOM_H - HELP_BAR_H - 8) / GRID_ITEM_H;
                int prev = ts->selection;
                if (kDown & KEY_LEFT) ts->selection--;
                if (kDown & KEY_RIGHT) ts->selection++;
                if (rep_up) ts->selection -= GRID_COLS;
                if (rep_down) ts->selection += GRID_COLS;
                if (ts->selection < 0) ts->selection = 0;
                if (ts->selection >= total) ts->selection = total - 1;
                clamp_scroll_grid(&ts->scroll, ts->selection, visible_rows, total_rows);
                if ((kDown & (KEY_LEFT | KEY_RIGHT)) || rep_up || rep_down) state_dirty = true;
                if (ts->selection != prev) audio_play(SOUND_MOVE);
                if (kDown & KEY_A) {
                    snprintf(status_message, sizeof(status_message), "Launching...");
                    status_timer = 60;
                    audio_play(SOUND_SELECT);
                }
            } else if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            int visible = (BOTTOM_H - HELP_BAR_H - 8) / LIST_ITEM_H;
            int prev = ts->selection;
            bool moving = rep_up || rep_down;
            int step = 1;
            if (moving && (hold_up > 24 || hold_down > 24)) step = 2;
            if (rep_up) ts->selection -= step;
            if (rep_down) ts->selection += step;
                if (moving) move_cooldown = 8;
                if (ts->selection < 0) ts->selection = 0;
                if (cache->count <= 0) ts->selection = 0;
                if (ts->selection >= cache->count && cache->count > 0) ts->selection = cache->count - 1;
                clamp_scroll_list(&ts->scroll, ts->selection, visible, cache->count);
                if (rep_up || rep_down) state_dirty = true;
                if (ts->selection != prev) audio_play(SOUND_MOVE);
                if (!moving && move_cooldown == 0 && g_nds_banners) preload_nds_page(ts, cache, visible, NDS_PRELOAD_BUDGET);
                if (!moving && move_cooldown == 0 && g_nds_banners && cache->count > 0 && ts->selection < cache->count && !cache->entries[ts->selection].is_dir) {
                    char joined[512];
                    path_join(ts->path, cache->entries[ts->selection].name, joined, sizeof(joined));
                    char sdpath[512];
                    make_sd_path(joined, sdpath, sizeof(sdpath));
                    if (is_nds_name(cache->entries[ts->selection].name)) build_nds_entry(sdpath);
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
                        preload_nds_page(ts, cache, visible, NDS_PRELOAD_BUDGET);
                        state_dirty = true;
                        audio_play(SOUND_BACK);
                    }
                }
                if (kDown & KEY_A && cache->count > 0) {
                    FileEntry* fe = &cache->entries[ts->selection];
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
                        bool is_nds = is_nds_name(fe->name);
                        if (is_nds) {
                            char sdpath[512];
                            make_sd_path(joined, sdpath, sizeof(sdpath));
                            launch_nds_loader(target, sdpath, status_message, sizeof(status_message));
                        } else {
                            snprintf(status_message, sizeof(status_message), "Launching...");
                        }
                        audio_play(SOUND_SELECT);
                    }
                    status_timer = 60;
                }
            }
        }

        if (status_timer > 0) status_timer--;
        if (status_timer == 0) status_message[0] = 0;

        if (state_dirty) {
            u64 now = osGetTime();
            if (now - last_save_ms > 1000) {
                save_state(state);
                last_save_ms = now;
                state_dirty = false;
            }
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(g_top, C2D_Color32(10, 12, 14, 255));
        C2D_TargetClear(g_bottom, C2D_Color32(15, 16, 18, 255));

        C2D_SceneBegin(g_top);
        C2D_TextBufClear(g_textbuf);

        draw_rect(0, 0, PREVIEW_W, TOP_H, C2D_Color32(24, 26, 30, 255));
        draw_rect(PREVIEW_W, 0, TARGET_LIST_W, TOP_H, C2D_Color32(18, 20, 24, 255));

        const char* preview_title = "";
        char preview_buf[64];
        if (!strcmp(target->type, "system_menu")) {
            preview_title = "System Menu";
        } else if (!strcmp(target->type, "installed_titles")) {
            if (g_title_catalog.count > 0 && ts->selection < g_title_catalog.count) {
                preview_title = g_title_catalog.entries[ts->selection].name;
            } else {
                snprintf(preview_buf, sizeof(preview_buf), "Title %03d", ts->selection + 1);
                preview_title = preview_buf;
            }
        } else if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            if (cache->count > 0) {
                char joined[512];
                path_join(ts->path, cache->entries[ts->selection].name, joined, sizeof(joined));
                char sdpath[512];
                make_sd_path(joined, sdpath, sizeof(sdpath));
                bool is_file = !cache->entries[ts->selection].is_dir;
                bool is_nds = is_file && is_nds_name(cache->entries[ts->selection].name);
                NdsCacheEntry* nds = (is_nds ? nds_cache_entry(sdpath) : NULL);
                if (g_nds_banners && nds && nds->title[0]) preview_title = nds->title;
                if (preview_title[0] == 0) preview_title = cache->entries[ts->selection].name;
            } else {
                preview_title = "Empty";
            }
        }

        float text_scale = 0.6f;
        int max_lines = 3;
        draw_wrap_text_limited(8, 8, text_scale, C2D_Color32(230, 230, 230, 255), PREVIEW_W - 16, max_lines, preview_title);
        float banner_y = TOP_H - 96 - 20;
        u32 preview_color = C2D_Color32(40, 42, 48, 255);
        draw_rect(8, banner_y, 96, 96, preview_color);
        bool drew_icon = false;
        if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            if (cache->count > 0) {
                char joined[512];
                path_join(ts->path, cache->entries[ts->selection].name, joined, sizeof(joined));
                char sdpath[512];
                make_sd_path(joined, sdpath, sizeof(sdpath));
                bool is_file = !cache->entries[ts->selection].is_dir;
                bool is_nds = is_file && is_nds_name(cache->entries[ts->selection].name);
                NdsCacheEntry* nds = is_nds ? nds_cache_entry(sdpath) : NULL;
                if (g_nds_banners && nds && nds->has_rgba) {
                    draw_rgba_icon(8, banner_y, 3.0f, nds->rgba);
                    drew_icon = true;
                } else if (!g_nds_banners) {
                    if (nds && nds->has_rgba) {
                        draw_rgba_icon(8, banner_y, 3.0f, nds->rgba);
                        drew_icon = true;
                    } else if (move_cooldown == 0 && is_nds) {
                        build_nds_entry(sdpath);
                    }
                    if (!drew_icon) {
                        u32 col1 = hash_color(cache->entries[ts->selection].name);
                        u32 col2 = hash_color(cache->entries[ts->selection].name + 1);
                        u8 tmp[32 * 32 * 4];
                        make_sprite(tmp, col1, col2);
                        draw_rgba_icon(8, banner_y, 3.0f, tmp);
                        drew_icon = true;
                    }
                }
            }
        } else if (!strcmp(target->type, "installed_titles")) {
            drew_icon = true;
        } else if (!strcmp(target->type, "homebrew_browser")) {
            drew_icon = true;
        }
        if (!drew_icon) {
            draw_text(12, banner_y + 36, 0.5f, C2D_Color32(160, 160, 160, 255), "Preview");
        }

        char debug_line[128] = "";
        if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            if (cache->count > 0) {
                char joined[512];
                path_join(ts->path, cache->entries[ts->selection].name, joined, sizeof(joined));
                char sdpath[512];
                make_sd_path(joined, sdpath, sizeof(sdpath));
                NdsCacheEntry* nds = cache->entries[ts->selection].is_dir ? NULL : nds_cache_entry(sdpath);
                if (nds) {
                    snprintf(debug_line, sizeof(debug_line), "ofs:%lu ready:%d parsed:%d icon:%d", (unsigned long)nds->banner_off, nds->ready ? 1 : 0, nds->parsed ? 1 : 0, nds->icon.loaded ? 1 : 0);
                }
            }
        }
        if (debug_line[0]) {
            draw_text(8, TOP_H - 14, 0.45f, C2D_Color32(200, 200, 120, 255), debug_line);
        }

        int target_visible = TOP_H / LIST_ITEM_H;
        int target_scroll = 0;
        clamp_scroll_list(&target_scroll, current_target, target_visible, cfg->target_count);
        for (int i = 0; i < target_visible; i++) {
            int idx = target_scroll + i;
            if (idx >= cfg->target_count) break;
            int y = i * LIST_ITEM_H + 2;
            u32 color = (idx == current_target) ? C2D_Color32(60, 90, 140, 255) : C2D_Color32(28, 30, 36, 255);
            draw_rect(PREVIEW_W + 2, y, TARGET_LIST_W - 4, LIST_ITEM_H - 2, color);
            draw_wrap_text(PREVIEW_W + 4, y + 2, 0.6f, C2D_Color32(220, 220, 220, 255), TARGET_LIST_W - 8, cfg->targets[idx].label);
        }

        C2D_SceneBegin(g_bottom);
        C2D_TextBufClear(g_textbuf);

        if (options_open) {
            draw_rect(0, 0, BOTTOM_W, BOTTOM_H, C2D_Color32(12, 12, 16, 220));
            draw_text(8, 6, 0.6f, C2D_Color32(240, 240, 240, 255), "Options");
            int visible = (BOTTOM_H - HELP_BAR_H - 20) / LIST_ITEM_H;
            for (int i = 0; i < visible; i++) {
                int idx = options_scroll + i;
                if (idx >= g_option_count) break;
                int y = 24 + i * LIST_ITEM_H;
                u32 color = (idx == options_selection) ? C2D_Color32(70, 80, 120, 255) : C2D_Color32(28, 30, 36, 255);
                draw_rect(6, y, BOTTOM_W - 12, LIST_ITEM_H - 2, color);
                draw_text(10, y + 3, 0.5f, C2D_Color32(220, 220, 220, 255), g_options[idx].label);
            }
        } else if (!strcmp(target->type, "installed_titles")) {
            ensure_titles_loaded(cfg);
            int total = g_title_catalog.count;
            int total_rows = (total + GRID_COLS - 1) / GRID_COLS;
            int visible_rows = (BOTTOM_H - HELP_BAR_H - 8) / GRID_ITEM_H;
            for (int r = 0; r < visible_rows; r++) {
                int row = ts->scroll + r;
                if (row >= total_rows) break;
                for (int c = 0; c < GRID_COLS; c++) {
                    int idx = row * GRID_COLS + c;
                    if (idx >= total) break;
                    int x = 28 + c * GRID_ITEM_W;
                    int y = 6 + r * GRID_ITEM_H;
                    u32 color = (idx == ts->selection) ? C2D_Color32(70, 100, 150, 255) : C2D_Color32(26, 28, 34, 255);
                    draw_rect(x, y, GRID_ITEM_W - 6, GRID_ITEM_H - 6, color);
                    const char* name = (idx < g_title_catalog.count) ? g_title_catalog.entries[idx].name : "";
                    draw_text(x + 6, y + 10, 0.45f, C2D_Color32(220, 220, 220, 255), name);
                }
            }
            const char* alpha = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            for (int i = 0; i < 27; i++) {
                int y = 6 + i * 8;
                char label[2] = { alpha[i], 0 };
                bool active = false;
                if (g_title_catalog.count > 0 && ts->selection < g_title_catalog.count) {
                    char b = g_title_catalog.entries[ts->selection].bucket;
                    if (alpha[i] == '#') active = (b < 'A' || b > 'Z');
                    else active = b == alpha[i];
                }
                u32 color = active ? C2D_Color32(230, 210, 120, 255) : C2D_Color32(140, 140, 140, 255);
                draw_text(4, y, 0.4f, color, label);
            }
        } else if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser")) {
            DirCache* cache = &g_runtimes[current_target].cache;
            int visible = (BOTTOM_H - HELP_BAR_H - 8) / LIST_ITEM_H;
            for (int i = 0; i < visible; i++) {
                int idx = ts->scroll + i;
                if (idx >= cache->count) break;
                int y = 6 + i * LIST_ITEM_H;
                u32 color = (idx == ts->selection) ? C2D_Color32(70, 90, 140, 255) : C2D_Color32(26, 28, 34, 255);
                draw_rect(6, y, BOTTOM_W - 12, LIST_ITEM_H - 2, color);
                char joined[512];
                path_join(ts->path, cache->entries[idx].name, joined, sizeof(joined));
                const char* label = cache->entries[idx].name;
                static char fname[256];
                base_name_no_ext(cache->entries[idx].name, fname, sizeof(fname));
                label = fname[0] ? fname : cache->entries[idx].name;
                float text_x = 10.0f;
                draw_text(text_x, y + 3, 0.5f, C2D_Color32(220, 220, 220, 255), label);
            }
        } else {
            draw_text(8, 10, 0.6f, C2D_Color32(220, 220, 220, 255), "System Menu");
            draw_text(8, 30, 0.5f, C2D_Color32(160, 160, 160, 255), "Press A to exit");
        }

        if (cfg->help_bar) {
            const char* help = "A Launch   B Back   X Sort   Y Search";
            if (!strcmp(target->type, "homebrew_browser") || !strcmp(target->type, "rom_browser")) {
                DirCache* cache = &g_runtimes[current_target].cache;
                if (cache->count > 0 && cache->entries[ts->selection].is_dir) {
                    help = "A Open   B Back   X Sort   Y Search";
                }
            }
            draw_help_bar(help);
        }

        if (status_message[0]) {
            draw_rect(60, 90, 200, 40, C2D_Color32(0, 0, 0, 200));
            draw_text(90, 104, 0.6f, C2D_Color32(240, 240, 240, 255), status_message);
        }

        C3D_FrameEnd(0);
    }

    save_state(state);

    C2D_TextBufDelete(g_textbuf);
    C2D_Fini();
    C3D_Fini();
    ndspExit();
    fsExit();
    gfxExit();
    return 0;
}
