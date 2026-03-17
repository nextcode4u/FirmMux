#include "preview_manager.h"
#include "fmux.h"
#include "stb_image.h"

#include <string.h>
#include <stdlib.h>

#define PREVIEW_MAX_W 96
#define PREVIEW_MAX_H 96

static char g_pending_path[640];
static u32 g_pending_generation = 0;
static bool g_pending_valid = false;

static char g_ready_path[640];
static u32 g_ready_generation = 0;
static bool g_ready_valid = false;
static int g_ready_w = 0;
static int g_ready_h = 0;
static u8 g_ready_rgba[PREVIEW_MAX_W * PREVIEW_MAX_H * 4];

static bool load_png_preview_rgba(const char* path, u8* out_rgba, size_t out_rgba_size, int* out_w, int* out_h) {
    if (!path || !path[0] || !out_rgba || out_rgba_size < (PREVIEW_MAX_W * PREVIEW_MAX_H * 4)) return false;
    u8* file = NULL;
    size_t fsize = 0;
    if (debug_log_enabled()) debug_log("cover: preview load start path=%s", path);
    if (!read_file(path, &file, &fsize) || !file || fsize == 0) {
        if (debug_log_enabled()) debug_log("cover: read fail path=%s size=%lu", path ? path : "(null)", (unsigned long)fsize);
        if (file) free(file);
        return false;
    }
    if (debug_log_enabled()) debug_log("cover: preview read ok path=%s size=%lu", path, (unsigned long)fsize);
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load_from_memory(file, (int)fsize, &w, &h, &comp, 4);
    free(file);
    if (!data || w <= 0 || h <= 0) {
        if (debug_log_enabled()) debug_log("cover: decode fail path=%s decoded=%p w=%d h=%d comp=%d", path, (void*)data, w, h, comp);
        if (data) stbi_image_free(data);
        return false;
    }
    if (debug_log_enabled()) debug_log("cover: decode ok path=%s src=%dx%d comp=%d", path, w, h, comp);

    int tw = w;
    int th = h;
    if (tw > PREVIEW_MAX_W || th > PREVIEW_MAX_H) {
        float sx = (float)PREVIEW_MAX_W / (float)tw;
        float sy = (float)PREVIEW_MAX_H / (float)th;
        float s = (sx < sy) ? sx : sy;
        tw = (int)((float)tw * s);
        th = (int)((float)th * s);
        if (tw < 1) tw = 1;
        if (th < 1) th = 1;
    }

    memset(out_rgba, 0, out_rgba_size);
    if (tw == w && th == h) {
        memcpy(out_rgba, data, (size_t)w * (size_t)h * 4);
    } else {
        for (int y = 0; y < th; y++) {
            int sy = (y * h) / th;
            if (sy < 0) sy = 0;
            if (sy >= h) sy = h - 1;
            for (int x = 0; x < tw; x++) {
                int sx = (x * w) / tw;
                if (sx < 0) sx = 0;
                if (sx >= w) sx = w - 1;
                const u8* src = data + ((size_t)sy * (size_t)w + (size_t)sx) * 4;
                u8* dst = out_rgba + ((size_t)y * (size_t)tw + (size_t)x) * 4;
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = src[3];
            }
        }
    }
    stbi_image_free(data);
    if (debug_log_enabled()) debug_log("cover: ready path=%s out=%dx%d", path, tw, th);
    if (out_w) *out_w = tw;
    if (out_h) *out_h = th;
    return true;
}

void preview_manager_init(void) {
    g_pending_path[0] = 0;
    g_pending_generation = 0;
    g_pending_valid = false;
    g_ready_path[0] = 0;
    g_ready_generation = 0;
    g_ready_valid = false;
    g_ready_w = 0;
    g_ready_h = 0;
}

void preview_manager_shutdown(void) {
    preview_manager_init();
}

void preview_request(const char* path, u32 generation) {
    if (!path || !path[0]) return;
    if (g_ready_valid && g_ready_generation == generation && !strcmp(g_ready_path, path)) return;
    if (debug_log_enabled()) debug_log("cover: preview request path=%s gen=%lu", path, (unsigned long)generation);

    int w = 0, h = 0;
    if (!load_png_preview_rgba(path, g_ready_rgba, sizeof(g_ready_rgba), &w, &h)) {
        if (debug_log_enabled()) debug_log("cover: request load failed %s", path);
        g_ready_valid = false;
        g_ready_w = 0;
        g_ready_h = 0;
        g_ready_path[0] = 0;
        g_ready_generation = generation;
        return;
    }

    copy_str(g_ready_path, sizeof(g_ready_path), path);
    g_ready_generation = generation;
    g_ready_valid = true;
    g_ready_w = w;
    g_ready_h = h;
}

void preview_cancel(u32 generation) {
    g_pending_valid = false;
    g_pending_path[0] = 0;
    g_pending_generation = generation;
    g_ready_valid = false;
    g_ready_path[0] = 0;
    g_ready_generation = generation;
    g_ready_w = 0;
    g_ready_h = 0;
}

void preview_update(int budget) {
    (void)budget;
}

bool preview_get_ready_texture(u32 generation, const u8** out_rgba, int* out_w, int* out_h) {
    if (!g_ready_valid || g_ready_w <= 0 || g_ready_h <= 0) return false;
    if (generation != 0 && g_ready_generation != generation) return false;
    if (out_rgba) *out_rgba = g_ready_rgba;
    if (out_w) *out_w = g_ready_w;
    if (out_h) *out_h = g_ready_h;
    return true;
}
