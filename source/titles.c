#include "fmux.h"
#include <string.h>

TitleCatalog g_title_catalog;

void ensure_titles_loaded(const Config* cfg) {
    if (g_title_catalog.loading || g_title_catalog.count > 0) return;
    g_title_catalog.loading = true;
    int total = 200;
    g_title_catalog.count = 0;
    for (int i = 0; i < total && i < MAX_3DS_TITLES; i++) {
        TitleInfo3ds* t = &g_title_catalog.entries[g_title_catalog.count++];
        memset(t, 0, sizeof(*t));
        t->titleId = 0;
        t->media = MEDIATYPE_SD;
        char bucket = bucket_for_index(i)[0];
        snprintf(t->name, sizeof(t->name), "%c Title %03d", bucket, i + 1);
        t->bucket = bucket;
        t->ready = true;
        t->visible = true;
    }
    g_title_catalog.loading = false;
}

