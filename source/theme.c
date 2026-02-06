#include "fmux.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "stb_image.h"

static void reorder_channels(u8* data, int count, const char* order);

static bool parse_color_value(const char* v, u32* out) {
    if (!v || !out) return false;
    char buf[16];
    size_t len = strlen(v);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, v, len);
    buf[len] = 0;
    strip_quotes(buf);
    if (buf[0] == '#') memmove(buf, buf + 1, strlen(buf));
    if (!strncmp(buf, "0x", 2) || !strncmp(buf, "0X", 2)) memmove(buf, buf + 2, strlen(buf) - 1);
    size_t blen = strlen(buf);
    if (blen != 6 && blen != 8) return false;
    char* end = NULL;
    unsigned long val = strtoul(buf, &end, 16);
    if (!end || *end) return false;
    u8 a = 255;
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    if (blen == 6) {
        r = (val >> 16) & 0xFF;
        g = (val >> 8) & 0xFF;
        b = val & 0xFF;
    } else {
        a = (val >> 24) & 0xFF;
        r = (val >> 16) & 0xFF;
        g = (val >> 8) & 0xFF;
        b = val & 0xFF;
    }
    *out = C2D_Color32(r, g, b, a);
    return true;
}

static int clamp_int(int v, int minv, int maxv) {
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

static bool parse_float_value(const char* v, float* out) {
    if (!v || !out) return false;
    char buf[32];
    size_t len = strlen(v);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, v, len);
    buf[len] = 0;
    strip_quotes(buf);
    char* end = NULL;
    float f = strtof(buf, &end);
    if (!end || *end) return false;
    *out = f;
    return true;
}

static void theme_free_images(Theme* t) {
    if (!t) return;
    icon_free(&t->top_tex);
    icon_free(&t->bottom_tex);
    icon_free(&t->status_tex);
    icon_free(&t->sprite_tex);
    icon_free(&t->list_item_tex);
    icon_free(&t->list_sel_tex);
    icon_free(&t->tab_item_tex);
    icon_free(&t->tab_sel_tex);
    icon_free(&t->option_item_tex);
    icon_free(&t->option_sel_tex);
    icon_free(&t->preview_frame_tex);
    icon_free(&t->help_tex);
    t->top_loaded = false;
    t->bottom_loaded = false;
    t->status_loaded = false;
    t->sprite_loaded = false;
    t->list_item_loaded = false;
    t->list_sel_loaded = false;
    t->tab_item_loaded = false;
    t->tab_sel_loaded = false;
    t->option_item_loaded = false;
    t->option_sel_loaded = false;
    t->preview_frame_loaded = false;
    t->help_loaded = false;
}

static bool build_theme_path(const char* folder, const char* rel, char* out, size_t out_size) {
    if (!rel || !rel[0] || !out || out_size == 0) return false;
    if (!strncmp(rel, "sdmc:/", 6) || rel[0] == '/') {
        snprintf(out, out_size, "%s", rel);
        return true;
    }
    snprintf(out, out_size, "sdmc:/3ds/FirmMux/themes/%s/%s", folder, rel);
    return true;
}

static float compute_alpha_center_norm(const u8* data, int w, int h) {
    if (!data || w <= 0 || h <= 0) return 0.5f;
    int min_y = h;
    int max_y = -1;
    for (int y = 0; y < h; y++) {
        const u8* row = data + (size_t)y * (size_t)w * 4;
        for (int x = 0; x < w; x++) {
            if (row[x * 4 + 3] != 0) {
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                break;
            }
        }
    }
    if (max_y < min_y) return 0.5f;
    float center = (min_y + max_y + 1) * 0.5f;
    return center / (float)h;
}

