#include "fmux.h"
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <3ds.h>
#include "stb_image.h"

static bool g_debug_log_enabled = false;

void trim(char* s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || isspace((unsigned char)s[len - 1]))) {
        s[len - 1] = 0;
        len--;
    }
    char* p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

void copy_str(char* dst, size_t dst_size, const char* src) {
    if (!dst_size) return;
    size_t len = strlen(src);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = 0;
}

bool parse_bool(const char* v, bool* out) {
    if (!v) return false;
    if (!strcmp(v, "true")) { *out = true; return true; }
    if (!strcmp(v, "false")) { *out = false; return true; }
    return false;
}

void strip_quotes(char* s) {
    size_t len = strlen(s);
    if (len >= 2 && s[0] == '"' && s[len - 1] == '"') {
        memmove(s, s + 1, len - 2);
        s[len - 2] = 0;
    }
}

bool parse_value(const char* line, char* out, size_t out_size) {
    const char* colon = strchr(line, ':');
    if (!colon) return false;
    colon++;
    while (*colon && isspace((unsigned char)*colon)) colon++;
    snprintf(out, out_size, "%s", colon);
    trim(out);
    strip_quotes(out);
    return true;
}

void normalize_path(char* path) {
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[len - 1] = 0;
        len--;
    }
}

void normalize_path_sd(char* path, size_t path_size) {
    if (path_size == 0) return;
    for (size_t i = 0; path[i]; i++) if (path[i] == '\\') path[i] = '/';
    const char* p = path;
    if (!strncmp(path, "sd:/", 4)) p = path + 4;
    else if (!strncmp(path, "sdmc:", 5)) p = path + 5;
    else if (!strncmp(path, "fat:", 4)) p = path + 4;
    else if (!strncmp(path, "/sd/", 4)) p = path + 3;
    else if (path[0] == '/' && path[1] != 0) p = path;
    while (*p == '/') p++;
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "sd:/%s", p);
    // collapse any accidental double slashes after prefix
    char compact[512];
    size_t w = 0;
    for (size_t i = 0; tmp[i] && w + 1 < sizeof(compact); i++) {
        if (tmp[i] == '/' && w > 0 && compact[w - 1] == '/') continue;
        compact[w++] = tmp[i];
    }
    compact[w] = 0;
    snprintf(path, path_size, "%s", compact);
    normalize_path(path);
}

void normalize_path_to_sd_colon(char* path, size_t path_size) {
    if (!path || !path[0]) return;
    if (!strncasecmp(path, "cart:", 5)) return;
    if (!strncasecmp(path, "slot1:", 6)) return;
    if (!strncasecmp(path, "slot-1:", 7)) return;
    normalize_path_sd(path, path_size);
}

bool write_file(const char* path, const char* data) {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    size_t len = strlen(data);
    size_t w = fwrite(data, 1, len, f);
    fclose(f);
    return w == len;
}

bool read_file(const char* path, u8** out, size_t* out_size) {
    *out = NULL;
    *out_size = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return false; }
    u8* buf = (u8*)malloc(size);
    if (!buf) { fclose(f); return false; }
    size_t r = fread(buf, 1, size, f);
    fclose(f);
    if (r != (size_t)size) { free(buf); return false; }
    *out = buf;
    *out_size = r;
    return true;
}

void ensure_dirs(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/FirmMux", 0777);
    mkdir(CACHE_DIR, 0777);
    mkdir(CACHE_NDS_DIR, 0777);
    mkdir(CACHE_3DS_DIR, 0777);
    mkdir(LAUNCH_DIR, 0777);
    mkdir(DEBUG_DIR, 0777);
    mkdir(BACKGROUNDS_DIR, 0777);
    mkdir(BACKGROUNDS_TOP_DIR, 0777);
    mkdir(BACKGROUNDS_BOTTOM_DIR, 0777);
}

