#include "fmux.h"
#include <string.h>
#include <stdlib.h>
#include <3ds/ndsp/channel.h>

typedef struct {
    ndspWaveBuf buf;
    void* data;
    size_t data_size;
    int format;
    int rate;
    bool loaded;
} Sound;

static Sound g_sounds[SOUND_MAX];
static int g_next_channel = 0;
static bool g_audio_ready = false;
static Sound g_bgm;
static bool g_bgm_playing = false;
static bool g_bgm_enabled = true;
static char g_ui_sounds_dir[192] = "sdmc:/3ds/FirmMux/ui sounds";
static char g_bgm_path[192] = "sdmc:/3ds/FirmMux/bgm/bgm.wav";

static void normalize_sdmc_path(char* out, size_t out_size, const char* in) {
    if (!out || out_size == 0) return;
    if (!in || !in[0]) {
        out[0] = 0;
        return;
    }
    if (!strncasecmp(in, "sd:/", 4)) {
        snprintf(out, out_size, "sdmc:/%s", in + 4);
        return;
    }
    copy_str(out, out_size, in);
}

static bool strip_trailing_component(const char* in, const char* comp, char* out, size_t out_size) {
    if (!in || !in[0] || !out || out_size == 0) return false;
    size_t in_len = strlen(in);
    size_t comp_len = strlen(comp);
    if (in_len <= comp_len + 1) return false;
    if (in[in_len - comp_len - 1] != '/') return false;
    if (strncasecmp(in + in_len - comp_len, comp, comp_len) != 0) return false;
    size_t new_len = in_len - comp_len - 1;
    if (new_len >= out_size) new_len = out_size - 1;
    memcpy(out, in, new_len);
    out[new_len] = 0;
    return true;
}

static void sound_free(Sound* s) {
    if (!s) return;
    if (s->data) linearFree(s->data);
    memset(s, 0, sizeof(Sound));
}

static void build_sound_path(char* out, size_t out_size, const char* dir, const char* name) {
    if (!out || out_size == 0) return;
    if (!dir || !dir[0]) {
        snprintf(out, out_size, "%s", name);
        return;
    }
    snprintf(out, out_size, "%s/%s", dir, name);
}

static bool load_wav(const char* path, Sound* out) {
    u8* file = NULL;
    size_t size = 0;
    if (!read_file(path, &file, &size)) return false;
    if (size < 44) { free(file); return false; }
    if (memcmp(file, "RIFF", 4) || memcmp(file + 8, "WAVE", 4)) { free(file); return false; }
    u16 fmt_code = 0;
    u16 channels = 0;
    u32 sample_rate = 0;
    u16 bits = 0;
    size_t data_off = 0;
    size_t data_size = 0;
    size_t pos = 12;
    while (pos + 8 <= size) {
        u32 cid = *(u32*)(file + pos);
        u32 clen = *(u32*)(file + pos + 4);
        pos += 8;
        if (pos + clen > size) break;
        if (cid == 0x20746D66) {
            if (clen >= 16) {
                fmt_code = *(u16*)(file + pos + 0);
                channels = *(u16*)(file + pos + 2);
                sample_rate = *(u32*)(file + pos + 4);
                bits = *(u16*)(file + pos + 14);
            }
        } else if (cid == 0x61746164) {
            data_off = pos;
            data_size = clen;
        }
        pos += clen;
        if (pos & 1) pos++;
    }
    if (fmt_code != 1 || (channels != 1 && channels != 2) || bits != 16 || data_size == 0) { free(file); return false; }
    size_t block_align = (size_t)channels * 2;
    if (block_align == 0) { free(file); return false; }
    if (data_size % block_align) {
        data_size -= (data_size % block_align);
    }
    if (data_off + data_size > size) { free(file); return false; }
    void* pcm = linearAlloc(data_size);
    if (!pcm) { free(file); return false; }
    memcpy(pcm, file + data_off, data_size);
    GSPGPU_FlushDataCache(pcm, data_size);
    free(file);
    memset(out, 0, sizeof(Sound));
    out->data = pcm;
    out->data_size = data_size;
    out->format = (channels == 1) ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16;
    out->rate = (int)sample_rate;
    out->buf.data_vaddr = pcm;
    out->buf.nsamples = data_size / block_align;
    out->buf.looping = false;
    out->buf.status = NDSP_WBUF_FREE;
    out->loaded = true;
    return true;
}