static bool load_theme_image(const char* folder, const char* rel, IconTexture* icon, int* out_w, int* out_h, bool* loaded, const char* order, bool swap_rb, float* out_center_y) {
    if (!rel || !rel[0]) return false;
    char path[256];
    if (!build_theme_path(folder, rel, path, sizeof(path))) return false;
    if (debug_log_enabled()) debug_log("theme: load image %s", path);
    u8* file = NULL;
    size_t fsize = 0;
    if (!read_file(path, &file, &fsize) || !file || fsize == 0) {
        if (file) free(file);
        if (debug_log_enabled()) debug_log("theme: read failed %s", path);
        return false;
    }
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load_from_memory(file, (int)fsize, &w, &h, &comp, 4);
    free(file);
    if (!data || w <= 0 || h <= 0) {
        if (debug_log_enabled()) debug_log("theme: decode failed %s (%s)", path, stbi_failure_reason() ? stbi_failure_reason() : "unknown");
        if (data) stbi_image_free(data);
        return false;
    }
    const char* use_order = order && order[0] ? order : "rgba";
    if (swap_rb && (!order || !order[0])) use_order = "bgra";
    if (use_order) reorder_channels(data, w * h, use_order);
    if (out_center_y) *out_center_y = compute_alpha_center_norm(data, w, h);
    bool ok = icon_from_rgba(icon, data, w, h);
    stbi_image_free(data);
    if (ok) {
        if (debug_log_enabled()) debug_log("theme: loaded %s (%dx%d)", path, w, h);
        if (out_w) *out_w = w;
        if (out_h) *out_h = h;
        if (loaded) *loaded = true;
    } else if (debug_log_enabled()) {
        debug_log("theme: upload failed %s", path);
    }
    return ok;
}

static void reorder_channels(u8* data, int count, const char* order) {
    if (!data || !order) return;
    char rgb[4] = {0,0,0,0};
    int ri = 0;
    for (size_t i = 0; order[i] && ri < 3; i++) {
        char c = (char)tolower((unsigned char)order[i]);
        if (c == 'a') continue;
        if (c == 'r' || c == 'g' || c == 'b') {
            rgb[ri++] = c;
        }
    }
    if (ri < 3) return;
    int map[3] = {0,1,2};
    for (int i = 0; i < 3; i++) {
        if (rgb[i] == 'r') map[i] = 0;
        else if (rgb[i] == 'g') map[i] = 1;
        else if (rgb[i] == 'b') map[i] = 2;
    }
    for (int i = 0; i < count; i++) {
        u8* px = data + i * 4;
        u8 src[4] = { px[0], px[1], px[2], px[3] };
        px[0] = src[map[0]];
        px[1] = src[map[1]];
        px[2] = src[map[2]];
        px[3] = src[3];
    }
}