void clear_dir_recursive(const char* path, bool keep_root) {
    DIR* dir = opendir(path);
    if (!dir) return;
    struct dirent* ent;
    while ((ent = readdir(dir))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
        if (ent->d_type == DT_DIR) {
            clear_dir_recursive(full, false);
            rmdir(full);
        } else {
            remove(full);
        }
    }
    closedir(dir);
    if (!keep_root) rmdir(path);
}

bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool is_nds_name(const char* name) {
    const char* dot = strrchr(name, '.');
    if (!dot) return false;
    return strcasecmp(dot, ".nds") == 0 || strcasecmp(dot, ".dsi") == 0;
}

bool is_3dsx_name(const char* name) {
    const char* dot = strrchr(name, '.');
    if (!dot) return false;
    return strcasecmp(dot, ".3dsx") == 0;
}

bool path_has_prefix(const char* path, const char* prefix) {
    size_t lp = strlen(prefix);
    if (lp == 0) return true;
    return strncmp(path, prefix, lp) == 0;
}

void make_sd_path(const char* in, char* out, size_t out_size) {
    if (!strncmp(in, "sdmc:", 5)) {
        snprintf(out, out_size, "%s", in);
        return;
    }
    if (in[0] == '/') {
        snprintf(out, out_size, "sdmc:%s", in);
        return;
    }
    snprintf(out, out_size, "sdmc:/%s", in);
}

void path_join(const char* base, const char* name, char* out, size_t out_size) {
    if (out_size == 0) return;
    size_t blen = strlen(base);
    size_t nlen = strlen(name);
    size_t needed = blen + 1 + nlen + 1;
    if (needed > out_size) {
        out[0] = 0;
        return;
    }
    size_t pos = 0;
    if (!strcmp(base, "/")) {
        out[pos++] = '/';
    } else {
        memcpy(out + pos, base, blen);
        pos += blen;
        out[pos++] = '/';
    }
    memcpy(out + pos, name, nlen);
    pos += nlen;
    out[pos] = 0;
}

void path_parent(char* path) {
    size_t len = strlen(path);
    if (len <= 1) return;
    char* p = strrchr(path, '/');
    if (!p) return;
    if (p == path) {
        path[1] = 0;
        return;
    }
    *p = 0;
}

void base_name_no_ext(const char* path, char* out, size_t out_size) {
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    copy_str(out, out_size, base);
    char* dot = strrchr(out, '.');
    if (dot && dot != out) *dot = 0;
}

const char* bucket_for_index(int index) {
    if (index < 10) return "#";
    char c = 'A' + ((index - 10) % 26);
    static char buf[2];
    buf[0] = c;
    buf[1] = 0;
    return buf;
}

void debug_set_enabled(bool on) {
    g_debug_log_enabled = on;
    if (on) {
        ensure_dirs();
        FILE* f = fopen(DEBUG_LOG_PATH, "w");
        if (f) fclose(f);
        debug_log("debug log enabled");
    }
}

bool debug_log_enabled(void) {
    return g_debug_log_enabled;
}

