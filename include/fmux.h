#ifndef FMUX_H
#define FMUX_H

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stddef.h>

#define CONFIG_PATH "sdmc:/3ds/FirmMux/config.yaml"
#define CONFIG_BAK_PATH "sdmc:/3ds/FirmMux/config.yaml.bak"
#define STATE_PATH "sdmc:/3ds/FirmMux/state.json"
#define STATE_BAK_PATH "sdmc:/3ds/FirmMux/state.json.bak"
#define STATE_PATH_OLD "sdmc:/3ds/FirmMux/state->json"
#define STATE_BAK_PATH_OLD "sdmc:/3ds/FirmMux/state->json.bak"
#define CACHE_DIR "sdmc:/3ds/FirmMux/cache"
#define CACHE_NDS_DIR "sdmc:/3ds/FirmMux/cache/nds"
#define CACHE_3DS_DIR "sdmc:/3ds/FirmMux/cache/3ds"
#define LAUNCH_DIR "sdmc:/3ds/FirmMux/launch"
#define DEBUG_DIR "sdmc:/3ds/FirmMux/logs"
#define DEBUG_LOG_PATH DEBUG_DIR "/debug.log"
#define DEBUG_ICON_PATH DEBUG_DIR "/icon"
#define SYSTEM_BLACKLIST_PATH "sdmc:/3ds/FirmMux/system_blacklist.txt"
#define SYSTEM_ALIAS_PATH "sdmc:/3ds/FirmMux/system_aliases.txt"
#define FMUX_BOOTSTRAP_TITLEID 0x000400000FF40500ULL
#define NDS_CACHE_MAGIC 0x4e445343
#define MAX_3DS_TITLES 512

#define MAX_TARGETS 16
#define MAX_EXTENSIONS 8
#define MAX_ENTRIES 1024
#define MAX_OPTIONS 32

#define TOP_W 400
#define TOP_H 240
#define BOTTOM_W 320
#define BOTTOM_H 240

#define PREVIEW_W 250
#define TARGET_LIST_W 150

#define LIST_ITEM_H 18
#define HELP_BAR_H 16

#define GRID_COLS 3
#define GRID_ITEM_W 90
#define GRID_ITEM_H 36
#define NDS_PRELOAD_BUDGET 0
#define BANNER_TEX_W 32
#define BANNER_TEX_H 32
#define BANNER_PIXELS (BANNER_TEX_W * BANNER_TEX_H * 4)

typedef struct {
    char name[256];
    bool is_dir;
} FileEntry;

typedef struct {
    char id[32];
    char type[32];
    char label[64];
    char root[256];
    char loader_title_id[32];
    char loader_media[16];
    bool show_system_titles;
    bool alpha_buckets;
    char extensions[MAX_EXTENSIONS][16];
    int ext_count;
} Target;

typedef struct {
    int version;
    char default_target[32];
    bool remember_last_position;
    bool help_bar;
    Target targets[MAX_TARGETS];
    int target_count;
} Config;

typedef struct {
    char id[32];
    char path[256];
    int selection;
    int scroll;
    int nds_banner_mode;
    char loader_title_id[32];
    char loader_media[16];
} TargetState;

typedef struct {
    char last_target[32];
    TargetState entries[MAX_TARGETS];
    int count;
} State;

typedef struct {
    char path[256];
    FileEntry entries[MAX_ENTRIES];
    int count;
    bool valid;
} DirCache;

typedef struct {
    DirCache cache;
} TargetRuntime;

typedef enum {
    OPTION_ACTION_NONE = 0,
    OPTION_ACTION_REBUILD_NDS_CACHE,
    OPTION_ACTION_CLEAR_CACHE,
    OPTION_ACTION_RELOAD_CONFIG,
    OPTION_ACTION_TOGGLE_DEBUG,
    OPTION_ACTION_TOGGLE_NDS_BANNERS,
    OPTION_ACTION_SELECT_LAUNCHER,
    OPTION_ACTION_AUTOBOOT_STATUS,
    OPTION_ACTION_ABOUT
} OptionAction;

typedef struct {
    char label[96];
    OptionAction action;
} OptionItem;

typedef struct {
    C3D_Tex tex;
    C2D_Image image;
    Tex3DS_SubTexture subtex;
    bool loaded;
} IconTexture;

