/* stdlib.h - malloc and free for Helix and zlib (pashim.cpp: the thread's heap) */
#ifndef PA_STDLIB_H
#define PA_STDLIB_H
#include "string.h"
void *malloc(size_t);
void *calloc(size_t, size_t);
void free(void *);
#endif
