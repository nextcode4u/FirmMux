#include "fmux.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <3ds.h>

NdsCache g_nds_cache;

static void bgr555_to_rgba(u16 c, u8* out, bool transparent) {
    u8 r = (c & 0x1F) << 3;
    u8 g = ((c >> 5) & 0x1F) << 3;
    u8 b = ((c >> 10) & 0x1F) << 3;
    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = transparent ? 0x00 : 0xFF;
}

static u32 rgba_to_u32(const u8* px) {
    return ((u32)px[3] << 24) | ((u32)px[0] << 16) | ((u32)px[1] << 8) | (u32)px[2];
}

static u32 sample_color_from_rgba(const u8* px, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const u8* c = px + i * 4;
        if (c[3] != 0) return rgba_to_u32(c);
    }
    return rgba_to_u32(px);
}

static void mark_nds_failed(NdsCacheEntry* e) {
    if (!e) return;
    icon_free(&e->icon);
    e->ready = false;
    e->parsed = false;
    e->dirty = true;
    e->in_progress = false;
}

void icon_free(IconTexture* icon) {
    if (icon->loaded) {
        C3D_TexDelete(&icon->tex);
        icon->loaded = false;
    }
}

bool icon_from_rgba(IconTexture* icon, const u8* data, int w, int h) {
    icon_free(icon);
    if (!data) return false;
    int tw = 1;
    while (tw < w) tw <<= 1;
    int th = 1;
    while (th < h) th <<= 1;
    if (!C3D_TexInit(&icon->tex, tw, th, GPU_RGBA8)) return false;
    C3D_TexSetFilter(&icon->tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&icon->tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    size_t buf_size = (size_t)tw * (size_t)th * 4;
    u8* buf = (u8*)linearAlloc(buf_size);
    if (!buf) return false;
    memset(buf, 0, buf_size);
    for (int y = 0; y < h; y++) {
        memcpy(buf + (size_t)y * (size_t)tw * 4, data + (size_t)y * (size_t)w * 4, (size_t)w * 4);
    }
    GSPGPU_FlushDataCache(buf, buf_size);
    if (w > 64 || h > 64) {
        GSPGPU_FlushDataCache(icon->tex.data, icon->tex.size);
        C3D_SyncDisplayTransfer((u32*)buf, GX_BUFFER_DIM(tw, th), (u32*)icon->tex.data, GX_BUFFER_DIM(tw, th),
            GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
            GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
            GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    } else {
        C3D_TexUpload(&icon->tex, buf);
    }
    linearFree(buf);
    icon->image.tex = &icon->tex;
    icon->image.subtex = &icon->subtex;
    icon->subtex.width = w;
    icon->subtex.height = h;
    icon->subtex.left = 0.0f;
    icon->subtex.top = (float)h / (float)th;
    icon->subtex.right = (float)w / (float)tw;
    icon->subtex.bottom = 0.0f;
    icon->loaded = true;
    return true;
}

static u64 path_hash(const char* s) {
    u64 h = 1469598103934665603ULL;
    while (*s) {
        h ^= (u8)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

static bool decode_nds_banner(const u8* data, size_t size, char* title, size_t title_size, u8* rgba_out) {
    if (size < 0x840) return false;
    const u8* icon = data + 0x20;
    const u8* palette = data + 0x220;
    const u8* title_en = data + 0x240;
    if (title_size > 0) {
        size_t len = title_size - 1;
        size_t out = 0;
        for (size_t i = 0; i + 1 < 0x100 && out < len; i += 2) {
            u16 ch = title_en[i] | (title_en[i + 1] << 8);
            if (ch == 0) break;
            if (ch < 128) title[out++] = (char)ch;
        }
        title[out] = 0;
        if (title[0] == 0) snprintf(title, title_size, "NDS");
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
            bgr555_to_rgba(pal[idx2], &rgba_out[p0], idx2 == 0);
            bgr555_to_rgba(pal[idx1], &rgba_out[p1], idx1 == 0);
        }
    }
    return true;
}

static size_t banner_size_from_version(u16 v) {
    switch (v) {
        case 0x0002: return 0x0940;
        case 0x0003: return 0x0A40;
        case 0x0103: return 0x23C0;
        default: return 0x0840;
    }
}

static void nds_cache_filename(const char* rom_path, char* out, size_t out_size) {
    u64 h = path_hash(rom_path);
    snprintf(out, out_size, "%s/%016llx.bin", CACHE_NDS_DIR, (unsigned long long)h);
}

static bool load_nds_cache(const char* rom_path, u64 size, u64 mtime, NdsCacheEntry* out_entry) {
    char path[256];
    nds_cache_filename(rom_path, path, sizeof(path));
    u8* data = NULL;
    size_t sz = 0;
    if (!read_file(path, &data, &sz)) return false;
    bool ok = false;
    if (sz >= sizeof(u32) * 2 + sizeof(u64) * 2 + sizeof(u32) + sizeof(u32) + sizeof(u32) + 128 + 32 * 32 * 4) {
        const u8* p = data;
        u32 magic = *(u32*)p; p += 4;
        u32 ver = *(u32*)p; p += 4;
        u64 fsize = *(u64*)p; p += 8;
        u64 fmtime = *(u64*)p; p += 8;
        u32 banner_off = *(u32*)p; p += 4;
        u32 parsed = *(u32*)p; p += 4;
        u32 sample = *(u32*)p; p += 4;
        if (magic == NDS_CACHE_MAGIC && ver == 4 && fsize == size && fmtime == mtime) {
            copy_str(out_entry->path, sizeof(out_entry->path), rom_path);
            out_entry->size = size;
            out_entry->mtime = mtime;
            out_entry->banner_off = banner_off;
            memcpy(out_entry->title, p, 128);
            p += 128;
            out_entry->parsed = parsed != 0;
            out_entry->sample_color = sample;
            u8 rgba[32 * 32 * 4];
            memcpy(rgba, p, sizeof(rgba));
            icon_from_rgba(&out_entry->icon, rgba, 32, 32);
            out_entry->ready = out_entry->parsed;
            out_entry->dirty = banner_off == 0;
            memcpy(out_entry->rgba, rgba, sizeof(rgba));
            out_entry->has_rgba = true;
            ok = out_entry->ready;
            if (ok) debug_log("nds cache hit %s parsed=%d off=%u", rom_path, out_entry->parsed ? 1 : 0, out_entry->banner_off);
        }
    }
    free(data);
    if (!ok) {
        char bak[300];
        snprintf(bak, sizeof(bak), "%s.bak", path);
        rename(path, bak);
        debug_log("nds cache miss %s", rom_path);
    }
    return ok;
}

static void save_nds_cache(const NdsCacheEntry* entry, const u8* rgba) {
    char path[256];
    nds_cache_filename(entry->path, path, sizeof(path));
    FILE* f = fopen(path, "wb");
    if (!f) return;
    u32 magic = NDS_CACHE_MAGIC;
    u32 ver = 4;
    fwrite(&magic, 1, 4, f);
    fwrite(&ver, 1, 4, f);
    fwrite(&entry->size, 1, 8, f);
    fwrite(&entry->mtime, 1, 8, f);
    fwrite(&entry->banner_off, 1, 4, f);
    u32 parsed = entry->parsed ? 1 : 0;
    fwrite(&parsed, 1, 4, f);
    fwrite(&entry->sample_color, 1, 4, f);
    char titlebuf[128];
    memset(titlebuf, 0, sizeof(titlebuf));
    copy_str(titlebuf, sizeof(titlebuf), entry->title);
    fwrite(titlebuf, 1, sizeof(titlebuf), f);
    fwrite(rgba, 1, 32 * 32 * 4, f);
    fclose(f);
}

NdsCacheEntry* nds_cache_entry(const char* path) {
    for (int i = 0; i < g_nds_cache.count; i++) {
        if (!strcmp(g_nds_cache.entries[i].path, path)) return &g_nds_cache.entries[i];
    }
    if (g_nds_cache.count >= MAX_ENTRIES) return NULL;
    NdsCacheEntry* e = &g_nds_cache.entries[g_nds_cache.count++];
    memset(e, 0, sizeof(*e));
    copy_str(e->path, sizeof(e->path), path);
    const char* base = strrchr(path, '/');
    if (!base) base = path; else base++;
    copy_str(e->title, sizeof(e->title), base);
    return e;
}

BannerBlob load_banner_blob(const char* full_path) {
    BannerBlob b = {0};
    struct stat st;
    if (stat(full_path, &st) != 0) return b;
    FILE* f = fopen(full_path, "rb");
    if (!f) return b;
    u8 header[0x70];
    size_t hr = fread(header, 1, sizeof(header), f);
    if (hr < 0x6C) { fclose(f); return b; }
    u32 banner_off = (u32)header[0x68] | ((u32)header[0x69] << 8) | ((u32)header[0x6A] << 16) | ((u32)header[0x6B] << 24);
    if (banner_off == 0 || banner_off >= (u64)st.st_size) { fclose(f); return b; }
    u16 ver = 0;
    if (fseek(f, banner_off, SEEK_SET) != 0) { fclose(f); return b; }
    if (fread(&ver, 1, sizeof(ver), f) < sizeof(ver)) { fclose(f); return b; }
    size_t expected = banner_size_from_version(ver);
    if (expected < 0x840) expected = 0x840;
    if (banner_off + expected > (u64)st.st_size) expected = (size_t)((u64)st.st_size - banner_off);
    if (expected < 0x840) { fclose(f); return b; }
    if (fseek(f, banner_off, SEEK_SET) != 0) { fclose(f); return b; }
    u8* buf = (u8*)malloc(expected);
    if (!buf) { fclose(f); return b; }
    size_t r = fread(buf, 1, expected, f);
    fclose(f);
    if (r < 0x840) { free(buf); return b; }
    b.data = buf;
    b.size = expected;
    b.offset = banner_off;
    debug_log("nds banner blob %s off=%u size=%lu ver=%u", full_path, banner_off, (unsigned long)expected, ver);
    return b;
}

void free_banner_blob(BannerBlob* b) {
    if (!b) return;
    if (b->data) free(b->data);
    b->data = NULL;
    b->size = 0;
}

bool decode_banner_blob(const BannerBlob* b, NdsCacheEntry* e, u8* rgba_out) {
    if (!b || !b->data || b->size < 0x840 || !e) return false;
    char title[128];
    u8 rgba[32 * 32 * 4];
    if (!decode_nds_banner(b->data, b->size, title, sizeof(title), rgba)) {
        debug_log("nds decode fail %s size=%lu off=%u", e->path, (unsigned long)b->size, (unsigned)b->offset);
        return false;
    }
    static int dump_count = 0;
    if (dump_count < 3) {
        char base[64];
        snprintf(base, sizeof(base), "icon_%02d_%s", dump_count, strrchr(e->path, '/') ? strrchr(e->path, '/') + 1 : e->path);
        debug_dump_rgba_named(rgba, sizeof(rgba), base);
        debug_log("nds rgba dump %d %s off=%u", dump_count, e->path, (unsigned)b->offset);
        dump_count++;
    }
    copy_str(e->title, sizeof(e->title), title);
    e->sample_color = sample_color_from_rgba(rgba, 32 * 32);
    e->parsed = true;
    e->ready = true;
    memcpy(e->rgba, rgba, sizeof(rgba));
    e->has_rgba = true;
    e->dirty = false;
    debug_log("nds decoded %s off=%u sample=%08x", e->path, (unsigned)b->offset, e->sample_color);
    if (rgba_out) memcpy(rgba_out, rgba, sizeof(rgba));
    return true;
}

bool load_nds_icon_direct(const char* full_path, NdsCacheEntry* e) {
    const char* base = strrchr(full_path, '/');
    base = base ? base + 1 : full_path;
    if (!is_nds_name(base)) { mark_nds_failed(e); return false; }
    if (e->dirty || e->in_progress) return false;
    e->in_progress = true;
    struct stat st;
    if (stat(full_path, &st) != 0) { mark_nds_failed(e); return false; }
    BannerBlob blob = load_banner_blob(full_path);
    if (!blob.data) { mark_nds_failed(e); debug_log("nds blob missing %s", full_path); return false; }
    u8 rgba[32 * 32 * 4];
    bool ok = decode_banner_blob(&blob, e, rgba);
    e->banner_off = blob.offset;
    e->size = st.st_size;
    e->mtime = st.st_mtime;
    if (ok) {
        save_nds_cache(e, rgba);
        debug_log("nds direct ok %s off=%u", full_path, e->banner_off);
    }
    else e->has_rgba = true;
    free_banner_blob(&blob);
    e->in_progress = false;
    return ok;
}

void build_nds_entry_blob(const char* full_path, const BannerBlob* blob) {
    if (!blob || !blob->data) return;
    NdsCacheEntry* e = nds_cache_entry(full_path);
    if (!e || e->in_progress) return;
    if (e->ready && e->parsed && e->icon.loaded && !e->dirty) return;
    struct stat st;
    if (stat(full_path, &st) != 0) { mark_nds_failed(e); return; }
    e->size = st.st_size;
    e->mtime = st.st_mtime;
    u8 rgba[32 * 32 * 4];
    if (!decode_banner_blob(blob, e, rgba)) {
        mark_nds_failed(e);
        return;
    }
    e->banner_off = blob->offset;
    e->ready = true;
    save_nds_cache(e, rgba);
}

void build_nds_entry(const char* full_path) {
    const char* base = strrchr(full_path, '/');
    base = base ? base + 1 : full_path;
    if (!is_nds_name(base)) return;
    struct stat st;
    if (stat(full_path, &st) != 0) { mark_nds_failed(nds_cache_entry(full_path)); return; }
    NdsCacheEntry* e = nds_cache_entry(full_path);
    if (!e || e->in_progress) return;
    if (e->ready && e->parsed && e->icon.loaded && !e->dirty) return;
    e->size = st.st_size;
    e->mtime = st.st_mtime;
    if (load_nds_cache(full_path, e->size, e->mtime, e)) {
        if (e->parsed && e->icon.loaded && !e->dirty) return;
        char path[256];
        nds_cache_filename(full_path, path, sizeof(path));
        char bak[300];
        snprintf(bak, sizeof(bak), "%s.bad", path);
        rename(path, bak);
        e->ready = false;
        e->dirty = true;
        e->has_rgba = false;
    }
    e->in_progress = true;
    BannerBlob blob = load_banner_blob(full_path);
    if (!blob.data) { mark_nds_failed(e); debug_log("nds build blob missing %s", full_path); return; }
    u8 rgba[32 * 32 * 4];
    bool ok = decode_banner_blob(&blob, e, rgba);
    e->banner_off = blob.offset;
    if (ok) {
        save_nds_cache(e, rgba);
        debug_log("nds built %s off=%u", full_path, e->banner_off);
    }
    else e->has_rgba = true;
    free_banner_blob(&blob);
    e->in_progress = false;
}
