#ifndef GPU_MEMCPY_SSE4_H_
#define GPU_MEMCPY_SSE4_H_

#include <stddef.h>

void *gpu_memcpy(void *restrict d, const void *restrict s, size_t size);

#endif
