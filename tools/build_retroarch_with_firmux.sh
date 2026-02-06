#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ZIP="$ROOT/Refrences/RetroArch-master.zip"
BUILD_DIR="/tmp/retroarch_build_firmux"
SRC_DIR="$BUILD_DIR/RetroArch-master"
OUT_DIR="$ROOT/SD/3ds/FirmMux/emulators"
RULES1="$ROOT/SD/3ds/emulators/retroarch_rules.json"
RULES2="$ROOT/SD/3ds/Emulators/retroarch_rules.json"
ENTRY="sd:/3ds/FirmMux/emulators/retroarch.3dsx"
export PATH="/opt/devkitpro/tools/bin:$PATH"
export DEVKITTOOLS="/opt/devkitpro/tools"
if [ ! -f "$ZIP" ]; then
  echo "Missing zip: $ZIP" >&2
  exit 1
fi
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR" "$OUT_DIR"
unzip -q "$ZIP" -d "$BUILD_DIR"
python3 - << 'PY'
from pathlib import Path
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/Makefile.ctr")
text = p.read_text()
if "HAVE_DYNAMIC" not in text:
    text = text.replace("HAVE_STATIC_DUMMY      ?= 0\n", "HAVE_STATIC_DUMMY      ?= 0\nHAVE_DYNAMIC           ?= 1\n")
if "-DHAVE_DYNAMIC" not in text:
    text = text.replace("DEFINES :=", "DEFINES := -DHAVE_DYNAMIC")
p.write_text(text)
PY

python3 - << 'PY'
from pathlib import Path
import re
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/frontend/frontend_salamander.c")
text = p.read_text()
if "firmux_boot_hook" in text:
    p.write_text(text)
    raise SystemExit(0)
insert_headers = "#include <ctype.h>\\n#include <stdio.h>\\n#include <string.h>\\n"
if insert_headers not in text:
    m = re.search(r"(#include \"../file_path_special\\.h\"\\n)", text)
    if m:
        text = text[:m.end()] + insert_headers + text[m.end():]