void theme_default(Theme* t) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    copy_str(t->name, sizeof(t->name), "default");
    t->list_item_h = 20;
    t->line_spacing = 26;
    t->status_h = 16;
    t->top_bg = C2D_Color32(10, 12, 14, 255);
    t->bottom_bg = C2D_Color32(15, 16, 18, 255);
    t->panel_left = C2D_Color32(24, 26, 30, 255);
    t->panel_right = C2D_Color32(18, 20, 24, 255);
    t->preview_bg = C2D_Color32(40, 42, 48, 255);
    t->text_primary = C2D_Color32(230, 230, 230, 255);
    t->text_secondary = C2D_Color32(200, 200, 200, 255);
    t->text_muted = C2D_Color32(160, 160, 160, 255);
    t->tab_bg = C2D_Color32(28, 30, 36, 255);
    t->tab_sel = C2D_Color32(60, 90, 140, 255);
    t->tab_text = C2D_Color32(220, 220, 220, 255);
    t->list_bg = C2D_Color32(26, 28, 34, 255);
    t->list_sel = C2D_Color32(70, 100, 150, 255);
    t->list_text = C2D_Color32(220, 220, 220, 255);
    t->option_bg = C2D_Color32(28, 30, 36, 255);
    t->option_sel = C2D_Color32(70, 80, 120, 255);
    t->option_text = C2D_Color32(220, 220, 220, 255);
    t->option_header = C2D_Color32(240, 240, 240, 255);
    t->overlay_bg = C2D_Color32(12, 12, 16, 220);
    t->help_bg = C2D_Color32(20, 20, 20, 255);
    t->help_line = C2D_Color32(90, 92, 100, 255);
    t->help_text = C2D_Color32(220, 220, 220, 255);
    t->status_bg = C2D_Color32(20, 22, 26, 255);
    t->status_text = C2D_Color32(210, 210, 210, 255);
    t->status_icon = C2D_Color32(200, 200, 200, 255);
    t->status_dim = C2D_Color32(90, 90, 90, 255);
    t->status_bolt = C2D_Color32(255, 220, 80, 255);
    t->toast_bg = C2D_Color32(0, 0, 0, 200);
    t->toast_text = C2D_Color32(240, 240, 240, 255);
    t->accent = 0;
    t->accent_set = false;
    t->font_scale_top = 1.0f;
    t->font_scale_bottom = 1.0f;
    t->panel_alpha = 100;
    t->row_padding = 1;
    t->tab_padding = 1;
    t->radius_global = 0.0f;
    t->radius_tabs = 0.0f;
    t->radius_list = 0.0f;
    t->radius_options = 0.0f;
    t->radius_panels = 0.0f;
    t->radius_preview = 0.0f;
    t->radius_status = 0.0f;
    t->radius_picker = 0.0f;
    t->ui_sounds_dir[0] = 0;
    t->bgm_path[0] = 0;
    t->list_item_offset_y = 0;
    t->list_text_offset_y = 0;
    t->tab_item_offset_y = 0;
    t->tab_text_offset_y = 0;
    t->option_item_offset_y = 0;
    t->option_text_offset_y = 0;
    t->help_text_offset_y = 0;
    t->status_text_offset_y = 0;
    t->image_swap_rb = false;
    copy_str(t->image_channel_order, sizeof(t->image_channel_order), "rgba");
    t->list_item_center_y = 0.5f;
    t->list_sel_center_y = 0.5f;
    t->tab_item_center_y = 0.5f;
    t->tab_sel_center_y = 0.5f;
    t->option_item_center_y = 0.5f;
    t->option_sel_center_y = 0.5f;
    t->preview_frame_center_y = 0.5f;
    t->help_strip_center_y = 0.5f;
    t->status_strip_center_y = 0.5f;
}

