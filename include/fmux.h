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
#define BACKGROUNDS_DIR "sdmc:/3ds/FirmMux/backgrounds"
#define BACKGROUNDS_TOP_DIR BACKGROUNDS_DIR "/top"
#define BACKGROUNDS_BOTTOM_DIR BACKGROUNDS_DIR "/bottom"
#define NDS_OPTIONS_DIR "sdmc:/_nds/firmmux/nds_options"
#define NDS_BOOTSTRAP_PREP_3DSX "sdmc:/3ds/FirmMux/firmux-bootstrap-prep.3dsx"
#define NDS_CHEATS_DIR "sdmc:/_nds/firmmux/nds_cheats"
#define NDS_CHEATS_DB_PATH "sdmc:/_nds/ntr-forwarder/usrcheat.dat"
#define NDS_WIDESCREEN_DIR "sdmc:/_nds/firmmux/nds_widescreen"
#define EMU_EXT_DIR "sdmc:/3ds/emulators"
#define RETRO_RULES_PATH EMU_EXT_DIR "/retroarch_rules.json"
#define RETRO_EMULATORS_PATH EMU_EXT_DIR "/emulators.json"
#define RETRO_LAUNCH_PATH EMU_EXT_DIR "/launch.json"
#define RETRO_LAUNCH_TMP_PATH LAUNCH_DIR "/retroarch_launch.tmp"
#define RETRO_LOG_PATH EMU_EXT_DIR "/log.txt"
#define RETRO_ROM_OPTIONS_PATH EMU_EXT_DIR "/rom_options.json"
#define RETRO_ROM_OPTIONS_TMP_PATH EMU_EXT_DIR "/rom_options.tmp.json"
#define RETRO_FILTER_FAV_PATH EMU_EXT_DIR "/filter_favorites.txt"
#define RETRO_RULES_BAK_PATH DEBUG_DIR "/retroarch_rules.bak.json"
#define RETRO_EMULATORS_BAK_PATH DEBUG_DIR "/emulators.bak.json"
#define RETRO_ENTRY_DEFAULT "sd:/3ds/FirmMux/emulators/retroarch.3dsx"
#define RETRO_CORES_DIR "sdmc:/retroarch/cores"
#define RETRO_FILTERS_DIR "sdmc:/retroarch/filters"
#define RETRO_VIDEO_FILTERS_DIR RETRO_FILTERS_DIR "/video"
#define RETRO_AUDIO_FILTERS_DIR RETRO_FILTERS_DIR "/audio"
#define RETRO_COMPAT_HANDOFF_PATH "sd:/firmmux/launch.json"
#define RETRO_COMPAT_HANDOFF_PATH_3DS "sd:/3ds/firmmux/launch.json"
#define NDS_CACHE_MAGIC 0x4e445343
#define MAX_3DS_TITLES 512

#define MAX_TARGETS 24
#define MAX_EXTENSIONS 8
#define MAX_ENTRIES 1024
#define MAX_OPTIONS 32
#define MAX_THEMES 32
#define MAX_BACKGROUNDS 64
#define MAX_SYSTEMS 20
#define MAX_RETRO_RULES 40

#define TOP_W 400
#define TOP_H 240
#define BOTTOM_W 320
#define BOTTOM_H 240

#define PREVIEW_W 250
#define TARGET_LIST_W 150

#define LIST_ITEM_H 20
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
    char card_launcher_title_id[32];
    char card_launcher_media[16];
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
    char theme[32];
    Target targets[MAX_TARGETS];
    int target_count;
} Config;

typedef struct {
    char id[32];
    char path[256];
    int selection;
    int scroll;
    int nds_banner_mode;
    int sort_mode;
    char loader_title_id[32];
    char loader_media[16];
    char card_launcher_title_id[32];
    char card_launcher_media[16];
} TargetState;

typedef struct {
    char last_target[32];
    char theme[32];
    char top_background[64];
    char bottom_background[64];
    int background_visibility;
    int bgm_enabled;
    bool retro_log_enabled;
    bool retro_chainload_enabled;
    int nds_launcher_mode;
    TargetState entries[MAX_TARGETS];
    int count;
} State;

typedef struct {
    int widescreen;
    int cheats;
    int ap_patch;
    int cpu_boost;
    int vram_boost;
    int async_read;
    int card_read_dma;
    int dsi_mode;
} NdsRomOptions;

typedef struct {
    char path[256];
    FileEntry entries[MAX_ENTRIES];
    int count;
    bool valid;
} DirCache;

