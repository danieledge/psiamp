/* pacore.c - PsiAmp's sound path (pacore.h). Portable C, integers only. */
#include "pacore.h"
#include "pakernel.h"

/* ---- resampling ------------------------------------------------------- */

static int gcd(int a, int b)
{
	while (b) { int t = a % b; a = b; b = t; }
	return a;
}

/* the kernel at u from its centre, in 1/256 of a table step (1/PA_KSTEPS
 * of an output sample) */
static int kernel_at(int u)
{
	int i, f;
	if (u < 0) u = -u;
	i = u >> 8;                  /* PA_KSTEPS is 256 */
	f = u & 255;
	if (i >= PA_KLEN - 1) return 0;
	return paKernel[i] + (((paKernel[i + 1] - paKernel[i]) * f) >> 8);
}

/* Output n is at input time n * down / up. Its taps are the input samples
 * from floor(that) - half + 1 to floor(that) + half, half = taps / 2; the
 * kernel is in output samples, so an input sample k input samples from the
 * output's time is k * up / down output samples away. Each phase's taps are
 * scaled to sum to 1 << 14, so the gain does not ripple from phase to phase. */
int pa_rs_open(PaResampler *r, int inRate, int maxBlock)
{
	int g, half, ph, j, sum, *raw;
	r->inRate = inRate;
	g = gcd(inRate, PA_OUT_RATE);
	r->up = PA_OUT_RATE / g;
	r->down = inRate / g;
	/* down >= up: input at 8000 a second or more (MP3's lowest is 8000) */
	if (r->down < r->up) { r->up = 1; r->down = 1; }
	half = (PA_KHALF * r->down + r->up - 1) / r->up;    /* input samples each side */
	if (r->up == r->down) half = 1;
	r->taps = 2 * half;
	r->coef = (short *)pa_alloc(r->up * r->taps * 2);
	raw = (int *)pa_alloc(r->taps * 4);
	r->histMax = r->taps + maxBlock + 4;
	r->hist = (short *)pa_alloc(r->histMax * 2);
	if (!r->coef || !raw || !r->hist) { pa_free(raw); pa_rs_close(r); return -1; }
	for (ph = 0; ph < r->up; ph++) {
		short *c = r->coef + ph * r->taps;
		if (r->up == r->down) {          /* 8000 in: as it is */
			c[0] = 0; c[1] = 1 << 14;
			continue;
		}
		/* the output's time is ph / up input samples after input sample
		 * half - 1 of the taps; tap j is (j - (half - 1)) input samples on,
		 * so (j - half + 1) * up - ph in 1/up input samples, which is
		 * that * PA_KSTEPS / down table steps */
		sum = 0;
		for (j = 0; j < r->taps; j++) {
			int d = (j - half + 1) * r->up - ph;
			raw[j] = kernel_at((d * (PA_KSTEPS * 256)) / r->down);   /* (|d| < 10000: no overflow) */
			sum += raw[j];
		}
		for (j = 0; j < r->taps; j++)
			c[j] = (short)(sum ? (raw[j] << 14) / sum : 0);    /* (raw < 1 << 15: no 64-bit division) */
	}
	pa_free(raw);
	pa_rs_reset(r);
	return 0;
}

void pa_rs_close(PaResampler *r)
{
	pa_free(r->coef); r->coef = 0;
	pa_free(r->hist); r->hist = 0;
}

void pa_rs_reset(PaResampler *r)
{
	int i;
	for (i = 0; i < r->taps - 1; i++) r->hist[i] = 0;
	r->histLen = r->taps - 1;
	r->phase = 0;
	r->pos = 0;
}

