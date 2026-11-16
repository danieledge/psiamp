/* pacore.h - PsiAmp's sound path, in portable C (the netBook and the host
 * tests build the same code): stereo to mono, any rate to the netBook's
 * 8000 samples a second, and A-law, which is all ESound.ldd plays; the
 * visualiser's levels; WAV and ID3 headers.
 *
 * No floating point (the SA-1100 has no FPU and the SDK links no soft
 * float), no writable static data (a program DLL cannot have any), no
 * library calls but pa_alloc and pa_free (pahost.c or the program's own).
 */
#ifndef PACORE_H
#define PACORE_H

#ifdef __cplusplus
extern "C" {
#endif

#define PA_OUT_RATE 8000

void *pa_alloc(int bytes);           /* zeroed; 0 when there is no memory */
void pa_free(void *p);

/* ---- resampling to PA_OUT_RATE --------------------------------------- */

typedef struct PaResampler {
	int inRate;
	int up, down;          /* output time = input time * up / down, in lowest terms */
	int taps;              /* per phase */
	short *coef;           /* up phases of taps, each summing to 1 << 14 */
	short *hist;           /* the input still needed: taps - 1 + room for a block */
	int histLen;           /* samples in hist */
	int histMax;
	int phase;             /* (output n * down) mod up for the next output */
	int pos;               /* where in hist the next output's taps start */
} PaResampler;

/* 0, or -1 with no memory. maxBlock: the most mono samples given at once */
int pa_rs_open(PaResampler *r, int inRate, int maxBlock);
void pa_rs_close(PaResampler *r);
void pa_rs_reset(PaResampler *r);     /* forget the history (a seek) */
/* n mono samples in; output samples written to out (room for
 * n * 8000 / inRate + 2); -> how many */
int pa_rs_run(PaResampler *r, const short *in, int n, short *out);

/* interleaved stereo (or mono: chans 1) to mono, in place; -> samples */
int pa_downmix(short *pcm, int frames, int chans);

/* ---- A-law (G.711) ---------------------------------------------------- */

unsigned char pa_alaw(int s16);
int pa_unalaw(unsigned char a);
/* n samples to A-law; volume 0..256 (256: as decoded) */
void pa_to_alaw(const short *in, int n, unsigned char *out, int volume);

/* ---- the visualiser --------------------------------------------------- */

#define PA_VIS_BARS 19
/* the levels 0..16 of PA_VIS_BARS bands (0 to about 4 kHz, spaced as
 * Winamp's, low on the left) of 128 samples at 8000 a second, and their
 * peak level 0..16 (the oscilloscope's height) */
void pa_vis_bars(const short *s128, unsigned char *bars);
/* 76 points of 128 samples, each 0..15 (the oscilloscope) */
void pa_vis_scope(const short *s128, unsigned char *pts);

/* ---- file headers ----------------------------------------------------- */

typedef struct PaWavInfo {
	int format;            /* 1 PCM, 6 A-law, 7 mu-law */
	int chans, rate, bits;
	int dataStart, dataLen;
} PaWavInfo;
/* the RIFF header in buf (at least its first 4 KB, or the whole file); 0 or -1 */
int pa_wav_parse(const unsigned char *buf, int len, PaWavInfo *w);

/* an ID3v2 tag at buf[0]: its length with its header, else 0 */
int pa_id3v2_len(const unsigned char *buf, int len);
/* title and artist from an ID3v2 tag (TIT2, TPE1; v2.2 TT2, TP1) or a
 * 128-byte ID3v1 tag, as Latin-1 into the given buffers (cap bytes with
 * the 0); -> 1 if either was found */
int pa_id3v2_text(const unsigned char *buf, int len, char *title, char *artist, int cap);
int pa_id3v1_text(const unsigned char *tag128, char *title, char *artist, int cap);

#ifdef __cplusplus
}
#endif
#endif