hook = r'''
typedef struct {
   const char *label;
   const char *cands[6];
   int count;
} firmux_core_map_t;

static const firmux_core_map_t g_firmux_core_map[] = {
   { "Stella 2014",     { "stella2014", "stella" }, 2 },
   { "Atari800",        { "atari800" }, 1 },
   { "ProSystem",       { "prosystem" }, 1 },
   { "blueMSX",         { "bluemsx", "blue_msx", "msx" }, 3 },
   { "Caprice32",       { "cap32", "caprice32" }, 2 },
   { "Gambatte",        { "gambatte" }, 1 },
   { "Genesis Plus GX", { "genesis_plus_gx", "genesisplusgx", "genesis" }, 3 },
   { "FreeIntv",        { "freeintv" }, 1 },
   { "O2EM",            { "o2em" }, 1 },
   { "Nestopia UE",     { "nestopia_ue", "nestopia" }, 2 },
   { "Beetle NeoPop",   { "beetle_ngp", "mednafen_ngp", "neopop", "ngp" }, 4 },
   { "PokeMini",        { "pokemini" }, 1 },
   { "Snes9x 2002",     { "snes9x2002", "snes9x_2002" }, 2 },
   { "Beetle PCE Fast", { "beetle_pce_fast", "mednafen_pce_fast", "pce_fast", "pcefast" }, 4 },
   { "Beetle Cygne",    { "beetle_cygne", "mednafen_wswan", "wswan", "wonderswan", "cygne" }, 5 }
};

static int firmux_core_map_count(void)
{
   return (int)(sizeof(g_firmux_core_map) / sizeof(g_firmux_core_map[0]));
}

static bool g_firmux_has_launch = false;
static bool g_firmux_log_inited = false;
bool firmux_has_content = false;
char firmux_content_path[512];
static char g_firmux_rom_sdmc[512];
static char g_firmux_core_path[512];

typedef struct {
   char core_override[128];
   int cpu_profile;
   int frameskip;
   int vsync;
   int audio_latency;
   int threaded_video;
   int hard_gpu_sync;
   int integer_scale;
   int aspect_ratio;
   float aspect_ratio_value;
   int bilinear;
   char shader[256];
   int runahead;
   int rewind;
} firmux_rom_opts_t;

static void firmux_log_line(const char *fmt, ...)
{
   const char *path = "sdmc:/3ds/emulators/retroarch_boot.log";
   FILE *f = fopen(path, g_firmux_log_inited ? "a" : "w");
   if (!f)
      return;
   g_firmux_log_inited = true;
   va_list ap;
   va_start(ap, fmt);
   vfprintf(f, fmt, ap);
   va_end(ap);
   fprintf(f, "\\n");
   fclose(f);
}

static bool firmux_file_exists(const char *path)
{
   FILE *f = fopen(path, "rb");
   if (!f)
      return false;
   fclose(f);
   return true;
}

static bool firmux_json_get(const char *text, const char *key, char *out, size_t out_size)
{
   char needle[64];
   const char *p, *q;
   size_t n;
   if (!text || !key || !out || out_size == 0)
      return false;
   needle[0] = '"';
   strncpy(needle + 1, key, sizeof(needle) - 3);
   needle[sizeof(needle) - 2] = '"';
   needle[sizeof(needle) - 1] = 0;
   p = strstr(text, needle);
   if (!p)
      return false;
   p = strchr(p, ':');
   if (!p)
      return false;
   p++;
   while (*p && isspace((unsigned char)*p))
      p++;
   if (*p != '"')
      return false;
   p++;
   q = strchr(p, '"');
   if (!q)
      return false;
   n = (size_t)(q - p);
   if (n >= out_size)
      n = out_size - 1;
   memcpy(out, p, n);
   out[n] = 0;
   return true;
}

static void firmux_sanitize(const char *in, char *out, size_t out_size)
{
   size_t w = 0;
   if (!out || out_size == 0)
      return;
   out[0] = 0;
   if (!in)
      return;
   for (size_t i = 0; in[i] && w + 1 < out_size; i++)
   {
      unsigned char c = (unsigned char)in[i];
      if (isalnum(c) || c == '_')
         out[w++] = (char)tolower(c);
   }
   out[w] = 0;
}

static void firmux_sd_to_sdmc(const char *in, char *out, size_t out_size)
{
   if (!out || out_size == 0)
      return;
   out[0] = 0;
   if (!in)
      return;
   if (strncmp(in, "sdmc:/", 6) == 0)
   {
      snprintf(out, out_size, "%s", in);
      return;
   }
   if (strncmp(in, "sd:/", 4) == 0)
   {
      snprintf(out, out_size, "sdmc:/%s", in + 4);
      return;
   }
   if (in[0] == '/')
   {
      snprintf(out, out_size, "sdmc:%s", in);
      return;
   }
   snprintf(out, out_size, "%s", in);
}

static bool firmux_core_path_from_label(const char *label, char *out, size_t out_size)
{
   int i, j;
   if (!label || !label[0] || !out || out_size == 0)
      return false;
   for (i = 0; i < firmux_core_map_count(); i++)
   {
      const firmux_core_map_t *m = &g_firmux_core_map[i];
      if (strcasecmp(m->label, label) != 0)
         continue;
      for (j = 0; j < m->count; j++)
      {
         char cand[128];
         firmux_sanitize(m->cands[j], cand, sizeof(cand));
         if (!cand[0])
            continue;
         snprintf(out, out_size, "sdmc:/retroarch/cores/%s_libretro.3dsx", cand);
         if (firmux_file_exists(out))
            return true;
         snprintf(out, out_size, "sdmc:/retroarch/cores/%s.3dsx", cand);
         if (firmux_file_exists(out))
            return true;
      }
      break;
   }
   {
      char fallback[128];
      firmux_sanitize(label, fallback, sizeof(fallback));
      if (fallback[0])
      {
         snprintf(out, out_size, "sdmc:/retroarch/cores/%s_libretro.3dsx", fallback);
         if (firmux_file_exists(out))
            return true;
         snprintf(out, out_size, "sdmc:/retroarch/cores/%s.3dsx", fallback);
         if (firmux_file_exists(out))
            return true;
      }
   }
   return false;
}

static void firmux_boot_hook(void)
{
   char *buf = NULL;
   size_t size = 0;
   FILE *f = fopen("sdmc:/3ds/emulators/launch.json", "rb");
   firmux_log_line("firmux: boot hook start");
   if (!f)
   {
      firmux_log_line("firmux: missing launch.json");
      return;
   }
   fseek(f, 0, SEEK_END);
   size = (size_t)ftell(f);
   fseek(f, 0, SEEK_SET);
   if (size == 0 || size > 65535)
   {
      fclose(f);
      firmux_log_line("firmux: launch.json invalid size");
      return;
   }
   buf = (char*)malloc(size + 1);
   if (!buf)
   {
      fclose(f);
      return;
   }
   if (fread(buf, 1, size, f) != size)
   {
      fclose(f);
      free(buf);
      firmux_log_line("firmux: launch.json read failed");
      return;
   }
   fclose(f);
   buf[size] = 0;
   char rom[512] = {0};
   char core[128] = {0};
   if (!firmux_json_get(buf, "rom", rom, sizeof(rom)) ||
       !firmux_json_get(buf, "core", core, sizeof(core)))
   {
      free(buf);
      firmux_log_line("firmux: launch.json missing keys");
      return;
   }
   firmux_sd_to_sdmc(rom, g_firmux_rom_sdmc, sizeof(g_firmux_rom_sdmc));
   if (!firmux_core_path_from_label(core, g_firmux_core_path, sizeof(g_firmux_core_path)))
   {
      free(buf);
      firmux_log_line("firmux: core not found");
      return;
   }
   snprintf(firmux_content_path, sizeof(firmux_content_path), "%s", g_firmux_rom_sdmc);
   firmux_has_content = true;
   g_firmux_has_launch = true;
   firmux_log_line("firmux: rom=%s core=%s", g_firmux_rom_sdmc, g_firmux_core_path);
   free(buf);
}
'''

