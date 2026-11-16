#!/usr/bin/env python3
"""pkg/mksample.py - "Coda", PsiAmp's sample: a short chiptune jingle made
here from nothing (pulse-wave lead and arpeggios, a triangle bass, noise
drums; the tune is our own), so that PsiAmp has something to play out of the
box. Mono, 22,050 samples a second, 48 kbps (the netBook plays 8,000 a
second anyway), about 20 s, ID3 title and artist "Coda".

    python3 pkg/mksample.py OUT.mp3        (needs ffmpeg with libmp3lame)
"""
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

R = 22050
BPM = 132
STEP = 60 / BPM / 4                    # a sixteenth


def midi(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def pulse(f, n, duty=0.25):
    t = np.arange(n) / R
    return np.where((t * f) % 1.0 < duty, 1.0, -1.0)


def tri(f, n):
    t = np.arange(n) / R
    p = (t * f) % 1.0
    return 4 * np.abs(p - 0.5) - 1


def env(n, a=0.004, d=0.18, s=0.55, rel=0.03):
    t = np.arange(n) / R
    e = np.minimum(1, t / a) * (s + (1 - s) * np.exp(-t / d))
    tail = max(1, int(rel * R))
    e[-tail:] *= np.linspace(1, 0, tail)
    return e


def noise(n, seed, decay):
    rng = np.random.default_rng(seed)
    x = np.sign(rng.standard_normal(n))      # 1-bit noise, as the old sound chips
    return x * np.exp(-np.arange(n) / (decay * R))


# the tune: four bars of a lead over arpeggios, twice, then an ending.
# Lead in (MIDI note, sixteenths); None is a rest
LEAD = [(76, 2), (79, 2), (84, 4), (83, 2), (79, 2), (76, 4),
        (77, 2), (81, 2), (84, 4), (86, 2), (84, 2), (81, 4),
        (79, 2), (83, 2), (86, 4), (88, 2), (86, 2), (83, 2), (79, 2),
        (84, 6), (None, 2), (79, 2), (84, 2), (88, 4)]
CHORDS = [(60, 64, 67), (65, 69, 72), (67, 71, 74), (60, 64, 67)]   # C F G C, a bar each
BASS = [48, 53, 55, 48]

bars = 4
reps = 2
n_bar = int(16 * STEP * R)
total = n_bar * bars * reps + int(2.2 * R)
mix = np.zeros(total)

def add(at, x, gain):
    i = int(at)
    j = min(total, i + len(x))
    mix[i:j] += gain * x[:j - i]

for rep in range(reps):
    base = rep * bars * n_bar
    # arpeggios: a sixteenth each, the chord's notes up and over
    for b, ch in enumerate(CHORDS):
        for s in range(16):
            m = ch[s % 3] + (12 if (s // 3) % 2 else 0)
            n = int(STEP * R)
            add(base + b * n_bar + s * n, pulse(midi(m), n, 0.125) * env(n, d=0.05, s=0.2), 0.10)
    # the bass: eighths, root and octave
    for b, root in enumerate(BASS):
        for s in range(8):
            n = int(2 * STEP * R)
            m = root - 12 + (12 if s % 2 else 0)
            add(base + b * n_bar + s * n, tri(midi(m), n) * env(n, d=0.2, s=0.7), 0.30)
    # drums: a kick on the beat, a snare on 2 and 4, hats on the off-beats
    for b in range(bars):
        for s in range(16):
            at = base + b * n_bar + s * int(STEP * R)
            if s % 4 == 0:
                n = int(0.12 * R)
                f = np.linspace(140, 45, n)
                add(at, np.sin(2 * np.pi * np.cumsum(f) / R) * np.exp(-np.arange(n) / (0.05 * R)), 0.45)
            if s in (4, 12):
                add(at, noise(int(0.12 * R), s + 7 * b, 0.04), 0.18)
            if s % 2 == 1:
                add(at, noise(int(0.03 * R), 100 + s, 0.008), 0.06)
    # the lead (an octave up the second time round)
    at = base
    for m, d in LEAD:
        n = int(d * STEP * R)
        if m is not None:
            mm = m + (12 if rep == 1 and d <= 2 else 0)
            vib = 1 + 0.004 * np.sin(2 * np.pi * 6 * np.arange(n) / R) * (np.arange(n) > 0.1 * R)
            t = np.arange(n) / R
            x = np.where((np.cumsum(midi(mm) * vib) / R) % 1.0 < 0.5, 1.0, -1.0)
            add(at, x * env(n, d=0.3, s=0.6), 0.16)
        at += n

# the ending: a C major chord, arpeggiated up and held
end = reps * bars * n_bar
for k, m in enumerate((60, 64, 67, 72, 76, 84)):
    n = int(1.8 * R) - k * int(STEP * R)
    add(end + k * int(STEP * R), pulse(midi(m), n, 0.25) * env(n, d=0.5, s=0.3, rel=0.4), 0.09)
n = int(1.8 * R)
add(end, tri(midi(36), n) * env(n, d=0.6, s=0.4, rel=0.5), 0.35)

mix /= np.abs(mix).max() * 1.1
pcm = (mix * 32767).astype('<i2')

out = sys.argv[1]
with tempfile.TemporaryDirectory() as tmp:
    w = os.path.join(tmp, 'coda.wav')
    with wave.open(w, 'wb') as f:
        f.setnchannels(1); f.setsampwidth(2); f.setframerate(R); f.writeframes(pcm.tobytes())
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-i', w, '-c:a', 'libmp3lame', '-b:a', '48k',
                    '-ac', '1', '-ar', str(R), '-metadata', 'title=Coda', '-metadata', 'artist=Coda',
                    '-id3v2_version', '3', '-write_xing', '1', out], check=True)
print(out, os.path.getsize(out), 'bytes,', round(len(pcm) / R, 1), 's')
