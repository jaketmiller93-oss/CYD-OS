#pragma once
#include "fake_sdk.h"
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_DMA 2
#define MALLOC_CAP_8BIT 4
static inline void *heap_caps_malloc(size_t n,int flags){(void)flags;return malloc(n);}
static inline unsigned heap_caps_get_largest_free_block(int flags){(void)flags;return 120000;}
static inline unsigned esp_get_free_heap_size(void){return 200000;}
