#!/usr/bin/env python3
"""engine/mkkernel.py - the low-pass kernel PsiAmp resamples with (pakernel.h).

The netBook plays 8000 samples a second (ESound.ldd's A-law), so every MP3
rate goes down to 8000: a Kaiser-windowed sinc, cut off at CUTOFF of the
output rate, HALF output samples each side, tabulated at STEPS points an
output sample (one side: it is symmetric), as 16-bit numbers. pacore.c
makes each phase's taps from it by linear interpolation, in integers (the
netBook has no FPU, and the SDK links no soft float).
    python3 engine/mkkernel.py > engine/pakernel.h
"""
import numpy as np

HALF = 20          # output samples each side (the transition is about 8000 / HALF * 2.2 Hz wide)
STEPS = 256        # table points an output sample
CUTOFF = 0.46      # of the output rate: 3680 Hz at 8000 (-6 dB)
BETA = 7.0         # Kaiser: about 70 dB down in the stop band

n = HALF * STEPS + 1
u = np.arange(n) / STEPS                          # output samples from the centre
h = 2 * CUTOFF * np.sinc(2 * CUTOFF * u) * np.kaiser(2 * n - 1, BETA)[n - 1:]
q = np.round(h / h.max() * 32767).astype(int)
print('/* pakernel.h - made by engine/mkkernel.py: do not edit */')
print(f'#define PA_KHALF {HALF}')
print(f'#define PA_KSTEPS {STEPS}')
print(f'#define PA_KLEN {n}')
print('static const short paKernel[PA_KLEN] = {')
for i in range(0, n, 16):
    print('\t' + ', '.join(str(v) for v in q[i:i + 16]) + ',')
print('};')