text = text.replace("struct defaults g_defaults;\n", "struct defaults g_defaults;\n\n" + hook + "\n")
text = text.replace("   salamander_init(libretro_path, sizeof(libretro_path));\n",
                    "   firmux_boot_hook();\n   salamander_init(libretro_path, sizeof(libretro_path));\n   if (g_firmux_has_launch && g_firmux_core_path[0]) strlcpy(libretro_path, g_firmux_core_path, sizeof(libretro_path));\n")
p.write_text(text)
PY
python3 - << 'PY'
from pathlib import Path
import re
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/retroarch.c")
text = p.read_text()
if "firmux_boot_hook" in text:
    p.write_text(text)
    raise SystemExit(0)
insert_headers = "#include <ctype.h>\n#include <stdio.h>\n#include <string.h>\n"
if insert_headers not in text:
    m = re.search(r"(#include \"retroarch\.h\"\n)", text)
    if m:
        text = text[:m.end()] + insert_headers + text[m.end():]

hook = r'''
#ifndef HAVE_DYNAMIC
#define HAVE_DYNAMIC 1
#endif
typedef struct {
   const char *label;
   const char *cands[6];
   int count;
} firmux_core_map_t;

static const firmux_core_map_t g_firmux_core_map[] = {
   { "Stella 2014",     { "stella2014", "stella" }, 2 },
   { "Atari800",        { "atari800" }, 1 },
   { "ProSystem",       { "prosystem" }, 1 },
   { "blueMSX",         { "bluemsx", "blue_msx", "msx" }, 3 },
   { "Caprice32",       { "cap32", "caprice32" }, 2 },
   { "Gambatte",        { "gambatte" }, 1 },
   { "Genesis Plus GX", { "genesis_plus_gx", "genesisplusgx", "genesis" }, 3 },
   { "FreeIntv",        { "freeintv" }, 1 },
   { "O2EM",            { "o2em" }, 1 },
   { "Nestopia UE",     { "nestopia_ue", "nestopia" }, 2 },
   { "Beetle NeoPop",   { "beetle_ngp", "mednafen_ngp", "neopop", "ngp" }, 4 },
   { "PokeMini",        { "pokemini" }, 1 },
   { "Snes9x 2002",     { "snes9x2002", "snes9x_2002" }, 2 },
   { "Beetle PCE Fast", { "beetle_pce_fast", "mednafen_pce_fast", "pce_fast", "pcefast" }, 4 },
   { "Beetle Cygne",    { "beetle_cygne", "mednafen_wswan", "wswan", "wonderswan", "cygne" }, 5 }
};

static int firmux_core_map_count(void)
{
   return (int)(sizeof(g_firmux_core_map) / sizeof(g_firmux_core_map[0]));
}

static bool g_firmux_has_launch = false;
static bool g_firmux_log_inited = false;
static char g_firmux_rom_sdmc[512];
static char g_firmux_core_path[512];

void firmux_log_line(const char *fmt, ...)
{
   const char *path = "sdmc:/3ds/emulators/retroarch_boot.log";
   FILE *f = fopen(path, g_firmux_log_inited ? "a" : "w");
   if (!f)
      return;
   g_firmux_log_inited = true;
   va_list ap;
   va_start(ap, fmt);
   vfprintf(f, fmt, ap);
   va_end(ap);
   fprintf(f, "\\n");
   fclose(f);
}

static bool firmux_file_exists(const char *path)
{
   FILE *f = fopen(path, "rb");
   if (!f)
      return false;
   fclose(f);
   return true;
}

static bool firmux_json_get(const char *text, const char *key, char *out, size_t out_size)
{
   char needle[64];
   const char *p, *q;
   size_t n;
   if (!text || !key || !out || out_size == 0)
      return false;
   needle[0] = '"';
   strncpy(needle + 1, key, sizeof(needle) - 3);
   needle[sizeof(needle) - 2] = '"';
   needle[sizeof(needle) - 1] = 0;
   p = strstr(text, needle);
   if (!p)
      return false;
   p = strchr(p, ':');
   if (!p)
      return false;
   p++;
   while (*p && isspace((unsigned char)*p))
      p++;
   if (*p != '"')
      return false;
   p++;
   q = strchr(p, '"');
   if (!q)
      return false;
   n = (size_t)(q - p);
   if (n >= out_size)
      n = out_size - 1;
   memcpy(out, p, n);
   out[n] = 0;
   return true;
}

static void firmux_sanitize(const char *in, char *out, size_t out_size)
{
   size_t w = 0;
   if (!out || out_size == 0)
      return;
   out[0] = 0;
   if (!in)
      return;
   for (size_t i = 0; in[i] && w + 1 < out_size; i++)
   {
      unsigned char c = (unsigned char)in[i];
      if (isalnum(c) || c == '_')
         out[w++] = (char)tolower(c);
   }
   out[w] = 0;
}

static void firmux_sd_to_sdmc(const char *in, char *out, size_t out_size)
{
   if (!out || out_size == 0)
      return;
   out[0] = 0;
   if (!in)
      return;
   if (strncmp(in, "sdmc:/", 6) == 0)
   {
      snprintf(out, out_size, "%s", in);
      return;
   }
   if (strncmp(in, "sd:/", 4) == 0)
   {
      snprintf(out, out_size, "sdmc:/%s", in + 4);
      return;
   }
   if (in[0] == '/')
   {
      snprintf(out, out_size, "sdmc:%s", in);
      return;
   }
   snprintf(out, out_size, "%s", in);
}

static bool firmux_core_path_from_label(const char *label, char *out, size_t out_size)
{
   int i, j;
   if (!label || !label[0] || !out || out_size == 0)
      return false;
   for (i = 0; i < firmux_core_map_count(); i++)
   {
      const firmux_core_map_t *m = &g_firmux_core_map[i];
      if (strcasecmp(m->label, label) != 0)
         continue;
      for (j = 0; j < m->count; j++)
      {
         char cand[128];
         firmux_sanitize(m->cands[j], cand, sizeof(cand));
         if (!cand[0])
            continue;
         snprintf(out, out_size, "sdmc:/retroarch/cores/%s_libretro.3dsx", cand);
         if (firmux_file_exists(out))
            return true;
         snprintf(out, out_size, "sdmc:/retroarch/cores/%s.3dsx", cand);
         if (firmux_file_exists(out))
            return true;
      }
      break;
   }
   {
      char fallback[128];
      firmux_sanitize(label, fallback, sizeof(fallback));
      if (fallback[0])
      {
         snprintf(out, out_size, "sdmc:/retroarch/cores/%s_libretro.3dsx", fallback);
         return true;
      }
   }
   return false;
}

static void firmux_environment_get(int *argc, char *argv[], void *args, void *params_data)
{
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
   if (!wrap_args)
      return;
   wrap_args->flags        |= RARCH_MAIN_WRAP_FLAG_TOUCHED;
   wrap_args->flags        &= ~RARCH_MAIN_WRAP_FLAG_NO_CONTENT;
   wrap_args->content_path  = g_firmux_rom_sdmc[0] ? g_firmux_rom_sdmc : NULL;
   wrap_args->libretro_path = g_firmux_core_path[0] ? g_firmux_core_path : NULL;
   firmux_log_line("firmux: environ_get content=%s core=%s",
      wrap_args->content_path ? wrap_args->content_path : "(null)",
      wrap_args->libretro_path ? wrap_args->libretro_path : "(null)");
}

static bool firmux_boot_hook(int *argc, char ***argv)
{
   g_firmux_log_inited = false;
   const char *paths[] = {
      "sdmc:/3ds/emulators/launch.json",
      "sdmc:/firmmux/launch.json",
      "sdmc:/3ds/FirmMux/launch.json",
      "sd:/3ds/emulators/launch.json",
      "sd:/firmmux/launch.json",
      "sd:/3ds/FirmMux/launch.json"
   };
   int pi;
   char rom[512];
   char rom_sdmc[512];
   char core[128];
   char core_path[512];
   firmux_log_line("firmux: boot hook start");
   for (pi = 0; pi < (int)(sizeof(paths)/sizeof(paths[0])); pi++)
   {
      FILE *f = fopen(paths[pi], "rb");
      if (!f)
         continue;
      firmux_log_line("firmux: read %s", paths[pi]);
      fseek(f, 0, SEEK_END);
      long sz = ftell(f);
      fseek(f, 0, SEEK_SET);
      if (sz <= 0 || sz > 65536)
      {
         fclose(f);
         continue;
      }
      char *buf = (char*)malloc((size_t)sz + 1);
      if (!buf)
      {
         fclose(f);
         continue;
      }
      size_t r = fread(buf, 1, (size_t)sz, f);
      fclose(f);
      buf[r] = 0;
      rom[0] = 0;
      core[0] = 0;
      if (!firmux_json_get(buf, "rom", rom, sizeof(rom)) || !firmux_json_get(buf, "core", core, sizeof(core)))
      {
         firmux_log_line("firmux: parse failed");
         free(buf);
         continue;
      }
      free(buf);
      if (!rom[0] || !core[0])
         continue;
      firmux_log_line("firmux: rom=%s core=%s", rom, core);
      firmux_sd_to_sdmc(rom, rom_sdmc, sizeof(rom_sdmc));
      if (!rom_sdmc[0])
         continue;
      if (!firmux_core_path_from_label(core, core_path, sizeof(core_path)))
      {
         firmux_log_line("firmux: core map failed");
         continue;
      }
      if (!firmux_file_exists(core_path))
      {
         firmux_log_line("firmux: core missing");
         continue;
      }
      firmux_log_line("firmux: core_path=%s", core_path);
      g_firmux_has_launch = true;
      snprintf(g_firmux_rom_sdmc, sizeof(g_firmux_rom_sdmc), "%s", rom_sdmc);
      snprintf(g_firmux_core_path, sizeof(g_firmux_core_path), "%s", core_path);
      firmux_log_line("firmux: launch set");
      return true;
   }
   firmux_log_line("firmux: no launch.json");
   return false;
}
'''

