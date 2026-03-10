#ifndef PREVIEW_MANAGER_H
#define PREVIEW_MANAGER_H

#include <3ds/types.h>
#include <stdbool.h>

void preview_manager_init(void);
void preview_manager_shutdown(void);
void preview_request(const char* path, u32 generation);
void preview_cancel(u32 generation);
void preview_update(int budget);
bool preview_get_ready_texture(u32 generation, const u8** out_rgba, int* out_w, int* out_h);

#endif
