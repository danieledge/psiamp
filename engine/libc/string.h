/* string.h - the little PsiAmp's C uses (Helix, zlib), over EUSER: memcpy
 * and memset are EUSER's own exports; memmove and the rest are in the
 * program (pashim.cpp). The SDK's libc headers want ESTLIB. */
#ifndef PA_STRING_H
#define PA_STRING_H
typedef unsigned int size_t;
#ifndef NULL
#define NULL 0
#endif
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
#endif
