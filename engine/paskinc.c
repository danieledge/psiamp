/* paskinc.c - classic Winamp 2 skins (.wsz: a zip of BMPs and text files)
 * read in portable C: the zip's directory, its members inflated (psion-core's
 * zlib), BMP pictures to 24-bit RGB, and the two text files (viscolor.txt,
 * pledit.txt). PsiAmp's own skin is a .wsz too, so one path reads both. */
#include "paskinc.h"
#include "zlib.h"

static int rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static int rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* ---- the zip ---------------------------------------------------------- */

int pa_zip_find(const unsigned char *z, int len, const char *want, PaZipEntry *e)
{
	int at, eocd = -1, n, i, cd;
	for (at = len - 22; at >= 0 && at >= len - 22 - 65535; at--)
		if (z[at] == 'P' && z[at + 1] == 'K' && z[at + 2] == 5 && z[at + 3] == 6) { eocd = at; break; }
	if (eocd < 0) return -1;
	n = rd16(z + eocd + 10);
	cd = rd32(z + eocd + 16);
	for (i = 0; i < n; i++) {
		int nl, xl, cl, k, j, base, match;
		const unsigned char *h = z + cd;
		if (cd < 0 || cd > len - 46 || rd32(h) != 0x02014b50) return -1;
		nl = rd16(h + 28); xl = rd16(h + 30); cl = rd16(h + 32);
		if (cd + 46 + nl > len) return -1;
		/* the name's last part (skins are often zipped in a folder) */
		base = 0;
		for (k = 0; k < nl; k++) if (h[46 + k] == '/' || h[46 + k] == '\\') base = k + 1;
		match = 1;
		for (j = 0; want[j] || base + j < nl; j++)
			if (base + j >= nl || lower(h[46 + base + j]) != lower((unsigned char)want[j])) { match = 0; break; }
		if (match && nl > base) {
			int loc = rd32(h + 42), lnl, lxl;
			if (loc < 0 || loc > len - 30 || rd32(z + loc) != 0x04034b50) return -1;
			lnl = rd16(z + loc + 26); lxl = rd16(z + loc + 28);
			e->method = rd16(h + 10);
			e->csize = rd32(h + 20);
			e->usize = rd32(h + 24);
			e->offset = loc + 30 + lnl + lxl;
			if (e->offset > len || e->csize < 0 || e->csize > len - e->offset || e->usize < 0 || e->usize > (4 << 20)) return -1;
			return 0;
		}
		cd += 46 + nl + xl + cl;
	}
	return -1;
}

static voidpf za(voidpf o, uInt n, uInt s) { (void)o; return pa_alloc((int)(n * s)); }
static void zf(voidpf o, voidpf p) { (void)o; pa_free(p); }

unsigned char *pa_zip_read(const unsigned char *z, int len, const PaZipEntry *e)
{
	unsigned char *out = (unsigned char *)pa_alloc(e->usize + 1);
	(void)len;
	if (!out) return 0;
	if (e->method == 0) {
		int i;
		if (e->csize != e->usize) { pa_free(out); return 0; }
		for (i = 0; i < e->usize; i++) out[i] = z[e->offset + i];
		return out;
	}
	if (e->method == 8) {
		z_stream s;
		int r;
		s.zalloc = za; s.zfree = zf; s.opaque = 0;
		s.next_in = (Bytef *)(z + e->offset); s.avail_in = (uInt)e->csize;
		s.next_out = out; s.avail_out = (uInt)e->usize;
		if (inflateInit2(&s, -MAX_WBITS) != Z_OK) { pa_free(out); return 0; }
		r = inflate(&s, Z_FINISH);
		inflateEnd(&s);
		if (r != Z_STREAM_END && !(r == Z_OK && s.avail_out == 0) && !(r == Z_BUF_ERROR && s.avail_out == 0)) { pa_free(out); return 0; }
		return out;
	}
	pa_free(out);
	return 0;
}

/* ---- BMP -------------------------------------------------------------- */

int pa_bmp_size(const unsigned char *b, int len, int *w, int *h)
{
	int hs;
	if (len < 26 || b[0] != 'B' || b[1] != 'M') return -1;
	hs = rd32(b + 14);
	if (hs == 12) { *w = rd16(b + 18); *h = rd16(b + 20); }
	else if (hs >= 40 && len >= 54) { *w = rd32(b + 18); *h = rd32(b + 22); if (*h < 0) *h = -*h; }
	else return -1;
	if (*w <= 0 || *h <= 0 || *w > 2048 || *h > 2048) return -1;
	return 0;
}

