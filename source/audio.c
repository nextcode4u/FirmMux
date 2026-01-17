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
    out->buf.nsamples = data_size / 2;
    out->buf.looping = false;
    out->buf.status = NDSP_WBUF_FREE;
    out->loaded = true;
    return true;
}

bool audio_init(void) {
    if (g_audio_ready) return true;
    if (ndspInit() != 0) return false;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    const char* paths[SOUND_MAX] = {
        "sdmc:/3ds/FirmMux/ui sounds/tap_01.wav",
        "sdmc:/3ds/FirmMux/ui sounds/select.wav",
        "sdmc:/3ds/FirmMux/ui sounds/toggle_off.wav",
        "sdmc:/3ds/FirmMux/ui sounds/swipe_01.wav",
        "sdmc:/3ds/FirmMux/ui sounds/toggle_on.wav",
        "sdmc:/3ds/FirmMux/ui sounds/caution.wav"
    };
    for (int i = 0; i < SOUND_MAX; i++) {
        load_wav(paths[i], &g_sounds[i]);
    }
    g_audio_ready = true;
    return true;
}

void audio_play(int id) {
    if (!g_audio_ready) return;
    if (id < 0 || id >= SOUND_MAX) return;
    Sound* s = &g_sounds[id];
    if (!s->loaded) return;
    int ch = g_next_channel % 8;
    g_next_channel++;
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, (float)s->rate);
    ndspChnSetFormat(ch, s->format);
    s->buf.status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(ch, &s->buf);
}

