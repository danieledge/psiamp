/* paskinc.h - classic Winamp 2 skins read in portable C (paskinc.c) */
#ifndef PASKINC_H
#define PASKINC_H
#include "pacore.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct PaZipEntry { int method, csize, usize, offset; } PaZipEntry;
/* a member by its name's last part, any case: 0, or -1 if not there */
int pa_zip_find(const unsigned char *zip, int len, const char *name, PaZipEntry *e);
/* the member, stored or deflated, in a new buffer (pa_free it), 0 on failure */
unsigned char *pa_zip_read(const unsigned char *zip, int len, const PaZipEntry *e);

/* a BMP's size: 0 or -1; then its pixels as RGB, w * h * 3, top row first
 * (1, 4, 8 bits with a palette, RLE8, 16, 24 and 32 bits) */
int pa_bmp_size(const unsigned char *bmp, int len, int *w, int *h);
int pa_bmp_decode(const unsigned char *bmp, int len, unsigned char *rgb);

/* viscolor.txt: up to 24 colours "r,g,b" a line -> how many */
int pa_viscolor_parse(const char *text, int len, unsigned char *rgb72);
/* pledit.txt: Normal, Current, NormalBG, SelectedBG -> how many found */
int pa_pledit_parse(const char *text, int len, unsigned char *rgb12);

#ifdef __cplusplus
}
#endif
#endif
