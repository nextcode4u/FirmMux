#include "fmux.h"
#include "smdh.h"
#include <string.h>
#include <3ds/services/am.h>
#include <3ds/util/utf.h>
#include <ctype.h>
#include <stdlib.h>

TitleCatalog g_title_catalog;
static char g_sys_blacklist[64][32];
static int g_sys_blacklist_count = 0;
static char g_sys_alias_key[256][32];
static char g_sys_alias_name[256][96];
static int g_sys_alias_count = 0;
static bool g_titles_dirty = false;

static void clear_catalog(void) {
    for (int i = 0; i < g_title_catalog.count; i++) {
        // nothing to free for software RGBA icons
    }
    memset(&g_title_catalog, 0, sizeof(g_title_catalog));
}

void titles_mark_dirty(void) {
    g_titles_dirty = true;
}

static void load_sys_blacklist(void) {
    g_sys_blacklist_count = 0;
    FILE* f = fopen(SYSTEM_BLACKLIST_PATH, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        char* end = p + strlen(p);
        while (end > p && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = 0;
        if (!*p) continue;
        if (*p == '#') continue;
        if (g_sys_blacklist_count >= 64) break;
        strncpy(g_sys_blacklist[g_sys_blacklist_count], p, sizeof(g_sys_blacklist[0]) - 1);
        g_sys_blacklist[g_sys_blacklist_count][sizeof(g_sys_blacklist[0]) - 1] = 0;
        g_sys_blacklist_count++;
    }
    fclose(f);
}

static void load_sys_aliases(void) {
    g_sys_alias_count = 0;
    FILE* f = fopen(SYSTEM_ALIAS_PATH, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        char* end = p + strlen(p);
        while (end > p && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = 0;
        if (!*p) continue;
        if (*p == '#') continue;
        char* eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        char* key = p;
        char* name = eq + 1;
        while (*key && (*key == ' ' || *key == '\t')) key++;
        while (*name && (*name == ' ' || *name == '\t')) name++;
        if (!*key || !*name) continue;
        if (g_sys_alias_count >= 256) break;
        copy_str(g_sys_alias_key[g_sys_alias_count], sizeof(g_sys_alias_key[0]), key);
        copy_str(g_sys_alias_name[g_sys_alias_count], sizeof(g_sys_alias_name[0]), name);
        g_sys_alias_count++;
    }
    fclose(f);
}

static bool alias_match(const char* pattern, const char* value) {
    if (!pattern || !value) return false;
    size_t lp = strlen(pattern);
    size_t lv = strlen(value);
    if (lp != lv) return false;
    for (size_t i = 0; i < lp; i++) {
        char pc = pattern[i];
        if (pc == '?') continue;
        if (tolower((unsigned char)pc) != tolower((unsigned char)value[i])) return false;
    }
    return true;
}

static const char* find_sys_alias(const char* key) {
    if (!key || !key[0]) return NULL;
    for (int i = 0; i < g_sys_alias_count; i++) {
        const char* pat = g_sys_alias_key[i];
        if (!pat[0]) continue;
        if (strchr(pat, '?')) {
            if (alias_match(pat, key)) return g_sys_alias_name[i];
        } else if (!strcasecmp(pat, key)) {
            return g_sys_alias_name[i];
        }
    }
    return NULL;
}

static bool is_blacklisted(const TitleInfo3ds* t) {
    if (!t || g_sys_blacklist_count == 0) return false;
    char tid_hex[32];
    snprintf(tid_hex, sizeof(tid_hex), "%016llX", (unsigned long long)t->titleId);
    for (int i = 0; i < g_sys_blacklist_count; i++) {
        const char* s = g_sys_blacklist[i];
        if (!s || !s[0]) continue;
        if (!strcasecmp(s, tid_hex)) return true;
        if (t->product[0] && !strcasecmp(s, t->product)) return true;
        if (t->name[0] && !strcasecmp(s, t->name)) return true;
    }
    return false;
}

static bool load_smdh(smdh_s* smdh, FS_MediaType media, u64 tid) {
    static const u32 filePath[] = {0, 0, 2, 0x6E6F6369, 0};
    u32 archivePath[] = { (u32)(tid & 0xFFFFFFFF), (u32)(tid >> 32), media, 0 };
    FS_Path apath = { PATH_BINARY, sizeof(archivePath), archivePath };
    FS_Path fpath = { PATH_BINARY, sizeof(filePath), filePath };
    Handle file = 0;
    Result res = FSUSER_OpenFileDirectly(&file, ARCHIVE_SAVEDATA_AND_CONTENT, apath, fpath, FS_OPEN_READ, 0);
    if (R_FAILED(res)) return false;
    u32 bytesRead = 0;
    res = FSFILE_Read(file, &bytesRead, 0, smdh, sizeof(*smdh));
    FSFILE_Close(file);
    return R_SUCCEEDED(res) && bytesRead == sizeof(*smdh);
}

static bool smdh_copy_text(const u16* src, char* out, size_t out_size) {
    if (!src || !out || out_size == 0) return false;
    int units = (int)utf16_to_utf8((u8*)out, src, out_size - 1);
    if (units < 0) units = 0;
    out[units] = 0;
    char* nl = strchr(out, '\n');
    if (nl) *nl = 0;
    nl = strchr(out, '\r');
    if (nl) *nl = 0;
    while (*out == ' ' || *out == '\t') memmove(out, out + 1, strlen(out));
    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\t')) out[--len] = 0;
    return out[0] != 0;
}

static bool smdh_pick_name(const smdh_s* smdh, char* out, size_t out_size) {
    int order[16] = {1,0,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    for (int i = 0; i < 16; i++) {
        int idx = order[i];
        if (smdh_copy_text(smdh->applicationTitles[idx].shortDescription, out, out_size)) return true;
    }
    for (int i = 0; i < 16; i++) {
        int idx = order[i];
        if (smdh_copy_text(smdh->applicationTitles[idx].longDescription, out, out_size)) return true;
    }
    for (int i = 0; i < 16; i++) {
        int idx = order[i];
        if (smdh_copy_text(smdh->applicationTitles[idx].publisher, out, out_size)) return true;
    }
    return false;
}

static void smdh_to_entry(const smdh_s* smdh, TitleInfo3ds* out, bool* name_ok) {
    bool ok = smdh_pick_name(smdh, out->name, sizeof(out->name));
    if (name_ok) *name_ok = ok;
    memcpy(out->icon_raw, smdh->bigIconData, sizeof(out->icon_raw));
    out->has_icon = true;
}

static u16 rgb555_to_rgb565(u16 rgb, bool transparent) {
    if (transparent) return 0;
    u16 r = rgb & 0x1F;
    u16 g = (rgb >> 5) & 0x1F;
    u16 b = (rgb >> 10) & 0x1F;
    u16 g6 = (g << 1) | (g >> 4);
    return (r << 11) | (g6 << 5) | b;
}

static bool decode_twl_banner(const u8* data, u16* icon_out, char* title_out, size_t title_size) {
    if (!data || !icon_out) return false;
    const u8* icon = data + 0x20;
    const u8* palette = data + 0x220;
    const u8* title_en = data + 0x240;
    if (title_out && title_size > 0) {
        size_t len = title_size - 1;
        size_t out = 0;
        for (size_t i = 0; i + 1 < 0x100 && out < len; i += 2) {
            u16 ch = title_en[i] | (title_en[i + 1] << 8);
            if (ch == 0 || ch == '\n' || ch == '\r') break;
            if (ch < 128) title_out[out++] = (char)ch;
        }
        title_out[out] = 0;
    }
    u16 pal[16];
    for (int i = 0; i < 16; i++) pal[i] = palette[i * 2] | (palette[i * 2 + 1] << 8);
    u16 icon32[32 * 32];
    memset(icon32, 0, sizeof(icon32));
    for (int tile = 0; tile < 16; ++tile) {
        for (int pixel = 0; pixel < 32; ++pixel) {
            u8 a = icon[(tile << 5) + pixel];
            int px = ((tile & 3) << 3) + ((pixel << 1) & 7);
            int py = ((tile >> 2) << 3) + (pixel >> 2);
            u8 idx1 = (a & 0xf0) >> 4;
            u8 idx2 = (a & 0x0f);
            int p1 = (py * 32) + (px + 1);
            int p0 = (py * 32) + (px + 0);
            icon32[p0] = rgb555_to_rgb565(pal[idx2], idx2 == 0);
            icon32[p1] = rgb555_to_rgb565(pal[idx1], idx1 == 0);
        }
    }
    for (int y = 0; y < 48; y++) {
        int sy = y * 32 / 48;
        for (int x = 0; x < 48; x++) {
            int sx = x * 32 / 48;
            icon_out[y * 48 + x] = icon32[sy * 32 + sx];
        }
    }
    return true;
}

static char bucket_for_title(const char* name) {
    if (!name || !name[0]) return '#';
    const unsigned char* p = (const unsigned char*)name;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    char c = (char)toupper(*p);
    if (c >= 'A' && c <= 'Z') return c;
    return '#';
}

void ensure_titles_loaded(const Config* cfg) {
    bool show_system = true; // always load system titles for system tab
    static bool cached_show_system = true;
    if (g_title_catalog.loading) return;
    if (!g_titles_dirty && g_title_catalog.count > 0 && cached_show_system == show_system) return;
    cached_show_system = show_system;
    g_titles_dirty = false;
    clear_catalog();
    g_title_catalog.loading = true;
    load_sys_blacklist();
    load_sys_aliases();

    if (R_FAILED(amInit())) {
        g_title_catalog.loading = false;
        return;
    }
    AM_InitializeExternalTitleDatabase(false);

    FS_MediaType medias[3];
    int media_count = 0;
    medias[media_count++] = MEDIATYPE_SD;
    medias[media_count++] = MEDIATYPE_NAND;
    bool inserted = false;
    FS_CardType card_type = CARD_CTR;
    if (R_SUCCEEDED(FSUSER_CardSlotIsInserted(&inserted)) && inserted &&
        R_SUCCEEDED(FSUSER_GetCardType(&card_type)) && card_type == CARD_CTR) {
        medias[media_count++] = MEDIATYPE_GAME_CARD;
    }
    for (int mi = 0; mi < media_count; mi++) {
        u32 count = 0;
        if (R_FAILED(AM_GetTitleCount(medias[mi], &count)) || count == 0) continue;
        u64* list = (u64*)malloc(sizeof(u64) * count);
        if (!list) continue;
        u32 read = 0;
        if (R_FAILED(AM_GetTitleList(&read, medias[mi], count, list))) { free(list); continue; }
        for (u32 i = 0; i < read && g_title_catalog.count < MAX_3DS_TITLES; i++) {
            u64 tid = list[i];
            u32 high = (u32)(tid >> 32);
            if (high == 0x0004000E || high == 0x0004008C) continue;
            if (high != 0x00040000 && high != 0x00040010 && high != 0x00040002 && high != 0x00048004) continue;
            TitleInfo3ds* t = &g_title_catalog.entries[g_title_catalog.count++];
            memset(t, 0, sizeof(*t));
            t->titleId = tid;
            t->media = medias[mi];
            t->is_system = (high == 0x00040010);
            smdh_s smdh;
            bool name_ok = false;
            bool dsiware = ((high & 0x00008000) != 0);
            if (dsiware) {
                u8 banner[0x840];
                char twl_name[128] = {0};
                if (R_SUCCEEDED(FSUSER_GetLegacyBannerData(medias[mi], tid, banner)) &&
                    decode_twl_banner(banner, t->icon_raw, twl_name, sizeof(twl_name))) {
                    t->has_icon = true;
                    t->icon_linear = true;
                    if (twl_name[0]) {
                        copy_str(t->name, sizeof(t->name), twl_name);
                        name_ok = true;
                    }
                } else {
                    t->has_icon = false;
                }
            } else if (load_smdh(&smdh, medias[mi], tid)) {
                smdh_to_entry(&smdh, t, &name_ok);
            } else {
                t->has_icon = false;
            }
            if (!name_ok || t->name[0] == 0) {
                char prod[16] = {0};
                if (R_SUCCEEDED(AM_GetTitleProductCode(medias[mi], tid, prod)) && prod[0]) {
                    copy_str(t->name, sizeof(t->name), prod);
                    copy_str(t->product, sizeof(t->product), prod);
                } else {
                    snprintf(t->name, sizeof(t->name), "%016llX", (unsigned long long)tid);
                }
            } else {
                char prod[16] = {0};
                if (R_SUCCEEDED(AM_GetTitleProductCode(medias[mi], tid, prod)) && prod[0]) {
                    copy_str(t->product, sizeof(t->product), prod);
                }
            }
            t->friendly_name = name_ok;
            char tid_hex[32];
            snprintf(tid_hex, sizeof(tid_hex), "%016llX", (unsigned long long)t->titleId);
            const char* alias = NULL;
            if (t->product[0]) alias = find_sys_alias(t->product);
            if (!alias) alias = find_sys_alias(tid_hex);
            if (t->is_system) {
                bool sys_match = false;
                if (t->product[0] && strncasecmp(t->product, "CTR-N-", 6) == 0) sys_match = true;
                if (alias && alias[0]) sys_match = true;
                if (!sys_match) t->is_system = false;
            }
            if (t->is_system && alias && alias[0]) {
                copy_str(t->name, sizeof(t->name), alias);
                t->friendly_name = true;
            }
            t->blacklisted = is_blacklisted(t);
            t->bucket = bucket_for_title(t->name);
            t->ready = true;
            t->visible = !t->is_system || (t->friendly_name && !t->blacklisted);
        }
        free(list);
    }

    amExit();
    g_title_catalog.loading = false;
}

static TitleInfo3ds* find_by_filter(bool system_only, int idx) {
    int count = 0;
    for (int i = 0; i < g_title_catalog.count; i++) {
        TitleInfo3ds* t = &g_title_catalog.entries[i];
        if (system_only != t->is_system) continue;
        if (!t->visible) continue;
        if (count == idx) return t;
        count++;
    }
    return NULL;
}

static int title_name_cmp(const void* a, const void* b) {
    const TitleInfo3ds* ta = *(TitleInfo3ds* const*)a;
    const TitleInfo3ds* tb = *(TitleInfo3ds* const*)b;
    if (!ta || !tb) return 0;
    const char* an = ta->name;
    const char* bn = tb->name;
    if (!an) an = "";
    if (!bn) bn = "";
    int a_letter = isalpha((unsigned char)an[0]) ? 1 : 0;
    int b_letter = isalpha((unsigned char)bn[0]) ? 1 : 0;
    if (a_letter != b_letter) {
        return a_letter - b_letter;
    }
    return strcasecmp(an, bn);
}

static void sort_titles(TitleInfo3ds** arr, int count, int sort_mode) {
    if (!arr || count <= 1) return;
    qsort(arr, count, sizeof(TitleInfo3ds*), title_name_cmp);
    if (sort_mode == 1) {
        int i = 0;
        int j = count - 1;
        while (i < j) {
            TitleInfo3ds* tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
            i++;
            j--;
        }
    }
}

static void build_sorted_lists(int sort_mode, TitleInfo3ds*** out_user, int* out_user_count, TitleInfo3ds*** out_sys, int* out_sys_count) {
    static TitleInfo3ds* user_list[MAX_3DS_TITLES];
    static TitleInfo3ds* sys_list[MAX_3DS_TITLES];
    int uc = 0;
    int sc = 0;
    for (int i = 0; i < g_title_catalog.count; i++) {
        TitleInfo3ds* t = &g_title_catalog.entries[i];
        if (!t->visible) continue;
        if (t->is_system) {
            sys_list[sc++] = t;
        } else {
            user_list[uc++] = t;
        }
    }
    sort_titles(user_list, uc, sort_mode);
    sort_titles(sys_list, sc, sort_mode);
    *out_user = user_list;
    *out_user_count = uc;
    *out_sys = sys_list;
    *out_sys_count = sc;
}

int title_count_user(void) {
    int c = 0;
    for (int i = 0; i < g_title_catalog.count; i++) {
        TitleInfo3ds* t = &g_title_catalog.entries[i];
        if (!t->is_system && t->visible) c++;
    }
    return c;
}

int title_count_system(void) {
    int c = 0;
    for (int i = 0; i < g_title_catalog.count; i++) {
        TitleInfo3ds* t = &g_title_catalog.entries[i];
        if (t->is_system && t->visible) c++;
    }
    return c;
}

static TitleInfo3ds* title_user_card(void) {
    for (int i = 0; i < g_title_catalog.count; i++) {
        TitleInfo3ds* t = &g_title_catalog.entries[i];
        if (t->is_system || !t->visible) continue;
        if (t->media == MEDIATYPE_GAME_CARD) return t;
    }
    return NULL;
}

TitleInfo3ds* title_user_at(int idx) {
    TitleInfo3ds* card = title_user_card();
    if (card) {
        if (idx == 0) return card;
        idx--;
    }
    int count = 0;
    for (int i = 0; i < g_title_catalog.count; i++) {
        TitleInfo3ds* t = &g_title_catalog.entries[i];
        if (t->is_system || !t->visible) continue;
        if (card && t == card) continue;
        if (count == idx) return t;
        count++;
    }
    return NULL;
}
TitleInfo3ds* title_system_at(int idx) { return find_by_filter(true, idx); }

TitleInfo3ds* title_user_at_sorted(int idx, int sort_mode) {
    TitleInfo3ds* card = title_user_card();
    if (card) {
        if (idx == 0) return card;
        idx--;
    }
    TitleInfo3ds** user_list = NULL;
    TitleInfo3ds** sys_list = NULL;
    int uc = 0;
    int sc = 0;
    build_sorted_lists(sort_mode, &user_list, &uc, &sys_list, &sc);
    if (idx < 0 || idx >= uc) return NULL;
    return user_list[idx];
}

TitleInfo3ds* title_system_at_sorted(int idx, int sort_mode) {
    TitleInfo3ds** user_list = NULL;
    TitleInfo3ds** sys_list = NULL;
    int uc = 0;
    int sc = 0;
    build_sorted_lists(sort_mode, &user_list, &uc, &sys_list, &sc);
    if (idx < 0 || idx >= sc) return NULL;
    return sys_list[idx];
}