bool load_theme(Theme* t, const char* name) {
    if (!t) return false;
    theme_free_images(t);
    theme_default(t);
    if (!name || !name[0]) return true;
    copy_str(t->name, sizeof(t->name), name);
    char path[256];
    snprintf(path, sizeof(path), "sdmc:/3ds/FirmMux/themes/%s/theme.yaml", name);
    u8* data = NULL;
    size_t size = 0;
    if (!read_file(path, &data, &size) || !data || size == 0) {
        if (data) free(data);
        if (debug_log_enabled()) debug_log("theme: missing %s", path);
        return false;
    }
    char* text = (char*)malloc(size + 1);
    if (!text) { free(data); return false; }
    memcpy(text, data, size);
    text[size] = 0;
    free(data);
    char* p = text;
    while (*p) {
        char* end = strchr(p, '\n');
        if (end) *end = 0;
        trim(p);
        if (p[0] && p[0] != '#') {
            char val[64];
            if (parse_value(p, val, sizeof(val))) {
                if (!strncmp(p, "name:", 5)) {
                    copy_str(t->name, sizeof(t->name), val);
                } else if (!strncmp(p, "list_item_h:", 12)) {
                    t->list_item_h = clamp_int(atoi(val), 16, 30);
                } else if (!strncmp(p, "line_spacing:", 13)) {
                    t->line_spacing = clamp_int(atoi(val), 18, 34);
                } else if (!strncmp(p, "status_bar_h:", 13)) {
                    t->status_h = clamp_int(atoi(val), 10, 24);
                } else if (!strncmp(p, "font_scale_top:", 15)) {
                    float f = 1.0f;
                    if (parse_float_value(val, &f)) t->font_scale_top = f;
                } else if (!strncmp(p, "font_scale_bottom:", 18)) {
                    float f = 1.0f;
                    if (parse_float_value(val, &f)) t->font_scale_bottom = f;
                } else if (!strncmp(p, "panel_alpha:", 12)) {
                    t->panel_alpha = clamp_int(atoi(val), 0, 100);
                } else if (!strncmp(p, "row_padding:", 12)) {
                    t->row_padding = clamp_int(atoi(val), 0, 6);
                } else if (!strncmp(p, "tab_padding:", 12)) {
                    t->tab_padding = clamp_int(atoi(val), 0, 6);
                } else if (!strncmp(p, "radius_global:", 14)) {
                    float f = 0.0f;
                    if (parse_float_value(val, &f)) t->radius_global = f;
                } else if (!strncmp(p, "radius_tabs:", 12)) {
                    float f = 0.0f;
                    if (parse_float_value(val, &f)) t->radius_tabs = f;
                } else if (!strncmp(p, "radius_list:", 12)) {
                    float f = 0.0f;
                    if (parse_float_value(val, &f)) t->radius_list = f;
                } else if (!strncmp(p, "radius_options:", 15)) {
                    float f = 0.0f;
                    if (parse_float_value(val, &f)) t->radius_options = f;
                } else if (!strncmp(p, "radius_panels:", 14)) {
                    float f = 0.0f;
                    if (parse_float_value(val, &f)) t->radius_panels = f;
                } else if (!strncmp(p, "radius_preview:", 15)) {
                    float f = 0.0f;
                    if (parse_float_value(val, &f)) t->radius_preview = f;
                } else if (!strncmp(p, "radius_status:", 14)) {
                    float f = 0.0f;
                    if (parse_float_value(val, &f)) t->radius_status = f;
                } else if (!strncmp(p, "radius_picker:", 14)) {
                    float f = 0.0f;
                    if (parse_float_value(val, &f)) t->radius_picker = f;
                } else if (!strncmp(p, "ui_sounds_dir:", 14)) {
                    copy_str(t->ui_sounds_dir, sizeof(t->ui_sounds_dir), val);
                } else if (!strncmp(p, "bgm_path:", 9)) {
                    copy_str(t->bgm_path, sizeof(t->bgm_path), val);
                } else if (!strncmp(p, "top_image:", 10)) {
                    copy_str(t->top_image, sizeof(t->top_image), val);
                } else if (!strncmp(p, "bottom_image:", 13)) {
                    copy_str(t->bottom_image, sizeof(t->bottom_image), val);
                } else if (!strncmp(p, "status_strip:", 12)) {
                    copy_str(t->status_strip, sizeof(t->status_strip), val);
                } else if (!strncmp(p, "sprite_icon:", 12)) {
                    copy_str(t->sprite_icon, sizeof(t->sprite_icon), val);
                } else if (!strncmp(p, "list_item_image:", 16)) {
                    copy_str(t->list_item_image, sizeof(t->list_item_image), val);
                } else if (!strncmp(p, "list_sel_image:", 15)) {
                    copy_str(t->list_sel_image, sizeof(t->list_sel_image), val);
                } else if (!strncmp(p, "tab_item_image:", 15)) {
                    copy_str(t->tab_item_image, sizeof(t->tab_item_image), val);
                } else if (!strncmp(p, "tab_sel_image:", 14)) {
                    copy_str(t->tab_sel_image, sizeof(t->tab_sel_image), val);
                } else if (!strncmp(p, "option_item_image:", 18)) {
                    copy_str(t->option_item_image, sizeof(t->option_item_image), val);
                } else if (!strncmp(p, "option_sel_image:", 17)) {
                    copy_str(t->option_sel_image, sizeof(t->option_sel_image), val);
                } else if (!strncmp(p, "preview_frame:", 14)) {
                    copy_str(t->preview_frame, sizeof(t->preview_frame), val);
                } else if (!strncmp(p, "help_strip:", 11)) {
                    copy_str(t->help_strip, sizeof(t->help_strip), val);
                } else if (!strncmp(p, "list_item_offset_y:", 19)) {
                    t->list_item_offset_y = clamp_int(atoi(val), -8, 8);
                } else if (!strncmp(p, "list_text_offset_y:", 19)) {
                    t->list_text_offset_y = clamp_int(atoi(val), -8, 8);
                } else if (!strncmp(p, "tab_item_offset_y:", 18)) {
                    t->tab_item_offset_y = clamp_int(atoi(val), -8, 8);
                } else if (!strncmp(p, "tab_text_offset_y:", 18)) {
                    t->tab_text_offset_y = clamp_int(atoi(val), -8, 8);
                } else if (!strncmp(p, "option_item_offset_y:", 21)) {
                    t->option_item_offset_y = clamp_int(atoi(val), -8, 8);
                } else if (!strncmp(p, "option_text_offset_y:", 21)) {
                    t->option_text_offset_y = clamp_int(atoi(val), -8, 8);
                } else if (!strncmp(p, "help_text_offset_y:", 19)) {
                    t->help_text_offset_y = clamp_int(atoi(val), -8, 8);
                } else if (!strncmp(p, "status_text_offset_y:", 21)) {
                    t->status_text_offset_y = clamp_int(atoi(val), -8, 8);
                } else if (!strncmp(p, "image_swap_rb:", 14)) {
                    bool b = false;
                    if (parse_bool(val, &b)) t->image_swap_rb = b;
                } else if (!strncmp(p, "image_channel_order:", 20)) {
                    copy_str(t->image_channel_order, sizeof(t->image_channel_order), val);
                } else {
                    u32 c;
                    if (!strncmp(p, "accent:", 7)) {
                        if (parse_color_value(val, &c)) {
                            t->accent = c;
                            t->accent_set = true;
                        }
                    } else if (parse_color_value(val, &c)) {
                        if (!strncmp(p, "top_bg:", 7)) t->top_bg = c;
                        else if (!strncmp(p, "bottom_bg:", 10)) t->bottom_bg = c;
                        else if (!strncmp(p, "panel_left:", 11)) t->panel_left = c;
                        else if (!strncmp(p, "panel_right:", 12)) t->panel_right = c;
                        else if (!strncmp(p, "preview_bg:", 11)) t->preview_bg = c;
                        else if (!strncmp(p, "text_primary:", 13)) t->text_primary = c;
                        else if (!strncmp(p, "text_secondary:", 15)) t->text_secondary = c;
                        else if (!strncmp(p, "text_muted:", 11)) t->text_muted = c;
                        else if (!strncmp(p, "tab_bg:", 7)) t->tab_bg = c;
                        else if (!strncmp(p, "tab_sel:", 8)) t->tab_sel = c;
                        else if (!strncmp(p, "tab_text:", 9)) t->tab_text = c;
                        else if (!strncmp(p, "list_bg:", 8)) t->list_bg = c;
                        else if (!strncmp(p, "list_sel:", 9)) t->list_sel = c;
                        else if (!strncmp(p, "list_text:", 10)) t->list_text = c;
                        else if (!strncmp(p, "option_bg:", 10)) t->option_bg = c;
                        else if (!strncmp(p, "option_sel:", 11)) t->option_sel = c;
                        else if (!strncmp(p, "option_text:", 12)) t->option_text = c;
                        else if (!strncmp(p, "option_header:", 14)) t->option_header = c;
                        else if (!strncmp(p, "overlay_bg:", 11)) t->overlay_bg = c;
                        else if (!strncmp(p, "help_bg:", 8)) t->help_bg = c;
                        else if (!strncmp(p, "help_line:", 10)) t->help_line = c;
                        else if (!strncmp(p, "help_text:", 10)) t->help_text = c;
                        else if (!strncmp(p, "status_bg:", 10)) t->status_bg = c;
                        else if (!strncmp(p, "status_text:", 12)) t->status_text = c;
                        else if (!strncmp(p, "status_icon:", 12)) t->status_icon = c;
                        else if (!strncmp(p, "status_dim:", 11)) t->status_dim = c;
                        else if (!strncmp(p, "status_bolt:", 12)) t->status_bolt = c;
                        else if (!strncmp(p, "toast_bg:", 9)) t->toast_bg = c;
                        else if (!strncmp(p, "toast_text:", 11)) t->toast_text = c;
                    }
                }
            }
        }
        if (!end) break;
        p = end + 1;
    }
    free(text);
    return true;
}