typedef struct {
    DirCache cache;
    bool root_missing;
} TargetRuntime;

typedef struct {
    char folder[16];
    char extensions[MAX_EXTENSIONS][16];
    int ext_count;
    char core[64];
} RetroRule;

typedef struct {
    int version;
    char mode[64];
    char retroarch_entry[256];
    char handoff_path[256];
    RetroRule rules[MAX_RETRO_RULES];
    int rule_count;
} RetroRules;

typedef struct {
    char key[16];
    char display_name[48];
    bool enabled;
    char rom_folder[256];
} EmuSystem;

typedef struct {
    int version;
    EmuSystem systems[MAX_SYSTEMS];
    int count;
} EmuConfig;

typedef struct {
    char core_override[64];
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
    char video_filter[192];
    char audio_filter[192];
    int runahead;
    int rewind;
} RetroRomOptions;

typedef enum {
    OPTION_ACTION_NONE = 0,
    OPTION_ACTION_REBUILD_NDS_CACHE,
    OPTION_ACTION_CLEAR_CACHE,
    OPTION_ACTION_RELOAD_CONFIG,
    OPTION_ACTION_TOGGLE_DEBUG,
    OPTION_ACTION_TOGGLE_NDS_BANNERS,
    OPTION_ACTION_SELECT_LAUNCHER,
    OPTION_ACTION_NDS_LAUNCHER_MODE,
    OPTION_ACTION_SELECT_CARD_LAUNCHER,
    OPTION_ACTION_THEME_MENU,
    OPTION_ACTION_TOP_BACKGROUND,
    OPTION_ACTION_BOTTOM_BACKGROUND,
    OPTION_ACTION_BG_VISIBILITY,
    OPTION_ACTION_TOGGLE_BGM,
    OPTION_ACTION_RETRO_LOG_TOGGLE,
    OPTION_ACTION_RETRO_CHAINLOAD_TOGGLE,
    OPTION_ACTION_RETRO_INFO,
    OPTION_ACTION_EMULATORS_MENU,
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
    char name[32];
    int list_item_h;
    int line_spacing;
    int status_h;
    u32 top_bg;
    u32 bottom_bg;
    u32 panel_left;
    u32 panel_right;
    u32 preview_bg;
    u32 text_primary;
    u32 text_secondary;
    u32 text_muted;
    u32 tab_bg;
    u32 tab_sel;
    u32 tab_text;
    u32 list_bg;
    u32 list_sel;
    u32 list_text;
    u32 option_bg;
    u32 option_sel;
    u32 option_text;
    u32 option_header;
    u32 overlay_bg;
    u32 help_bg;
    u32 help_line;
    u32 help_text;
    u32 status_bg;
    u32 status_text;
    u32 status_icon;
    u32 status_dim;
    u32 status_bolt;
    u32 toast_bg;
    u32 toast_text;
    char top_image[64];
    char bottom_image[64];
    char status_strip[64];
    char sprite_icon[64];
    char list_item_image[64];
    char list_sel_image[64];
    char tab_item_image[64];
    char tab_sel_image[64];
    char option_item_image[64];
    char option_sel_image[64];
    char preview_frame[64];
    char help_strip[64];
    int list_item_offset_y;
    int list_text_offset_y;
    int tab_item_offset_y;
    int tab_text_offset_y;
    int option_item_offset_y;
    int option_text_offset_y;
    int help_text_offset_y;
    int status_text_offset_y;
    float list_item_center_y;
    float list_sel_center_y;
    float tab_item_center_y;
    float tab_sel_center_y;
    float option_item_center_y;
    float option_sel_center_y;
    float preview_frame_center_y;
    float help_strip_center_y;
    float status_strip_center_y;
    bool image_swap_rb;
    char image_channel_order[8];
    IconTexture top_tex;
    IconTexture bottom_tex;
    IconTexture status_tex;
    IconTexture sprite_tex;
    IconTexture list_item_tex;
    IconTexture list_sel_tex;
    IconTexture tab_item_tex;
    IconTexture tab_sel_tex;
    IconTexture option_item_tex;
    IconTexture option_sel_tex;
    IconTexture preview_frame_tex;
    IconTexture help_tex;
    int top_w;
    int top_h;
    int bottom_w;
    int bottom_h;
    int status_w;
    int status_h_img;
    int sprite_w;
    int sprite_h;
    int list_item_w;
    int list_item_h_img;
    int list_sel_w;
    int list_sel_h_img;
    int tab_item_w;
    int tab_item_h_img;
    int tab_sel_w;
    int tab_sel_h_img;
    int option_item_w;
    int option_item_h_img;
    int option_sel_w;
    int option_sel_h_img;
    int preview_frame_w;
    int preview_frame_h;
    int help_w;
    int help_h;
    bool top_loaded;
    bool bottom_loaded;
    bool status_loaded;
    bool sprite_loaded;
    bool list_item_loaded;
    bool list_sel_loaded;
    bool tab_item_loaded;
    bool tab_sel_loaded;
    bool option_item_loaded;
    bool option_sel_loaded;
    bool preview_frame_loaded;
    bool help_loaded;
} Theme;

