#ifndef RUNTIME_CACHE_H
#define RUNTIME_CACHE_H

#include "fmux.h"

void runtime_cache_init(void);
void runtime_cache_shutdown(void);
TargetRuntime* runtime_get(const char* target_id);

#endif
