#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static unsigned char* read_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    unsigned char* buf = (unsigned char*)malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (n != (size_t)size) {
        free(buf);
        return NULL;
    }
    if (out_size) *out_size = (size_t)size;
    return buf;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <in.png> <out.png> <w> <h>\n", argv[0]);
        return 1;
    }
    const char* in_path = argv[1];
    const char* out_path = argv[2];
    int out_w = atoi(argv[3]);
    int out_h = atoi(argv[4]);
    if (out_w <= 0 || out_h <= 0) return 1;

    size_t fsize = 0;
    unsigned char* file = read_file(in_path, &fsize);
    if (!file) return 1;

    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load_from_memory(file, (int)fsize, &w, &h, &comp, 4);
    free(file);
    if (!data || w <= 0 || h <= 0) {
        if (data) stbi_image_free(data);
        return 1;
    }

    size_t out_size = (size_t)out_w * (size_t)out_h * 4;
    unsigned char* out = (unsigned char*)malloc(out_size);
    if (!out) {
        stbi_image_free(data);
        return 1;
    }

    for (int y = 0; y < out_h; y++) {
        int sy = (int)((long long)y * h / out_h);
        for (int x = 0; x < out_w; x++) {
            int sx = (int)((long long)x * w / out_w);
            unsigned char* src = data + (sy * w + sx) * 4;
            unsigned char* dst = out + (y * out_w + x) * 4;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        }
    }

    int ok = stbi_write_png(out_path, out_w, out_h, 4, out, out_w * 4);
    free(out);
    stbi_image_free(data);
    return ok ? 0 : 1;
}