if hook not in text:
    if insert_headers in text:
        text = text.replace(insert_headers, insert_headers + hook + "\n")
    else:
        m = re.search(r"(#include \"retroarch\.h\"\n)", text)
        if m:
            text = text[:m.end()] + hook + "\n" + text[m.end():]

if "firmux_boot_hook(&argc, &argv);" not in text:
    text = text.replace(
        "      content_ctx_info_t info;\n",
        "      bool firmux_has = firmux_boot_hook(&argc, &argv);\n      content_ctx_info_t info;\n"
    )
    text = text.replace(
        "      info.environ_get     = frontend_state_get_ptr()->current_frontend_ctx->environment_get;\n",
        "      info.environ_get     = firmux_has ? firmux_environment_get : frontend_state_get_ptr()->current_frontend_ctx->environment_get;\n"
    )

p.write_text(text)
PY

python3 - << 'PY'
from pathlib import Path
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/tasks/task_content.c")
text = p.read_text()
if "#ifndef HAVE_DYNAMIC" not in text:
    text = text.replace("#include <time.h>", "#include <time.h>\n#ifndef HAVE_DYNAMIC\n#define HAVE_DYNAMIC 1\n#endif")
if "firmux_log_line" not in text:
    text = text.replace("#include <time.h>", "#include <time.h>\nvoid firmux_log_line(const char *fmt, ...);")