bool audio_init(void) {
    if (g_audio_ready) return true;
    if (ndspInit() != 0) return false;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetMasterVol(0.8f);
    const char* names[SOUND_MAX] = {
        "tap_01.wav",
        "select.wav",
        "toggle_off.wav",
        "swipe_01.wav",
        "toggle_on.wav",
        "caution.wav"
    };
    for (int i = 0; i < SOUND_MAX; i++) {
        char path[256];
        bool ok = false;
        build_sound_path(path, sizeof(path), g_ui_sounds_dir, names[i]);
        ok = load_wav(path, &g_sounds[i]);
        if (!ok) {
            const char* alt = NULL;
            if (!strcasecmp(names[i], "tap_01.wav")) alt = "tap.wav";
            else if (!strcasecmp(names[i], "swipe_01.wav")) alt = "swipe.wav";
            if (alt) {
                build_sound_path(path, sizeof(path), g_ui_sounds_dir, alt);
                ok = load_wav(path, &g_sounds[i]);
            }
        }
        if (!ok && strcasecmp(g_ui_sounds_dir, "sdmc:/3ds/FirmMux/ui sounds") != 0) {
            build_sound_path(path, sizeof(path), "sdmc:/3ds/FirmMux/ui sounds", names[i]);
            ok = load_wav(path, &g_sounds[i]);
            if (!ok) {
                const char* alt = NULL;
                if (!strcasecmp(names[i], "tap_01.wav")) alt = "tap.wav";
                else if (!strcasecmp(names[i], "swipe_01.wav")) alt = "swipe.wav";
                if (alt) {
                    build_sound_path(path, sizeof(path), "sdmc:/3ds/FirmMux/ui sounds", alt);
                    load_wav(path, &g_sounds[i]);
                }
            }
        }
    }
    if (g_bgm_path[0] && load_wav(g_bgm_path, &g_bgm)) {
        ndspChnReset(0);
        ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
        ndspChnSetRate(0, (float)g_bgm.rate);
        ndspChnSetFormat(0, g_bgm.format);
        float mix[12] = {0};
        mix[0] = 0.5f;
        mix[1] = 0.5f;
        ndspChnSetMix(0, mix);
        g_bgm.buf.looping = true;
        g_bgm.buf.status = NDSP_WBUF_FREE;
        if (g_bgm_enabled) {
            ndspChnWaveBufAdd(0, &g_bgm.buf);
            g_bgm_playing = true;
        }
    }
    g_audio_ready = true;
    return true;
}

void audio_play(int id) {
    if (!g_audio_ready) return;
    if (id < 0 || id >= SOUND_MAX) return;
    Sound* s = &g_sounds[id];
    if (!s->loaded) return;
    int ch = 1 + (g_next_channel % 7);
    g_next_channel++;
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, (float)s->rate);
    ndspChnSetFormat(ch, s->format);
    s->buf.status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(ch, &s->buf);
}

void audio_set_bgm_enabled(bool enabled) {
    g_bgm_enabled = enabled;
    if (!g_audio_ready || !g_bgm.loaded) return;
    if (!enabled) {
        ndspChnWaveBufClear(0);
        ndspChnReset(0);
        g_bgm_playing = false;
        return;
    }
    if (!g_bgm_playing) {
        ndspChnReset(0);
        ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
        ndspChnSetRate(0, (float)g_bgm.rate);
        ndspChnSetFormat(0, g_bgm.format);
        float mix[12] = {0};
        mix[0] = 0.5f;
        mix[1] = 0.5f;
        ndspChnSetMix(0, mix);
        g_bgm.buf.looping = true;
        g_bgm.buf.status = NDSP_WBUF_FREE;
        ndspChnWaveBufAdd(0, &g_bgm.buf);
        g_bgm_playing = true;
    }
}