typedef struct {
    u64 titleId;
    FS_MediaType media;
    char name[80];
    char bucket;
    u16 icon_raw[48 * 48];
    bool has_icon;
    bool icon_linear;
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
void audio_set_bgm_enabled(bool enabled);

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
bool load_nds_rom_options(const char* sd_path, NdsRomOptions* opt);
bool save_nds_rom_options(const char* sd_path, const NdsRomOptions* opt);
bool write_nds_bootstrap_ini(const char* sd_path, const NdsRomOptions* opt);
bool copy_file_simple(const char* from, const char* to);
bool copy_file_stream(const char* from, const char* to);
bool find_nds_widescreen_bin(const char* sd_path, char* out, size_t out_size);
bool launch_title_id(u64 title_id, FS_MediaType media, char* status_message, size_t status_size);
bool decode_jpeg_rgba(const unsigned char* jpg, size_t jpg_size, unsigned char** out, unsigned* w, unsigned* h);
bool homebrew_load_meta(const char* sd_path, char* title_out, size_t title_size, u16* icon_out, size_t icon_count);
bool homebrew_launch_3dsx(const char* sd_path, char* status_message, size_t status_size);

bool load_or_create_retro_rules(RetroRules* rules, bool* regenerated);
const char* retro_resolve_core(const RetroRules* rules, const char* system_key, const char* ext_lower, bool* matched_rule);
int retro_extensions_for_system(const RetroRules* rules, const char* system_key, char out_exts[MAX_EXTENSIONS][16]);
bool retro_write_launch(const RetroRules* rules, const char* rom_sd_path, const char* core, char* status_message, size_t status_size);
bool retro_retroarch_exists(const RetroRules* rules);
bool retro_core_available(const char* core_label, bool* known, bool* available);
bool retro_chainload_available(void);
bool retro_chainload(const char* retroarch_sd_path, char* status_message, size_t status_size);
void retro_log_set_enabled(bool on);
bool retro_log_is_enabled(void);
void retro_log_reset(void);
void retro_log_line(const char* fmt, ...);
void retro_rom_options_default(RetroRomOptions* opt);
bool retro_rom_options_load(const char* rom_sd_path, RetroRomOptions* out);
bool retro_rom_options_save(const char* rom_sd_path, const RetroRomOptions* opt);
int retro_shader_favorites_load(char out_list[][192], int max_items);

bool load_or_create_emulators(EmuConfig* cfg, bool* regenerated);
bool save_emulators(const EmuConfig* cfg);
const EmuSystem* emu_find_by_key(const EmuConfig* cfg, const char* key);
EmuSystem* emu_find_by_key_mut(EmuConfig* cfg, const char* key);
const EmuSystem* emu_find_by_path(const EmuConfig* cfg, const char* rom_sd_path);
bool emu_resolve_system(const EmuConfig* cfg, const char* rom_sd_path, const char* fallback_key, char* out_key, size_t out_size);
int emu_known_system_keys(const char** out_keys, int max_keys);

bool load_or_create_config(Config* cfg);
void theme_default(Theme* t);
bool load_theme(Theme* t, const char* name);

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
void sort_dir_cache(DirCache* cache, int sort_mode);

void ensure_titles_loaded(const Config* cfg);
void titles_mark_dirty(void);

extern NdsCache g_nds_cache;
extern TitleCatalog g_title_catalog;

int title_count_user(void);
int title_count_system(void);
TitleInfo3ds* title_user_at(int idx);
TitleInfo3ds* title_system_at(int idx);
TitleInfo3ds* title_user_at_sorted(int idx, int sort_mode);
TitleInfo3ds* title_system_at_sorted(int idx, int sort_mode);

#endif