int pa_rs_run(PaResampler *r, const short *in, int n, short *out)
{
	int i, got = 0, keep;
	short *h = r->hist;
	if (r->histLen + n > r->histMax) n = r->histMax - r->histLen;
	for (i = 0; i < n; i++) h[r->histLen + i] = in[i];
	r->histLen += n;
	while (r->pos + r->taps <= r->histLen) {
		const short *x = h + r->pos;
		const short *c = r->coef + r->phase * r->taps;
		int j, acc = 0;
		for (j = 0; j + 4 <= r->taps; j += 4)
			acc += x[j] * c[j] + x[j + 1] * c[j + 1] + x[j + 2] * c[j + 2] + x[j + 3] * c[j + 3];
		for (; j < r->taps; j++) acc += x[j] * c[j];
		acc = (acc + (1 << 13)) >> 14;
		if (acc > 32767) acc = 32767; else if (acc < -32768) acc = -32768;
		out[got++] = (short)acc;
		/* the next output: down / up input samples on */
		r->phase += r->down;
		while (r->phase >= r->up) { r->phase -= r->up; r->pos++; }
	}
	/* keep what the next outputs need */
	keep = r->histLen - r->pos;
	for (i = 0; i < keep; i++) h[i] = h[r->pos + i];
	r->histLen = keep;
	r->pos = 0;
	return got;
}

int pa_downmix(short *pcm, int frames, int chans)
{
	int i;
	if (chans != 2) return frames;
	for (i = 0; i < frames; i++)
		pcm[i] = (short)((pcm[2 * i] + pcm[2 * i + 1]) >> 1);
	return frames;
}

/* ---- A-law ------------------------------------------------------------ */

unsigned char pa_alaw(int s)
{
	int mask, seg, aval;
	s >>= 3;                                    /* 13 bits */
	if (s >= 0) mask = 0xd5;
	else { mask = 0x55; s = -s - 1; }
	if (s > 0xfff) s = 0xfff;
	if (s < 32) aval = s >> 1;
	else {
		seg = 1;
		while (s >= (64 << (seg - 1)) && seg < 7) seg++;
		aval = (seg << 4) | ((s >> seg) & 0x0f);
	}
	return (unsigned char)(aval ^ mask);
}

int pa_unalaw(unsigned char a)
{
	int t, seg;
	a ^= 0x55;
	t = (a & 0x0f) << 4;
	seg = (a & 0x70) >> 4;
	if (seg == 0) t += 8;
	else if (seg == 1) t += 0x108;
	else { t += 0x108; t <<= seg - 1; }
	return (a & 0x80) ? t : -t;
}

void pa_to_alaw(const short *in, int n, unsigned char *out, int volume)
{
	int i;
	if (volume >= 256)
		for (i = 0; i < n; i++) out[i] = pa_alaw(in[i]);
	else
		for (i = 0; i < n; i++) out[i] = pa_alaw((in[i] * volume) >> 8);
}

/* ---- the visualiser --------------------------------------------------- */

/* 19 bands: their centres in 1/128 of 8000 a second (62.5 Hz), roughly
 * evenly on a log scale from 125 Hz to 3.75 kHz. A Goertzel filter a band
 * over the 128 samples (Q14 coefficients: 2 cos(2 pi k / 128)) */
static const int paCos2[PA_VIS_BARS] = {
	32610, 32413, 32138, 31786, 31357, 30853, 30274, 28899, 27246, 25330, 23170,
	19520, 15447, 9512, 3212, -6393, -15447, -25330, -32138 };

static int isqrt(unsigned int x)
{
	unsigned int r = 0, b = 1u << 30;
	while (b > x) b >>= 2;
	while (b) {
		if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
		else r >>= 1;
		b >>= 2;
	}
	return (int)r;
}

