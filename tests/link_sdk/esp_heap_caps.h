#pragma once
#include "cyd.h"
#define MALLOC_CAP_SPIRAM 8
#define MALLOC_CAP_8BIT 4
void *heap_caps_calloc(size_t, size_t, unsigned);
void heap_caps_free(void *);