void audio_set_theme_paths(const char* ui_sounds_dir, const char* bgm_path) {
    if (ui_sounds_dir && ui_sounds_dir[0]) normalize_sdmc_path(g_ui_sounds_dir, sizeof(g_ui_sounds_dir), ui_sounds_dir);
    else copy_str(g_ui_sounds_dir, sizeof(g_ui_sounds_dir), "sdmc:/3ds/FirmMux/ui sounds");
    if (bgm_path && bgm_path[0]) normalize_sdmc_path(g_bgm_path, sizeof(g_bgm_path), bgm_path);
    else copy_str(g_bgm_path, sizeof(g_bgm_path), "sdmc:/3ds/FirmMux/bgm/bgm.wav");

    if (!g_audio_ready) return;

    char parent_dir[192] = {0};
    bool has_parent = strip_trailing_component(g_ui_sounds_dir, "sounds", parent_dir, sizeof(parent_dir));
    if (debug_log_enabled()) {
        debug_log("audio: ui_sounds_dir=%s", g_ui_sounds_dir);
        if (has_parent) debug_log("audio: ui_sounds_parent=%s", parent_dir);
        debug_log("audio: bgm_path=%s", g_bgm_path);
    }

    for (int i = 0; i < SOUND_MAX; i++) {
        sound_free(&g_sounds[i]);
    }
    sound_free(&g_bgm);
    g_bgm_playing = false;

    const char* names[SOUND_MAX] = {
        "tap_01.wav",
        "select.wav",
        "toggle_off.wav",
        "swipe_01.wav",
        "toggle_on.wav",
        "caution.wav"
    };
    for (int i = 0; i < SOUND_MAX; i++) {
        char path[256];
        bool ok = false;
        build_sound_path(path, sizeof(path), g_ui_sounds_dir, names[i]);
        ok = load_wav(path, &g_sounds[i]);
        if (!ok) {
            const char* alt = NULL;
            if (!strcasecmp(names[i], "tap_01.wav")) alt = "tap.wav";
            else if (!strcasecmp(names[i], "swipe_01.wav")) alt = "swipe.wav";
            if (alt) {
                build_sound_path(path, sizeof(path), g_ui_sounds_dir, alt);
                ok = load_wav(path, &g_sounds[i]);
            }
        }
        if (!ok && has_parent) {
            build_sound_path(path, sizeof(path), parent_dir, names[i]);
            ok = load_wav(path, &g_sounds[i]);
            if (!ok) {
                const char* alt = NULL;
                if (!strcasecmp(names[i], "tap_01.wav")) alt = "tap.wav";
                else if (!strcasecmp(names[i], "swipe_01.wav")) alt = "swipe.wav";
                if (alt) {
                    build_sound_path(path, sizeof(path), parent_dir, alt);
                    ok = load_wav(path, &g_sounds[i]);
                }
            }
        }
        if (!ok && strcasecmp(g_ui_sounds_dir, "sdmc:/3ds/FirmMux/ui sounds") != 0) {
            build_sound_path(path, sizeof(path), "sdmc:/3ds/FirmMux/ui sounds", names[i]);
            ok = load_wav(path, &g_sounds[i]);
            if (!ok) {
                const char* alt = NULL;
                if (!strcasecmp(names[i], "tap_01.wav")) alt = "tap.wav";
                else if (!strcasecmp(names[i], "swipe_01.wav")) alt = "swipe.wav";
                if (alt) {
                    build_sound_path(path, sizeof(path), "sdmc:/3ds/FirmMux/ui sounds", alt);
                    load_wav(path, &g_sounds[i]);
                }
            }
        }
        if (debug_log_enabled()) debug_log("audio: sound %s ok=%d", names[i], ok ? 1 : 0);
    }
    if ((!g_bgm_path[0] || !file_exists(g_bgm_path)) && g_ui_sounds_dir[0]) {
        char tmp[256];
        build_sound_path(tmp, sizeof(tmp), g_ui_sounds_dir, "bgm.wav");
        if (file_exists(tmp)) copy_str(g_bgm_path, sizeof(g_bgm_path), tmp);
        else if (has_parent) {
            build_sound_path(tmp, sizeof(tmp), parent_dir, "bgm.wav");
            if (file_exists(tmp)) copy_str(g_bgm_path, sizeof(g_bgm_path), tmp);
        }
    }
    if (g_bgm_path[0] && load_wav(g_bgm_path, &g_bgm)) {
        ndspChnReset(0);
        ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
        ndspChnSetRate(0, (float)g_bgm.rate);
        ndspChnSetFormat(0, g_bgm.format);
        float mix[12] = {0};
        mix[0] = 0.5f;
        mix[1] = 0.5f;
        ndspChnSetMix(0, mix);
        g_bgm.buf.looping = true;
        g_bgm.buf.status = NDSP_WBUF_FREE;
        if (g_bgm_enabled) {
            ndspChnWaveBufAdd(0, &g_bgm.buf);
            g_bgm_playing = true;
        }
        if (debug_log_enabled()) debug_log("audio: bgm ok=1");
    } else if (strcasecmp(g_bgm_path, "sdmc:/3ds/FirmMux/bgm/bgm.wav") != 0) {
        if (load_wav("sdmc:/3ds/FirmMux/bgm/bgm.wav", &g_bgm)) {
            ndspChnReset(0);
            ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
            ndspChnSetRate(0, (float)g_bgm.rate);
            ndspChnSetFormat(0, g_bgm.format);
            float mix[12] = {0};
            mix[0] = 0.5f;
            mix[1] = 0.5f;
            ndspChnSetMix(0, mix);
            g_bgm.buf.looping = true;
            g_bgm.buf.status = NDSP_WBUF_FREE;
            if (g_bgm_enabled) {
                ndspChnWaveBufAdd(0, &g_bgm.buf);
                g_bgm_playing = true;
            }
            if (debug_log_enabled()) debug_log("audio: bgm ok=1 (fallback)");
        }
    } else if (debug_log_enabled()) {
        debug_log("audio: bgm ok=0");
    }
}