void pa_vis_bars(const short *s, unsigned char *bars)
{
	int b, i;
	for (b = 0; b < PA_VIS_BARS; b++) {
		int c = paCos2[b], q1 = 0, q2 = 0, q0, a, d, v, mag, n;
		for (i = 0; i < 128; i++) {
			q0 = ((c * q1) >> 14) - q2 + (s[i] >> 6);     /* (|q| stays under 1 << 17) */
			q2 = q1; q1 = q0;
		}
		/* |X|^2 = q1^2 + q2^2 - q1 q2 c; a full-scale sine in its band: |X| about 4096 */
		a = q1 >> 3; d = q2 >> 3;
		v = a * a + d * d - ((a * ((d * c) >> 14)));
		mag = isqrt(v > 0 ? (unsigned int)v : 0);
		/* 3 dB a count: n is about 24 at full scale; 16 bars from 4 to 24 (60 dB) */
		n = 0;
		while (mag > 1) { mag = (mag * 181) >> 8; n++; }
		n = (n - 4) * 16 / 20;
		bars[b] = (unsigned char)(n < 0 ? 0 : n > 16 ? 16 : n);
	}
}

void pa_vis_scope(const short *s, unsigned char *pts)
{
	int i;
	for (i = 0; i < 76; i++) {
		int v = s[(i * 128) / 76];
		v = 8 - (v >> 12);                         /* -32768..32767 -> 16..1 */
		if (v < 0) v = 0; else if (v > 15) v = 15;
		pts[i] = (unsigned char)v;
	}
}

/* ---- WAV -------------------------------------------------------------- */

static int le16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static int le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

int pa_wav_parse(const unsigned char *b, int len, PaWavInfo *w)
{
	int at = 12, haveFmt = 0;
	if (len < 12 || b[0] != 'R' || b[1] != 'I' || b[2] != 'F' || b[3] != 'F' ||
		b[8] != 'W' || b[9] != 'A' || b[10] != 'V' || b[11] != 'E') return -1;
	while (at + 8 <= len) {
		int sz = le32(b + at + 4);
		const unsigned char *c = b + at;
		if (c[0] == 'f' && c[1] == 'm' && c[2] == 't' && c[3] == ' ' && at + 24 <= len) {
			w->format = le16(c + 8);
			w->chans = le16(c + 10);
			w->rate = le32(c + 12);
			w->bits = le16(c + 22);
			if (w->format == 0xfffe && sz >= 40 && at + 8 + 26 <= len) w->format = le16(c + 8 + 24);
			haveFmt = 1;
		}
		else if (c[0] == 'd' && c[1] == 'a' && c[2] == 't' && c[3] == 'a') {
			if (!haveFmt) return -1;
			w->dataStart = at + 8;
			w->dataLen = sz;
			if (w->chans < 1 || w->chans > 2 || w->rate < 4000 || w->rate > 96000) return -1;
			if (w->format == 1 && w->bits != 8 && w->bits != 16) return -1;
			if ((w->format == 6 || w->format == 7) && w->bits != 8) return -1;
			if (w->format != 1 && w->format != 6 && w->format != 7) return -1;
			return 0;
		}
		/* (a size past the end of what is here: no overflow of at) */
		if (sz < 0 || sz > len - at - 8) return -1;
		at += 8 + sz + (sz & 1);
	}
	return -1;
}

/* ---- ID3 -------------------------------------------------------------- */

int pa_id3v2_len(const unsigned char *b, int len)
{
	int n;
	if (len < 10 || b[0] != 'I' || b[1] != 'D' || b[2] != '3') return 0;
	if ((b[6] | b[7] | b[8] | b[9]) & 0x80) return 0;
	n = (b[6] << 21) | (b[7] << 14) | (b[8] << 7) | b[9];
	n += 10;
	if (b[5] & 0x10) n += 10;                  /* a footer */
	return n;
}