/* to rgb (w * h * 3, top row first) */
int pa_bmp_decode(const unsigned char *b, int len, unsigned char *rgb)
{
	int hs, w, h, bpp, comp = 0, off, ncol, topdown = 0, x, y, stride, palEntry;
	const unsigned char *pal;
	if (pa_bmp_size(b, len, &w, &h)) return -1;
	hs = rd32(b + 14);
	off = rd32(b + 10);
	if (hs == 12) { bpp = rd16(b + 24); palEntry = 3; ncol = 0; }
	else {
		bpp = rd16(b + 28); comp = rd32(b + 30); ncol = rd32(b + 46); palEntry = 4;
		if (rd32(b + 22) < 0) topdown = 1;
	}
	/* (sizes and places from the file: checked as numbers, before any pointer is made) */
	if (hs > len - 14 || off < 0 || off > len) return -1;
	pal = b + 14 + hs;
	if (bpp <= 8 && (ncol <= 0 || ncol > (1 << bpp))) ncol = 1 << bpp;
	if (bpp <= 8 && ncol * palEntry > len - 14 - hs) return -1;
	if (comp == 1 && bpp == 8) {           /* RLE8 */
		const unsigned char *p = b + off, *e = b + len;
		int i;
		for (i = 0; i < w * h * 3; i++) rgb[i] = 0;
		x = 0; y = 0;
		while (p + 1 < e && y < h) {
			int n = p[0], c = p[1];
			p += 2;
			if (n) {
				while (n-- > 0 && x < w) {
					unsigned char *d = rgb + ((topdown ? y : h - 1 - y) * w + x) * 3;
					const unsigned char *q = pal + c * palEntry;
					if (c < ncol) { d[0] = q[2]; d[1] = q[1]; d[2] = q[0]; }
					x++;
				}
			}
			else if (c == 0) { x = 0; y++; }
			else if (c == 1) break;
			else if (c == 2) { if (p + 1 >= e) break; x += p[0]; y += p[1]; p += 2; }
			else {
				int k;
				for (k = 0; k < c && p < e; k++, p++) {
					if (x < w) {
						unsigned char *d = rgb + ((topdown ? y : h - 1 - y) * w + x) * 3;
						const unsigned char *q = pal + *p * palEntry;
						if (*p < ncol) { d[0] = q[2]; d[1] = q[1]; d[2] = q[0]; }
						x++;
					}
				}
				if (c & 1) p++;
			}
		}
		return 0;
	}
	if (comp != 0 && !(comp == 3 && (bpp == 16 || bpp == 32))) return -1;
	stride = ((w * bpp + 31) / 32) * 4;
	if (off + stride * h > len) return -1;
	for (y = 0; y < h; y++) {
		const unsigned char *row = b + off + (topdown ? y : h - 1 - y) * stride;
		unsigned char *d = rgb + y * w * 3;
		for (x = 0; x < w; x++, d += 3) {
			int i;
			switch (bpp) {
			case 1: i = (row[x >> 3] >> (7 - (x & 7))) & 1; goto palette;
			case 4: i = (row[x >> 1] >> ((x & 1) ? 0 : 4)) & 15; goto palette;
			case 8: i = row[x];
			palette:
				if (i < ncol) { const unsigned char *q = pal + i * palEntry; d[0] = q[2]; d[1] = q[1]; d[2] = q[0]; }
				else d[0] = d[1] = d[2] = 0;
				break;
			case 16: { int v = rd16(row + 2 * x); d[0] = (unsigned char)(((v >> 10) & 31) << 3); d[1] = (unsigned char)(((v >> 5) & 31) << 3); d[2] = (unsigned char)((v & 31) << 3); break; }
			case 24: d[0] = row[3 * x + 2]; d[1] = row[3 * x + 1]; d[2] = row[3 * x]; break;
			case 32: d[0] = row[4 * x + 2]; d[1] = row[4 * x + 1]; d[2] = row[4 * x]; break;
			default: return -1;
			}
		}
	}
	return 0;
}

/* ---- the text files --------------------------------------------------- */

/* the next number in t (after any other characters on the line); -1 at the
 * line's end */
static int next_num(const char **t, const char *e)
{
	const char *p = *t;
	int v = 0, any = 0;
	while (p < e && *p != '\n' && !(*p >= '0' && *p <= '9')) {
		if (*p == '/' && p + 1 < e && p[1] == '/') { while (p < e && *p != '\n') p++; break; }
		p++;
	}
	while (p < e && *p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; any = 1; }
	*t = p;
	return any ? v : -1;
}

int pa_viscolor_parse(const char *t, int len, unsigned char *rgb72)
{
	const char *e = t + len;
	int line = 0;
	while (t < e && line < 24) {
		int r = next_num(&t, e), g = next_num(&t, e), b = next_num(&t, e);
		if (r >= 0 && g >= 0 && b >= 0) {
			rgb72[line * 3] = (unsigned char)(r > 255 ? 255 : r);
			rgb72[line * 3 + 1] = (unsigned char)(g > 255 ? 255 : g);
			rgb72[line * 3 + 2] = (unsigned char)(b > 255 ? 255 : b);
			line++;
		}
		while (t < e && *t != '\n') t++;
		if (t < e) t++;
	}
	return line;
}

static int hexv(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	c = lower(c);
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

/* pledit.txt: [Text] Normal=#00FF00 Current=#FFFFFF NormalBG=#000000
 * SelectedBG=#0000FF; -> how many of the four were found */
int pa_pledit_parse(const char *t, int len, unsigned char *rgb12)
{
	static const char *const keys[4] = { "normal", "current", "normalbg", "selectedbg" };
	const char *e = t + len;
	int found = 0;
	while (t < e) {
		const char *ls = t, *eq;
		while (t < e && *t != '\n') t++;
		for (eq = ls; eq < t && *eq != '='; eq++) ;
		if (eq < t) {
			int k;
			for (k = 0; k < 4; k++) {
				const char *key = keys[k], *p = ls;
				int j = 0;
				while (p < eq && (*p == ' ' || *p == '\t')) p++;
				while (key[j] && p < eq && lower(*p) == key[j]) { p++; j++; }
				while (p < eq && (*p == ' ' || *p == '\t')) p++;
				if (!key[j] && p == eq) {
					const char *v = eq + 1;
					int i, c[6], ok = 1;
					while (v < t && (*v == ' ' || *v == '#')) v++;
					for (i = 0; i < 6; i++) { c[i] = v + i < t ? hexv(v[i]) : -1; if (c[i] < 0) ok = 0; }
					if (ok) {
						rgb12[k * 3] = (unsigned char)(c[0] * 16 + c[1]);
						rgb12[k * 3 + 1] = (unsigned char)(c[2] * 16 + c[3]);
						rgb12[k * 3 + 2] = (unsigned char)(c[4] * 16 + c[5]);
						found++;
					}
				}
			}
		}
		if (t < e) t++;
	}
	return found;
}
