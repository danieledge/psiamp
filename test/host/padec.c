/* padec.c - PsiAmp's decoder and sound path on the PC (test/host).
 *   padec IN.mp3|IN.wav NATIVE.wav NETBOOK.wav [LINEAR8K.wav]
 * NATIVE.wav: Helix's output as decoded (MP3 only), to compare with ffmpeg's;
 * NETBOOK.wav: what PsiAmp gives ESound.ldd (mono, 8000 a second, A-law),
 * decoded back to 16-bit, to listen to and to measure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mp3dec.h"
#include "pacore.h"

void *pa_alloc(int n) { return calloc(1, n); }
void pa_free(void *p) { free(p); }

static void wav_header(FILE *f, int rate, int chans, int bytes)
{
	unsigned char h[44];
	int br = rate * chans * 2;
	memcpy(h, "RIFF", 4); *(int *)(h + 4) = 36 + bytes; memcpy(h + 8, "WAVEfmt ", 8);
	*(int *)(h + 16) = 16; *(short *)(h + 20) = 1; *(short *)(h + 22) = chans;
	*(int *)(h + 24) = rate; *(int *)(h + 28) = br; *(short *)(h + 32) = chans * 2; *(short *)(h + 34) = 16;
	memcpy(h + 36, "data", 4); *(int *)(h + 40) = bytes;
	fseek(f, 0, SEEK_SET); fwrite(h, 1, 44, f);
}

static unsigned char *slurp(const char *p, int *len)
{
	FILE *f = fopen(p, "rb"); unsigned char *b; long n;
	if (!f) { perror(p); exit(1); }
	fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
	b = malloc(n + 16); if (fread(b, 1, n, f) != (size_t)n) exit(1); fclose(f); *len = (int)n; return b;
}

int main(int argc, char **argv)
{
	int len, nativeBytes = 0, outN = 0, rate = 0, chans = 0, frames = 0;
	unsigned char *buf, *p;
	short pcm[1152 * 2], mono8k[1152], *in = pcm;
	unsigned char al[1152];
	FILE *fn, *f8, *fl = 0;
	PaResampler rs;
	int haveRs = 0;
	clock_t t0;
	char title[64] = "", artist[64] = "";
	if (argc < 4) { fprintf(stderr, "usage: padec IN NATIVE.wav NETBOOK.wav\n"); return 1; }
	buf = slurp(argv[1], &len);
	fn = fopen(argv[2], "wb"); f8 = fopen(argv[3], "wb");
	fseek(fn, 44, SEEK_SET); fseek(f8, 44, SEEK_SET);
	if (argc > 4) { fl = fopen(argv[4], "wb"); fseek(fl, 44, SEEK_SET); }   /* LINEAR8K.wav: before A-law */
	t0 = clock();
	if (len > 12 && !memcmp(buf, "RIFF", 4)) {
		PaWavInfo w;
		if (pa_wav_parse(buf, len, &w)) { fprintf(stderr, "not a WAV PsiAmp plays\n"); return 1; }
		printf("WAV format %d, %d ch, %d Hz, %d bits\n", w.format, w.chans, w.rate, w.bits);
		pa_rs_open(&rs, w.rate, 1152); haveRs = 1;
		p = buf + w.dataStart;
		{
			int bps = w.bits / 8 * w.chans, left = w.dataLen / bps;
			while (left > 0) {
				int n = left > 1152 ? 1152 : left, i, k;
				for (i = 0; i < n * w.chans; i++) {
					if (w.format == 1 && w.bits == 16) pcm[i] = (short)(p[2 * i] | (p[2 * i + 1] << 8));
					else if (w.format == 1) pcm[i] = (short)((p[i] - 128) << 8);
					else if (w.format == 6) pcm[i] = (short)pa_unalaw(p[i]);
					else pcm[i] = 0;
				}
				p += n * bps; left -= n;
				pa_downmix(pcm, n, w.chans);
				k = pa_rs_run(&rs, pcm, n, mono8k);
				if (fl) fwrite(mono8k, 2, k, fl);
				if (fl) fwrite(mono8k, 2, k, fl);
			pa_to_alaw(mono8k, k, al, 256);
				for (i = 0; i < k; i++) { short s = (short)pa_unalaw(al[i]); fwrite(&s, 2, 1, f8); }
				outN += k;
			}
		}
	} else {
		HMP3Decoder h = MP3InitDecoder();
		if (getenv("PA_BANDLIMIT")) MP3SetBandLimit(h, atoi(getenv("PA_BANDLIMIT")));   /* as PsiAmp: 4400 */
		int left, tag;
		MP3FrameInfo fi;
		tag = pa_id3v2_len(buf, len);
		pa_id3v2_text(buf, len, title, artist, sizeof title);
		if (!title[0] && len > 128) pa_id3v1_text(buf + len - 128, title, artist, sizeof title);
		printf("ID3: \"%s\" by \"%s\" (v2 tag %d bytes)\n", title, artist, tag);
		p = buf + tag; left = len - tag;
		while (left > 0) {
			int off = MP3FindSyncWord(p, left), err, n, i, k;
			if (off < 0) break;
			p += off; left -= off;
			err = MP3Decode(h, &p, &left, pcm, 0);
			if (err == ERR_MP3_INDATA_UNDERFLOW || err == ERR_MP3_MAINDATA_UNDERFLOW) { if (err == ERR_MP3_INDATA_UNDERFLOW) break; continue; }
			if (err) { p++; left--; continue; }
			MP3GetLastFrameInfo(h, &fi);
			if (!haveRs) { rate = fi.samprate; chans = fi.nChans; pa_rs_open(&rs, rate, 1152); haveRs = 1;
				printf("MPEG %d layer %d, %d Hz, %d ch, %d kbps\n", fi.version, fi.layer, rate, chans, fi.bitrate / 1000); }
			fwrite(pcm, 2, fi.outputSamps, fn); nativeBytes += fi.outputSamps * 2;
			n = fi.outputSamps / fi.nChans;
			pa_downmix(in, n, fi.nChans);
			k = pa_rs_run(&rs, in, n, mono8k);
			if (fl) fwrite(mono8k, 2, k, fl);
			pa_to_alaw(mono8k, k, al, 256);
			for (i = 0; i < k; i++) { short s = (short)pa_unalaw(al[i]); fwrite(&s, 2, 1, f8); }
			outN += k; frames++;
		}
		MP3FreeDecoder(h);
	}
	printf("%d frames, %d samples at 8000 (%.2f s), %.3f s CPU on the PC\n", frames, outN, outN / 8000.0,
		(double)(clock() - t0) / CLOCKS_PER_SEC);
	wav_header(fn, rate ? rate : 8000, chans ? chans : 1, nativeBytes);
	wav_header(f8, 8000, 1, outN * 2);
	if (fl) { wav_header(fl, 8000, 1, outN * 2); fclose(fl); }
	fclose(fn); fclose(f8);
	return 0;
}