/* a text frame's content (its encoding byte first) as Latin-1 */
static void id3_text(const unsigned char *p, int n, char *out, int cap)
{
	int enc, k = 0, i;
	if (n < 1 || cap < 1) return;
	enc = p[0]; p++; n--;
	if (enc == 1 || enc == 2) {                 /* UTF-16 (with a BOM: 1) */
		int big = enc == 2;
		if (n >= 2 && p[0] == 0xff && p[1] == 0xfe) { big = 0; p += 2; n -= 2; }
		else if (n >= 2 && p[0] == 0xfe && p[1] == 0xff) { big = 1; p += 2; n -= 2; }
		for (i = 0; i + 1 < n && k < cap - 1; i += 2) {
			int ch = big ? (p[i] << 8) | p[i + 1] : p[i] | (p[i + 1] << 8);
			if (!ch) break;
			out[k++] = (char)(ch < 256 ? ch : '?');
		}
	}
	else if (enc == 3) {                         /* UTF-8 */
		for (i = 0; i < n && k < cap - 1; i++) {
			int ch = p[i];
			if (!ch) break;
			if (ch >= 0xc0 && ch < 0xe0 && i + 1 < n) { ch = ((ch & 0x1f) << 6) | (p[i + 1] & 0x3f); i++; }
			else if (ch >= 0xe0) { while (i + 1 < n && (p[i + 1] & 0xc0) == 0x80) i++; ch = '?'; }
			out[k++] = (char)(ch < 256 ? ch : '?');
		}
	}
	else
		for (i = 0; i < n && k < cap - 1 && p[i]; i++) out[k++] = (char)p[i];
	while (k > 0 && out[k - 1] == ' ') k--;
	out[k] = 0;
}

int pa_id3v2_text(const unsigned char *b, int len, char *title, char *artist, int cap)
{
	int ver, at = 10, end, found = 0;
	int tag = pa_id3v2_len(b, len);
	if (!tag) return 0;
	end = tag < len ? tag : len;
	ver = b[3];
	if (b[5] & 0x40 && ver >= 3 && len >= 14) {   /* an extended header */
		if (ver == 4) at += (b[10] << 21) | (b[11] << 14) | (b[12] << 7) | b[13];
		else at += 4 + ((b[10] << 24) | (b[11] << 16) | (b[12] << 8) | b[13]);
		if (at < 10 || at > end) return 0;        /* (a size that is not there) */
	}
	while (at + (ver == 2 ? 6 : 10) <= end) {
		const unsigned char *f = b + at;
		int sz, hd;
		if (f[0] == 0) break;
		if (ver == 2) { sz = (f[3] << 16) | (f[4] << 8) | f[5]; hd = 6; }
		else if (ver == 4) { sz = (f[4] << 21) | (f[5] << 14) | (f[6] << 7) | f[7]; hd = 10; }
		else { sz = (f[4] << 24) | (f[5] << 16) | (f[6] << 8) | f[7]; hd = 10; }
		if (sz <= 0 || sz > end - at - hd) break;
		if ((ver == 2 && f[0] == 'T' && f[1] == 'T' && f[2] == '2') ||
			(ver != 2 && f[0] == 'T' && f[1] == 'I' && f[2] == 'T' && f[3] == '2'))
			{ id3_text(f + hd, sz, title, cap); found = 1; }
		else if ((ver == 2 && f[0] == 'T' && f[1] == 'P' && f[2] == '1') ||
			(ver != 2 && f[0] == 'T' && f[1] == 'P' && f[2] == 'E' && f[3] == '1'))
			{ id3_text(f + hd, sz, artist, cap); found = 1; }
		at += hd + sz;
	}
	return found;
}

static void v1_field(const unsigned char *p, int n, char *out, int cap)
{
	int k = 0, i;
	for (i = 0; i < n && p[i] && k < cap - 1; i++) out[k++] = (char)p[i];
	while (k > 0 && out[k - 1] == ' ') k--;
	out[k] = 0;
}

int pa_id3v1_text(const unsigned char *t, char *title, char *artist, int cap)
{
	if (t[0] != 'T' || t[1] != 'A' || t[2] != 'G') return 0;
	v1_field(t + 3, 30, title, cap);
	v1_field(t + 33, 30, artist, cap);
	return title[0] || artist[0];
}