if "firmux: retroarch_main_init=" not in text:
    needle = "ret             = retroarch_main_init(wrap_args->argc, wrap_args->argv);"
    if needle in text:
        text = text.replace(needle, needle + "\n   firmux_log_line(\"firmux: retroarch_main_init=%d\", ret ? 1 : 0);")
if "firmux: argv" not in text:
    needle = "   if (args->flags & RARCH_MAIN_WRAP_FLAG_VERBOSE)\n      argv[(*argc)++] = strldup(\"-v\", sizeof(\"-v\"));\n"
    if needle in text:
        insert = needle + "\n   firmux_log_line(\"firmux: argv argc=%d\", *argc);\n   for (int i = 0; i < *argc; i++) firmux_log_line(\"firmux: argv[%d]=%s\", i, argv[i] ? argv[i] : \"(null)\");\n"
        text = text.replace(needle, insert)
block = """#ifdef HAVE_DYNAMIC\n   if (args->libretro_path)\n   {\n      argv[(*argc)++] = strldup(\"-L\", sizeof(\"-L\"));\n      argv[(*argc)++] = strdup(args->libretro_path);\n   }\n#endif\n"""
if block in text:
    text = text.replace(block, "   if (args->libretro_path)\n   {\n      argv[(*argc)++] = strldup(\"-L\", sizeof(\"-L\"));\n      argv[(*argc)++] = strdup(args->libretro_path);\n   }\n")
