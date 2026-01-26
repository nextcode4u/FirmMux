#include "fmux.h"
#include <string.h>
#include <stdlib.h>

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

void theme_default(Theme* t) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    copy_str(t->name, sizeof(t->name), "default");
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
}

bool load_theme(Theme* t, const char* name) {
    if (!t) return false;
    theme_default(t);
    if (!name || !name[0]) return true;
    copy_str(t->name, sizeof(t->name), name);
    char path[256];
    snprintf(path, sizeof(path), "sdmc:/3ds/FirmMux/themes/%s/theme.yaml", name);
    u8* data = NULL;
    size_t size = 0;
    if (!read_file(path, &data, &size) || !data || size == 0) {
        if (data) free(data);
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
                } else {
                    u32 c;
                    if (parse_color_value(val, &c)) {
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