typedef struct {
    u64 titleId;
    FS_MediaType media;
    char name[80];
    char bucket;
    u16 icon_raw[48 * 48];
    bool has_icon;
    char product[16];
    bool friendly_name;
    bool blacklisted;
    bool is_system;
    bool ready;
    bool visible;
} TitleInfo3ds;

typedef struct {
    TitleInfo3ds entries[MAX_3DS_TITLES];
    int count;
    bool loading;
} TitleCatalog;

typedef struct {
    char path[256];
    u64 size;
    u64 mtime;
    char title[128];
    IconTexture icon;
    u32 sample_color;
    u8 rgba[BANNER_PIXELS];
    bool ready;
    bool dirty;
    bool in_progress;
    u32 banner_off;
    bool parsed;
    bool has_rgba;
} NdsCacheEntry;

typedef struct {
    NdsCacheEntry entries[MAX_ENTRIES];
    int count;
} NdsCache;

typedef struct {
    u8* data;
    size_t size;
    u32 offset;
} BannerBlob;

BannerBlob load_banner_blob(const char* full_path);
void free_banner_blob(BannerBlob* b);
bool decode_banner_blob(const BannerBlob* b, NdsCacheEntry* e, u8* rgba_out);
void build_nds_entry_blob(const char* full_path, const BannerBlob* blob);

typedef enum {
    SOUND_MOVE = 0,
    SOUND_SELECT,
    SOUND_BACK,
    SOUND_OPEN,
    SOUND_TOGGLE,
    SOUND_ERROR,
    SOUND_MAX
} SoundId;

bool audio_init(void);
void audio_play(int id);

void trim(char* s);
void copy_str(char* dst, size_t dst_size, const char* src);
bool parse_bool(const char* v, bool* out);
void strip_quotes(char* s);
bool parse_value(const char* line, char* out, size_t out_size);
void normalize_path(char* path);
void normalize_path_sd(char* path, size_t path_size);
void normalize_path_to_sd_colon(char* path, size_t path_size);
bool write_file(const char* path, const char* data);
bool read_file(const char* path, u8** out, size_t* out_size);
void ensure_dirs(void);
void clear_dir_recursive(const char* path, bool keep_root);
bool file_exists(const char* path);
void make_sd_path(const char* in, char* out, size_t out_size);
void path_join(const char* base, const char* name, char* out, size_t out_size);
void path_parent(char* path);
void base_name_no_ext(const char* path, char* out, size_t out_size);
const char* bucket_for_index(int index);
bool is_nds_name(const char* name);
bool is_3dsx_name(const char* name);
bool path_has_prefix(const char* path, const char* prefix);
void debug_set_enabled(bool on);
bool debug_log_enabled(void);
void debug_log(const char* fmt, ...);
void debug_dump_rgba(const u8* rgba, size_t size);
void debug_dump_rgba_named(const u8* rgba, size_t size, const char* base);
u32 hash_color(const char* s);
void make_sprite(u8* rgba, u32 color1, u32 color2);
bool write_nextrom_txt(const char* sd_path);
bool write_nextrom_yaml(const char* sd_path);
bool write_launch_txt_for_nds(const char* sd_path);
bool launch_title_id(u64 title_id, FS_MediaType media, char* status_message, size_t status_size);
bool decode_jpeg_rgba(const unsigned char* jpg, size_t jpg_size, unsigned char** out, unsigned* w, unsigned* h);
bool homebrew_load_meta(const char* sd_path, char* title_out, size_t title_size, u16* icon_out, size_t icon_count);
bool homebrew_launch_3dsx(const char* sd_path, char* status_message, size_t status_size);

bool load_or_create_config(Config* cfg);

bool load_state(State* state);
bool save_state(const State* state);
TargetState* get_target_state(State* state, const char* id);

NdsCacheEntry* nds_cache_entry(const char* path);
bool load_nds_icon_direct(const char* full_path, NdsCacheEntry* e);
void build_nds_entry(const char* full_path);
bool icon_from_rgba(IconTexture* icon, const u8* data, int w, int h);
void icon_free(IconTexture* icon);

bool build_dir_cache(const Target* target, TargetState* ts, DirCache* cache);
bool cache_matches(const DirCache* cache, const char* path);

void ensure_titles_loaded(const Config* cfg);

extern NdsCache g_nds_cache;
extern TitleCatalog g_title_catalog;

int title_count_user(void);
int title_count_system(void);
TitleInfo3ds* title_user_at(int idx);
TitleInfo3ds* title_system_at(int idx);

#endif
