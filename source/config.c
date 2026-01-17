#include "fmux.h"
#include <string.h>
#include <stdlib.h>

static const char* base_config =
"version: 1\n"
"ui:\n"
"  default_target: 3ds\n"
"  remember_last_position: true\n"
"  help_bar: true\n"
"targets:\n"
"  - id: system\n"
"    type: system_menu\n"
"    label: \"System Menu\"\n"
"  - id: 3ds\n"
"    type: installed_titles\n"
"    label: \"3DS Titles\"\n"
"    show_system_titles: false\n"
"  - id: hbl\n"
"    type: homebrew_browser\n"
"    label: \"Homebrew Launcher\"\n"
"    root: /3ds/\n"
"  - id: nds\n"
"    type: rom_browser\n"
"    label: \"NDS Titles\"\n"
"    root: /roms/nds/\n"
"    extensions: [\".nds\"]\n"
"    folders:\n"
"      alpha_buckets: true\n";

static bool parse_config(const char* text, Config* cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->version = 1;
    const char* p = text;
    Target* current = NULL;
    while (*p) {
        const char* end = strchr(p, '\n');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        char line[256];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = 0;
        trim(line);
        if (strlen(line) == 0) { if (!end) break; p = end + 1; continue; }
        if (!strncmp(line, "version:", 8)) {
            int v = atoi(line + 8);
            cfg->version = v;
        } else if (!strncmp(line, "default_target:", 15)) {
            char val[64];
            if (parse_value(line, val, sizeof(val))) snprintf(cfg->default_target, sizeof(cfg->default_target), "%s", val);
        } else if (!strncmp(line, "remember_last_position:", 23)) {
            char val[16];
            if (parse_value(line, val, sizeof(val))) parse_bool(val, &cfg->remember_last_position);
        } else if (!strncmp(line, "help_bar:", 9)) {
            char val[16];
            if (parse_value(line, val, sizeof(val))) parse_bool(val, &cfg->help_bar);
        } else if (line[0] == '-' && cfg->target_count < MAX_TARGETS) {
            current = &cfg->targets[cfg->target_count++];
            memset(current, 0, sizeof(*current));
        } else if (current) {
            if (!strncmp(line, "id:", 3)) {
                char val[32];
                if (parse_value(line, val, sizeof(val))) snprintf(current->id, sizeof(current->id), "%s", val);
            } else if (!strncmp(line, "type:", 5)) {
                char val[32];
                if (parse_value(line, val, sizeof(val))) snprintf(current->type, sizeof(current->type), "%s", val);
            } else if (!strncmp(line, "label:", 6)) {
                char val[64];
                if (parse_value(line, val, sizeof(val))) snprintf(current->label, sizeof(current->label), "%s", val);
            } else if (!strncmp(line, "root:", 5)) {
                char val[256];
                if (parse_value(line, val, sizeof(val))) snprintf(current->root, sizeof(current->root), "%s", val);
            } else if (!strncmp(line, "loader_title_id:", 16)) {
                char val[32];
                if (parse_value(line, val, sizeof(val))) snprintf(current->loader_title_id, sizeof(current->loader_title_id), "%s", val);
            } else if (!strncmp(line, "loader_media:", 13)) {
                char val[16];
                if (parse_value(line, val, sizeof(val))) snprintf(current->loader_media, sizeof(current->loader_media), "%s", val);
            } else if (!strncmp(line, "extensions:", 11)) {
                const char* lb = strchr(line, '[');
                const char* rb = strchr(line, ']');
                if (lb && rb && rb > lb) {
                    char list[128];
                    size_t l = (size_t)(rb - lb - 1);
                    if (l >= sizeof(list)) l = sizeof(list) - 1;
                    memcpy(list, lb + 1, l);
                    list[l] = 0;
                    char* tok = strtok(list, ",");
                    while (tok && current->ext_count < MAX_EXTENSIONS) {
                        trim(tok);
                        strip_quotes(tok);
                        snprintf(current->extensions[current->ext_count++], 16, "%s", tok);
                        tok = strtok(NULL, ",");
                    }
                }
            } else if (!strncmp(line, "show_system_titles:", 18)) {
                char val[16];
                if (parse_value(line, val, sizeof(val))) parse_bool(val, &current->show_system_titles);
            } else if (!strncmp(line, "alpha_buckets:", 14)) {
                char val[16];
                if (parse_value(line, val, sizeof(val))) parse_bool(val, &current->alpha_buckets);
            }
        }
        if (!end) break; else p = end + 1;
    }
    if (cfg->default_target[0] == 0 && cfg->target_count > 0) snprintf(cfg->default_target, sizeof(cfg->default_target), "%s", cfg->targets[0].id);
    return cfg->target_count > 0;
}

bool load_or_create_config(Config* cfg) {
    ensure_dirs();
    u8* data = NULL;
    size_t size = 0;
    bool exists = file_exists(CONFIG_PATH);
    if (exists && read_file(CONFIG_PATH, &data, &size)) {
        bool ok = parse_config((const char*)data, cfg);
        free(data);
        if (ok) return true;
        rename(CONFIG_PATH, CONFIG_BAK_PATH);
    }
    ensure_dirs();
    if (!write_file(CONFIG_PATH, base_config)) return false;
    return parse_config(base_config, cfg);
}
