#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 0
inline void* heap_caps_calloc(size_t n, size_t size, uint32_t) {return calloc(n, size);}