p.write_text(text)
PY

python3 - << 'PY'
from pathlib import Path
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/retroarch.c")
text = p.read_text()
needle = "error:\n"
if needle in text and "firmux: retroarch_main_init error" not in text:
    insert = "error:\n   firmux_log_line(\"firmux: retroarch_main_init error: %s\", global_get_ptr()->error_string);\n"
    text = text.replace(needle, insert, 1)
p.write_text(text)
PY

python3 - << 'PY'
from pathlib import Path
import re
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/frontend/drivers/platform_ctr.c")
text = p.read_text()
text = text.replace("#ifdef HAVE_NETWORKING\n", "#if 0\n")
p.write_text(text)
PY

python3 - << 'PY'
from pathlib import Path
import re
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/frontend/drivers/platform_ctr.c")
text = p.read_text()
if "FIRMUX_DISABLE_NETPLAY_FORK" not in text:
    text = "#define FIRMUX_DISABLE_NETPLAY_FORK\n" + text

def wrap_netplay_exec(src):
    pat = r"(static\s+bool\s+frontend_ctr_exec\s*\(.*?\n\{)(.*?)(^\})"
    m = re.search(pat, src, flags=re.S | re.M)
    if not m:
        return src
    head, body, tail = m.group(1), m.group(2), m.group(3)
    if "NETPLAY_FORK_MAX_ARGS" not in body:
        return src
    guard = "#ifndef FIRMUX_DISABLE_NETPLAY_FORK\n" + body + "#endif\n"
    return src[:m.start(2)] + guard + src[m.end(2):]

