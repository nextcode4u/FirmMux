/* stb_image_write - v1.16 - public domain - http://nothings.org/stb */
#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

extern int stbi_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes);

#ifdef __cplusplus
}
#endif

#ifdef STB_IMAGE_WRITE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef STBIW_MALLOC
#define STBIW_MALLOC(sz)    malloc(sz)
#define STBIW_REALLOC(p,sz) realloc(p,sz)
#define STBIW_FREE(p)       free(p)
#endif

typedef unsigned char stbi_uc;

typedef struct
{
   stbi_uc *data;
   int size;
   int capacity;
} stbi__write_context;

static void stbi__writecontext_init(stbi__write_context *c)
{
   c->data = NULL;
   c->size = 0;
   c->capacity = 0;
}

static void stbi__writecontext_free(stbi__write_context *c)
{
   if (c->data) STBIW_FREE(c->data);
   c->data = NULL;
   c->size = 0;
   c->capacity = 0;
}

static int stbi__writecontext_grow(stbi__write_context *c, int needed)
{
   int new_cap = c->capacity ? c->capacity * 2 : 1024;
   if (new_cap < needed) new_cap = needed;
   stbi_uc *n = (stbi_uc*)STBIW_REALLOC(c->data, new_cap);
   if (!n) return 0;
   c->data = n;
   c->capacity = new_cap;
   return 1;
}

static void stbi__writecontext_putc(stbi__write_context *c, stbi_uc v)
{
   if (c->size + 1 > c->capacity) {
      if (!stbi__writecontext_grow(c, c->size + 1)) return;
   }
   c->data[c->size++] = v;
}

static void stbi__writecontext_write(stbi__write_context *c, const void *data, int len)
{
   if (len <= 0) return;
   if (c->size + len > c->capacity) {
      if (!stbi__writecontext_grow(c, c->size + len)) return;
   }
   memcpy(c->data + c->size, data, len);
   c->size += len;
}

static unsigned int stbi__crc32_table[256];
static int stbi__crc32_table_inited = 0;

static void stbi__crc32_init(void)
{
   unsigned int i,j,c;
   for (i=0; i < 256; i++) {
      c = (unsigned int) i;
      for (j=0; j < 8; j++)
         c = c & 1 ? 0xedb88320U ^ (c >> 1) : c >> 1;
      stbi__crc32_table[i] = c;
   }
   stbi__crc32_table_inited = 1;
}

static unsigned int stbi__crc32(unsigned int crc, unsigned char *buffer, int len)
{
   unsigned int c = crc ^ 0xffffffffU;
   int i;
   if (!stbi__crc32_table_inited) stbi__crc32_init();
   for (i = 0; i < len; i++)
      c = stbi__crc32_table[(c ^ buffer[i]) & 0xff] ^ (c >> 8);
   return c ^ 0xffffffffU;
}

static void stbi__write_uint32(stbi__write_context *s, unsigned int v)
{
   stbi__writecontext_putc(s, (stbi_uc)((v >> 24) & 0xff));
   stbi__writecontext_putc(s, (stbi_uc)((v >> 16) & 0xff));
   stbi__writecontext_putc(s, (stbi_uc)((v >>  8) & 0xff));
   stbi__writecontext_putc(s, (stbi_uc)((v      ) & 0xff));
}

static void stbi__write_chunk(stbi__write_context *s, const char *tag, unsigned char *data, int length)
{
   unsigned int crc;
   stbi__write_uint32(s, length);
   stbi__writecontext_write(s, tag, 4);
   if (length) stbi__writecontext_write(s, data, length);
   crc = stbi__crc32(0, (unsigned char*)tag, 4);
   if (length) crc = stbi__crc32(crc, data, length);
   stbi__write_uint32(s, crc);
}

static unsigned int stbi__adler32(unsigned char *data, int len)
{
   const unsigned int MOD_ADLER = 65521;
   unsigned int a = 1, b = 0;
   int i;
   for (i=0; i < len; i++) {
      a = (a + data[i]) % MOD_ADLER;
      b = (b + a) % MOD_ADLER;
   }
   return (b << 16) | a;
}

static void stbi__zlib_compress(stbi__write_context *s, unsigned char *data, int len)
{
   int block = 0;
   stbi__writecontext_putc(s, 0x78);
   stbi__writecontext_putc(s, 0x01);
   while (block < len) {
      int remaining = len - block;
      int size = remaining > 65535 ? 65535 : remaining;
      int final = (block + size) == len;
      stbi__writecontext_putc(s, (stbi_uc)(final ? 1 : 0));
      stbi__writecontext_putc(s, (stbi_uc)(size & 0xff));
      stbi__writecontext_putc(s, (stbi_uc)((size >> 8) & 0xff));
      stbi__writecontext_putc(s, (stbi_uc)(~size & 0xff));
      stbi__writecontext_putc(s, (stbi_uc)(((~size) >> 8) & 0xff));
      stbi__writecontext_write(s, data + block, size);
      block += size;
   }
   stbi__write_uint32(s, stbi__adler32(data, len));
}

int stbi_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes)
{
   int i, y;
   FILE *f;
   unsigned char sig[8] = { 137,80,78,71,13,10,26,10 };
   unsigned char *png_data;
   stbi__write_context s;
   int row_bytes = w * comp;
   if (stride_in_bytes == 0) stride_in_bytes = row_bytes;

   stbi__writecontext_init(&s);
   stbi__writecontext_write(&s, sig, 8);

   {
      unsigned char ihdr[13];
      ihdr[0] = (stbi_uc)((w>>24) & 0xff);
      ihdr[1] = (stbi_uc)((w>>16) & 0xff);
      ihdr[2] = (stbi_uc)((w>> 8) & 0xff);
      ihdr[3] = (stbi_uc)( w      & 0xff);
      ihdr[4] = (stbi_uc)((h>>24) & 0xff);
      ihdr[5] = (stbi_uc)((h>>16) & 0xff);
      ihdr[6] = (stbi_uc)((h>> 8) & 0xff);
      ihdr[7] = (stbi_uc)( h      & 0xff);
      ihdr[8] = 8;
      ihdr[9] = (comp == 3) ? 2 : 6;
      ihdr[10] = 0;
      ihdr[11] = 0;
      ihdr[12] = 0;
      stbi__write_chunk(&s, "IHDR", ihdr, 13);
   }

   png_data = (unsigned char*)STBIW_MALLOC((size_t)(h * (row_bytes + 1)));
   if (!png_data) {
      stbi__writecontext_free(&s);
      return 0;
   }
   for (y = 0; y < h; y++) {
      unsigned char *dst = png_data + y * (row_bytes + 1);
      const unsigned char *src = (const unsigned char*)data + y * stride_in_bytes;
      dst[0] = 0;
      memcpy(dst + 1, src, row_bytes);
   }

   {
      stbi__write_context z;
      stbi__writecontext_init(&z);
      stbi__zlib_compress(&z, png_data, h * (row_bytes + 1));
      stbi__write_chunk(&s, "IDAT", z.data, z.size);
      stbi__writecontext_free(&z);
   }

   stbi__write_chunk(&s, "IEND", NULL, 0);

   f = fopen(filename, "wb");
   if (!f) {
      STBIW_FREE(png_data);
      stbi__writecontext_free(&s);
      return 0;
   }
   fwrite(s.data, 1, s.size, f);
   fclose(f);

   STBIW_FREE(png_data);
   stbi__writecontext_free(&s);
   return 1;
}

#endif

#endif