void debug_log(const char* fmt, ...) {
    if (!g_debug_log_enabled) return;
    ensure_dirs();
    FILE* f = fopen(DEBUG_LOG_PATH, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

void debug_dump_rgba(const u8* rgba, size_t size) {
    static int dump_idx = 0;
    if (!g_debug_log_enabled || !rgba || size == 0) return;
    ensure_dirs();
    char path[256];
    snprintf(path, sizeof(path), "%s_%02d.bin", DEBUG_ICON_PATH, dump_idx++);
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fwrite(rgba, 1, size, f);
    fclose(f);
}

void debug_dump_rgba_named(const u8* rgba, size_t size, const char* base) {
    static int dump_idx_named = 0;
    if (!g_debug_log_enabled || !rgba || size == 0) return;
    ensure_dirs();
    char path[256];
    if (base && base[0]) snprintf(path, sizeof(path), "%s_%s.bin", DEBUG_ICON_PATH, base);
    else snprintf(path, sizeof(path), "%s_named_%02d.bin", DEBUG_ICON_PATH, dump_idx_named++);
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fwrite(rgba, 1, size, f);
    fclose(f);
}

u32 hash_color(const char* s) {
    const unsigned char* p = (const unsigned char*)(s ? s : "");
    u64 h = 1469598103934665603ULL;
    while (*p) {
        h ^= *p++;
        h *= 1099511628211ULL;
    }
    u8 r = (h >> 8) & 0xFF;
    u8 g = (h >> 16) & 0xFF;
    u8 b = (h >> 24) & 0xFF;
    if (r < 40) r += 40;
    if (g < 40) g += 40;
    if (b < 40) b += 40;
    return (r << 16) | (g << 8) | b;
}

void make_sprite(u8* rgba, u32 color1, u32 color2) {
    if (!rgba) return;
    u8 r1 = (color1 >> 16) & 0xFF;
    u8 g1 = (color1 >> 8) & 0xFF;
    u8 b1 = color1 & 0xFF;
    u8 r2 = (color2 >> 16) & 0xFF;
    u8 g2 = (color2 >> 8) & 0xFF;
    u8 b2 = color2 & 0xFF;
    u8 r_border = r1 > 20 ? r1 - 20 : r1;
    u8 g_border = g1 > 20 ? g1 - 20 : g1;
    u8 b_border = b1 > 20 ? b1 - 20 : b1;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            u8* p = rgba + (y * 32 + x) * 4;
            bool border = (x < 2 || x >= 30 || y < 2 || y >= 30);
            if (border) { p[0] = r_border; p[1] = g_border; p[2] = b_border; p[3] = 0xFF; continue; }
            p[0] = r1; p[1] = g1; p[2] = b1; p[3] = 0xFF;
            if (((x + y) % 6) < 3) {
                p[0] = r2;
                p[1] = g2;
                p[2] = b2;
            }
        }
    }
}

bool write_nextrom_yaml(const char* sd_path) {
    ensure_dirs();
    mkdir(LAUNCH_DIR, 0777);
    FILE* f = fopen(LAUNCH_DIR "/nextrom.yaml", "w");
    if (!f) return false;
    fprintf(f, "rom: \"%s\"\n", sd_path ? sd_path : "");
    fclose(f);
    return true;
}

bool write_nextrom_txt(const char* sd_path) {
    if (!sd_path || sd_path[0] == 0) return false;
    ensure_dirs();
    mkdir("sdmc:/_nds", 0777);
    mkdir("sdmc:/_nds/firmux", 0777);
    char norm[512];
    snprintf(norm, sizeof(norm), "%s", sd_path);
    if (strncasecmp(norm, "cart:", 5) != 0 &&
        strncasecmp(norm, "slot1:", 6) != 0 &&
        strncasecmp(norm, "slot-1:", 7) != 0) {
        normalize_path_sd(norm, sizeof(norm));
    }
    FILE* f = fopen("sdmc:/_nds/firmux/launch.txt", "w");
    if (!f) return false;
    fprintf(f, "%s\n", norm);
    fclose(f);
    return true;
}

bool write_launch_txt_for_nds(const char* sd_path) {
    return write_nextrom_txt(sd_path);
}

bool decode_jpeg_rgba(const unsigned char* jpg, size_t jpg_size, unsigned char** out, unsigned* w, unsigned* h) {
    if (!jpg || jpg_size == 0 || !out || !w || !h) return false;
    *out = NULL; *w = *h = 0;
    int iw = 0, ih = 0, comp = 0;
    unsigned char* data = stbi_load_from_memory(jpg, (int)jpg_size, &iw, &ih, &comp, 4);
    if (!data) return false;
    *out = data;
    *w = (unsigned)iw;
    *h = (unsigned)ih;
    return true;
}