text = wrap_netplay_exec(text)
p.write_text(text)
PY

python3 - << 'PY'
from pathlib import Path
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/frontend/drivers/platform_ctr.c")
text = p.read_text()
text = text.replace('static const char* elf_path_cst         = "sdmc:/retroarch/retroarch.3dsx";',
                    'static const char* elf_path_cst         = "sdmc:/3ds/FirmMux/emulators/retroarch.3dsx";')
p.write_text(text)
PY

python3 - << 'PY'
from pathlib import Path
import re
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/frontend/drivers/platform_ctr.c")
text = p.read_text()
if "firmux_has_content" not in text:
    text = text.replace('#include "ctr/exec-3dsx/exec_cia.h"\n',
                        '#include "ctr/exec-3dsx/exec_cia.h"\n#ifdef IS_SALAMANDER\nextern bool firmux_has_content;\nextern char firmux_content_path[512];\n#endif\n')
text = text.replace("char *arg_data[2];", "char *arg_data[3];")
pat = r"(arg_data\[0\]\s*=\s*\(char\*\)elf_path_cst;\s*\n\s*arg_data\[1\]\s*=\s*NULL;\s*)"
m = re.search(pat, text)
if m and "firmux_has_content" not in m.group(1):
    insert = m.group(1) + "\n#ifdef IS_SALAMANDER\n   if (firmux_has_content && firmux_content_path[0])\n   {\n      arg_data[1] = (char*)firmux_content_path;\n      arg_data[2] = NULL;\n   }\n#endif\n"
    text = text[:m.start(1)] + insert + text[m.end(1):]
p.write_text(text)
PY

python3 - << 'PY'
from pathlib import Path
import re
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/frontend/drivers/platform_ctr.c")
text = p.read_text()
text = text.replace("mcuHwcGetBatteryLevel", "MCUHWC_GetBatteryLevel")
lines = []
for line in text.splitlines():
    if "gfxTopRightFramebuffers" in line or "gfxTopLeftFramebuffers" in line or "gfxBottomFramebuffers" in line:
        continue
    lines.append(line)
text = "\n".join(lines) + "\n"
p.write_text(text)
PY
python3 - << 'PY'
from pathlib import Path
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/libretro-common/rthreads/ctr_pthread.h")
text = p.read_text()
if "USE_CTRULIB_2" not in text:
    text = text.replace("#define _CTR_PTHREAD_WRAP_CTR_", "#define _CTR_PTHREAD_WRAP_CTR_\n#define USE_CTRULIB_2 1")
else:
    if "#define USE_CTRULIB_2" not in text:
        text = text.replace("#define _CTR_PTHREAD_WRAP_CTR_", "#define _CTR_PTHREAD_WRAP_CTR_\n#define USE_CTRULIB_2 1")
p.write_text(text)
PY
python3 - << 'PY'
from pathlib import Path
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/gfx/drivers/ctr_gu.h")
text = p.read_text()
if "#define USE_CTRULIB_2" not in text:
    text = text.replace("#define CTR_GU_H", "#define CTR_GU_H\n#define USE_CTRULIB_2 1")
p.write_text(text)
PY
python3 - << 'PY'
from pathlib import Path
import re
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/frontend/drivers/platform_ctr.c")
text = p.read_text()
lines = text.splitlines()
out = []
i = 0
while i < len(lines):
    line = lines[i]
    if line.strip().startswith("#ifdef USE_CTRULIB_2"):
        j = i + 1
        if j < len(lines) and "gfxSetFramebufferInfo" in lines[j]:
            while i < len(lines) and not lines[i].strip().startswith("#endif"):
                i += 1
            if i < len(lines):
                i += 1
            out.append("void gfxSetFramebufferInfo(gfxScreen_t screen, u8 id)")
            out.append("{")
            out.append("   (void)screen;")
            out.append("   (void)id;")
            out.append("}")
            continue
    out.append(line)
    i += 1
text = "\n".join(out) + "\n"
p.write_text(text)
PY
python3 - << 'PY'
from pathlib import Path
import re
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/gfx/drivers/ctr_gfx.c")
text = p.read_text()
if "#include <sys/stat.h>" not in text:
    text = text.replace("#include <math.h>", "#include <math.h>\n#include <sys/stat.h>")
