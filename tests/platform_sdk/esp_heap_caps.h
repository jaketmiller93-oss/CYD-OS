#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1u
#define MALLOC_CAP_8BIT 2u
#define MALLOC_CAP_INTERNAL 4u
static int host_psram_fail;
static int host_calloc_fail;
static unsigned host_psram_calls;
static inline void *heap_caps_calloc(size_t n,size_t size,uint32_t caps) {
    assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));host_psram_calls++;
    return host_psram_fail||host_calloc_fail?NULL:calloc(n,size);
}
static inline void *heap_caps_realloc(void *p,size_t n,uint32_t caps) {
    assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));host_psram_calls++;
    return host_psram_fail?NULL:realloc(p,n);
}
static inline unsigned heap_caps_get_free_size(uint32_t caps) {
    return caps&MALLOC_CAP_INTERNAL?16000:8*1024*1024;
}
static inline unsigned heap_caps_get_largest_free_block(uint32_t caps) {
    return caps&MALLOC_CAP_INTERNAL?13000:8*1024*1024;
}
