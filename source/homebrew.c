#include "fmux.h"
#include "smdh.h"
#include <string.h>
#include <stdio.h>
#include <3ds.h>
#include <3ds/util/utf.h>

#define _3DSX_MAGIC 0x58534433

typedef struct {
    u32 magic;
    u16 headerSize;
    u16 relocHdrSize;
    u32 formatVer;
    u32 flags;
    u32 codeSegSize;
    u32 rodataSegSize;
    u32 dataSegSize;
    u32 bssSize;
    u32 smdhOffset;
    u32 smdhSize;
    u32 fsOffset;
} _3DSX_Header;

static bool read_smdh_at(FILE* f, long offset, smdh_s* out) {
    if (!f || !out) return false;
    if (fseek(f, offset, SEEK_SET) != 0) return false;
    if (fread(out, 1, sizeof(*out), f) != sizeof(*out)) return false;
    return out->header.magic == 0x48444D53;
}

static bool load_smdh_file(const char* path, smdh_s* out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    bool ok = read_smdh_at(f, 0, out);
    fclose(f);
    return ok;
}

static bool load_smdh_external(const char* sd_path, smdh_s* out) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", sd_path ? sd_path : "");
    char* dot = strrchr(tmp, '.');
    if (dot && !strcasecmp(dot, ".3dsx")) {
        snprintf(dot, sizeof(tmp) - (dot - tmp), ".smdh");
        if (load_smdh_file(tmp, out)) return true;
    }
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", sd_path ? sd_path : "");
    char* slash = strrchr(dir, '/');
    if (slash) {
        *slash = 0;
        char icon_path[512];
        snprintf(icon_path, sizeof(icon_path), "%s/icon.smdh", dir);
        if (load_smdh_file(icon_path, out)) return true;
    }
    return false;
}

static bool load_smdh_embedded(const char* sd_path, smdh_s* out) {
    FILE* f = fopen(sd_path, "rb");
    if (!f) return false;
    _3DSX_Header hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) { fclose(f); return false; }
    if (hdr.magic != _3DSX_MAGIC) { fclose(f); return false; }
    if (hdr.smdhOffset == 0 || hdr.smdhSize < sizeof(*out)) { fclose(f); return false; }
    bool ok = read_smdh_at(f, (long)hdr.smdhOffset, out);
    fclose(f);
    return ok;
}

static size_t utf16_ascii_to_utf8(char* out, size_t out_size, const u16* src, size_t max_units) {
    if (!out || out_size == 0 || !src) return 0;
    size_t oi = 0;
    for (size_t i = 0; i < max_units && src[i]; i++) {
        u16 ch = src[i];
        char c = (ch < 0x80) ? (char)ch : '?';
        if (oi + 1 >= out_size) break;
        out[oi++] = c;
    }
    out[oi] = 0;
    return oi;
}

static bool smdh_copy_text(const u16* src, size_t max_units, char* out, size_t out_size) {
    if (!src || !out || out_size == 0) return false;
    size_t units = utf16_ascii_to_utf8(out, out_size, src, max_units);
    if (units == 0) return false;
    while (*out == ' ' || *out == '\t') memmove(out, out + 1, strlen(out));
    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\t')) out[--len] = 0;
    return out[0] != 0;
}

static bool smdh_pick_name(const smdh_s* smdh, char* out, size_t out_size) {
    int order[16] = {1,0,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    for (int i = 0; i < 16; i++) {
        int idx = order[i];
        if (smdh_copy_text(smdh->applicationTitles[idx].shortDescription, 0x40, out, out_size)) return true;
    }
    for (int i = 0; i < 16; i++) {
        int idx = order[i];
        if (smdh_copy_text(smdh->applicationTitles[idx].longDescription, 0x80, out, out_size)) return true;
    }
    for (int i = 0; i < 16; i++) {
        int idx = order[i];
        if (smdh_copy_text(smdh->applicationTitles[idx].publisher, 0x40, out, out_size)) return true;
    }
    return false;
}

bool homebrew_load_meta(const char* sd_path, char* title_out, size_t title_size, u16* icon_out, size_t icon_count) {
    if (!sd_path || !sd_path[0]) return false;
    smdh_s smdh;
    bool ok = load_smdh_external(sd_path, &smdh);
    if (!ok) ok = load_smdh_embedded(sd_path, &smdh);
    if (!ok) return false;
    if (title_out && title_size > 0) {
        if (!smdh_pick_name(&smdh, title_out, title_size)) {
            title_out[0] = 0;
        }
    }
    if (icon_out && icon_count >= 48 * 48) {
        memcpy(icon_out, smdh.bigIconData, 48 * 48 * sizeof(u16));
    }
    return true;
}

static Result hbldr_set_target(Handle hbldr, const char* path) {
    u32 len = (u32)strlen(path) + 1;
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(2, 0, 2);
    cmdbuf[1] = IPC_Desc_StaticBuffer(len, 0);
    cmdbuf[2] = (u32)path;
    Result rc = svcSendSyncRequest(hbldr);
    if (R_SUCCEEDED(rc)) rc = cmdbuf[1];
    return rc;
}

static Result hbldr_set_argv(Handle hbldr, const void* buffer, u32 size) {
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(3, 0, 2);
    cmdbuf[1] = IPC_Desc_StaticBuffer(size, 1);
    cmdbuf[2] = (u32)buffer;
    Result rc = svcSendSyncRequest(hbldr);
    if (R_SUCCEEDED(rc)) rc = cmdbuf[1];
    return rc;
}

bool homebrew_launch_3dsx(const char* sd_path, char* status_message, size_t status_size) {
    if (status_message && status_size > 0) status_message[0] = 0;
    if (!sd_path || !sd_path[0]) return false;
    char norm[512];
    snprintf(norm, sizeof(norm), "%s", sd_path);
    normalize_path_sd(norm, sizeof(norm));
    if (!strncmp(norm, "sd:/", 4)) {
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "sdmc:/%s", norm + 4);
        copy_str(norm, sizeof(norm), tmp);
    }
    const char* target = norm;
    if (!strncmp(target, "sdmc:/", 6)) target += 5;
    Handle hbldr = 0;
    Result rc = svcConnectToPort(&hbldr, "hb:ldr");
    if (R_FAILED(rc)) {
        if (status_message && status_size > 0) snprintf(status_message, status_size, "hb:ldr not available");
        return false;
    }
    rc = hbldr_set_target(hbldr, target);
    if (R_FAILED(rc)) {
        svcCloseHandle(hbldr);
        if (status_message && status_size > 0) snprintf(status_message, status_size, "hb:ldr target failed");
        return false;
    }
    u32 args[0x400 / 4] = {0};
    char* dst = (char*)&args[1];
    size_t max = sizeof(args) - sizeof(u32);
    size_t len = strlen(norm) + 1;
    if (len > max) len = max;
    memcpy(dst, norm, len);
    args[0] = 1;
    rc = hbldr_set_argv(hbldr, args, sizeof(args));
    svcCloseHandle(hbldr);
    if (R_FAILED(rc)) {
        if (status_message && status_size > 0) snprintf(status_message, status_size, "hb:ldr argv failed");
        return false;
    }
    return true;
}