if "sdmc_getmtime(" not in text:
    insert = """
static int sdmc_getmtime(const char* path, time_t* out)
{
   struct stat st;
   if (!path) return -1;
   if (stat(path, &st) != 0) return -1;
   if (out) *out = st.st_mtime;
   return 0;
}

"""
    text = text.replace("#ifdef HAVE_CONFIG_H\n#include \"../../config.h\"\n#endif\n", "#ifdef HAVE_CONFIG_H\n#include \"../../config.h\"\n#endif\n" + insert)

lines = []
for line in text.splitlines():
    if "extern u8* gfxTopLeftFramebuffers" in line:
        continue
    if "extern u8* gfxTopRightFramebuffers" in line:
        continue
    if "extern u8* gfxBottomFramebuffers" in line:
        continue
    lines.append(line)
text = "\n".join(lines)
text = re.sub(r"gfxTopLeftFramebuffers\s*\[\s*ctr->current_buffer_top\s*\]", "gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL)", text)
text = re.sub(r"gfxTopRightFramebuffers\s*\[\s*ctr->current_buffer_top\s*\]", "gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL)", text)
text = re.sub(r"gfxBottomFramebuffers\s*\[\s*ctr->current_buffer_bottom\s*\]", "gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL)", text)
p.write_text(text)
PY
python3 - << 'PY'
from pathlib import Path
import re
p = Path("/tmp/retroarch_build_firmux/RetroArch-master/ctr/ctr_system.c")
text = p.read_text()
if "USE_CTRULIB_2" not in text or "#define USE_CTRULIB_2" not in text:
    insert = "#include <3ds.h>\n#ifndef USE_CTRULIB_2\n#define USE_CTRULIB_2 1\n#endif"
    text = text.replace("#include <3ds.h>", insert)
pat = r"void error_and_quit\\(const char\\* errorStr\\)\\s*\\{.*?\\n\\}"
m = re.search(pat, text, flags=re.S)
if m:
    repl = r'''void error_and_quit(const char* errorStr)
{
   FILE *f = fopen("sdmc:/3ds/emulators/retroarch_boot.log", "a");
   if (f)
   {
      fprintf(f, "retroarch: error_and_quit: %s\\n", errorStr ? errorStr : "");
      fclose(f);
   }
   svcExitProcess();
}'''
    text = text[:m.start()] + repl + text[m.end():]
    p.write_text(text)
else:
    p.write_text(text)
PY
make -C "$SRC_DIR" -f Makefile.ctr.salamander -j"$(nproc)"
cp -f "$SRC_DIR/retroarch_3ds_salamander.3dsx" "$OUT_DIR/retroarch.3dsx"
if [ -f "$SRC_DIR/retroarch_3ds_salamander.smdh" ]; then
  cp -f "$SRC_DIR/retroarch_3ds_salamander.smdh" "$OUT_DIR/retroarch.smdh"
fi
if [ -f "$RULES1" ]; then
  python3 - << PY
from pathlib import Path
p = Path("$RULES1")
s = p.read_text()
s = s.replace('"retroarch_entry": "sd:/3ds/RetroArch/retroarch.3dsx"', '"retroarch_entry": "$ENTRY"')
s = s.replace('"retroarch_entry": "sd:/3ds/firmmux/emulators/retroarch.3dsx"', '"retroarch_entry": "$ENTRY"')
p.write_text(s)
PY
fi
if [ -f "$RULES2" ]; then
  python3 - << PY
from pathlib import Path
p = Path("$RULES2")
s = p.read_text()
s = s.replace('"retroarch_entry": "sd:/3ds/RetroArch/retroarch.3dsx"', '"retroarch_entry": "$ENTRY"')
s = s.replace('"retroarch_entry": "sd:/3ds/firmmux/emulators/retroarch.3dsx"', '"retroarch_entry": "$ENTRY"')
p.write_text(s)
PY
fi
echo "Built with FirmMux hook: $OUT_DIR/retroarch.3dsx"
