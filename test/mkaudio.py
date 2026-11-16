#!/usr/bin/env python3
"""test/mkaudio.py - PsiAmp's test sounds, all made here (no recordings):
  OUT/psiamp-test.mp3   speech (ffmpeg's flite) then a short tune synthesised
                        below (chords, bass, a hi-hat), stereo 44.1 kHz,
                        128 kbps joint stereo, ID3 title and artist
  OUT/tune22.mp3        the tune at 22.05 kHz mono 64 kbps (MPEG-2)
  OUT/tune48.mp3        the tune at 48 kHz stereo 192 kbps
  OUT/sweep.mp3         a 20 Hz - 20 kHz sweep, for the resampler's alias test
  OUT/tones.wav         three tones, 16-bit 22.05 kHz stereo PCM (WAV playback)
  OUT/*.ref.wav         the sources as made
    python3 test/mkaudio.py OUT
"""
import subprocess, sys, os, wave
import numpy as np

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
R = 44100

def wav(path, x, rate):
    x = np.clip(x, -1, 1)
    a = (x * 32767).astype('<i2')
    ch = 1 if a.ndim == 1 else a.shape[1]
    with wave.open(path, 'wb') as w:
        w.setnchannels(ch); w.setsampwidth(2); w.setframerate(rate); w.writeframes(a.tobytes())

def ff(*a):
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', *a], check=True)

def note(f, dur, kind='saw', rate=R):
    t = np.arange(int(dur * rate)) / rate
    x = np.zeros_like(t)
    if kind == 'saw':
        for k in range(1, 12):
            if f * k < rate / 2: x += np.sin(2 * np.pi * f * k * t) / k
        x *= 0.5
    elif kind == 'bass':
        x = np.sin(2 * np.pi * f * t) + 0.3 * np.sin(4 * np.pi * f * t)
    env = np.minimum(1, t / 0.01) * np.exp(-t * 3)
    return x * env

def midi(m): return 440 * 2 ** ((m - 69) / 12)

# the tune: C, Am, F, G, two bars each, eighth-note arpeggios at 120 bpm
bpm = 120; eighth = 60 / bpm / 2
chords = [(60, 64, 67), (57, 60, 64), (53, 57, 60), (55, 59, 62)]
length = int(len(chords) * 2 * 8 * eighth * R) + R
L = np.zeros(length); Rr = np.zeros(length)
pos = 0
for rep in range(2):
    for ch in chords:
        b = note(midi(ch[0] - 24), 8 * eighth, 'bass')
        i = int(pos * R); L[i:i + len(b)] += 0.35 * b; Rr[i:i + len(b)] += 0.35 * b
        for s in range(8):
            m = ch[s % 3] + (12 if s >= 4 else 0)
            n = note(midi(m), eighth * 1.5)
            i = int((pos + s * eighth) * R)
            pan = 0.5 + 0.4 * np.sin(s)
            L[i:i + len(n)] += 0.25 * (1 - pan) * n; Rr[i:i + len(n)] += 0.25 * pan * n
            hat = np.random.default_rng(s + 8 * rep).standard_normal(int(0.03 * R)) * np.exp(-np.arange(int(0.03 * R)) / 200)
            L[i:i + len(hat)] += 0.05 * hat; Rr[i:i + len(hat)] += 0.05 * hat
        pos += 8 * eighth
    if rep == 0:
        pass
tune = np.stack([L, Rr], 1)[:int(pos * R) + R // 2]
tune /= np.abs(tune).max() * 1.12

# speech
ff('-f', 'lavfi', '-i', "flite=text='This is P S I Amp playing an M P 3 file on the Psion netBook.':voice=slt", '-ar', str(R), '-ac', '1', f'{out}/speech.wav')
with wave.open(f'{out}/speech.wav') as w:
    sp = np.frombuffer(w.readframes(w.getnframes()), '<i2') / 32768
sp = sp / np.abs(sp).max() * 0.8
full = np.concatenate([np.stack([sp, sp], 1), np.zeros((R // 2, 2)), tune])
wav(f'{out}/psiamp-test.ref.wav', full, R)
meta = ['-metadata', 'title=Coda Test Tune', '-metadata', 'artist=PsiAmp', '-id3v2_version', '3']
ff('-i', f'{out}/psiamp-test.ref.wav', '-c:a', 'libmp3lame', '-b:a', '128k', '-joint_stereo', '1', *meta, f'{out}/psiamp-test.mp3')
wav(f'{out}/tune.ref.wav', tune, R)
ff('-i', f'{out}/tune.ref.wav', '-ar', '22050', '-ac', '1', '-c:a', 'libmp3lame', '-b:a', '64k',
   '-metadata', 'title=Tune at 22 kHz', '-metadata', 'artist=PsiAmp', f'{out}/tune22.mp3')
ff('-i', f'{out}/tune.ref.wav', '-ar', '48000', '-c:a', 'libmp3lame', '-b:a', '192k',
   '-metadata', 'title=Tune at 48 kHz', '-metadata', 'artist=PsiAmp', f'{out}/tune48.mp3')
# a log sweep, 20 Hz to 20 kHz in 10 s, mono at -6 dB
t = np.arange(10 * R) / R
f0, f1 = 20, 20000
k = np.log(f1 / f0) / 10
sw = 0.5 * np.sin(2 * np.pi * f0 * (np.exp(k * t) - 1) / k)
wav(f'{out}/sweep.ref.wav', sw, R)
ff('-i', f'{out}/sweep.ref.wav', '-c:a', 'libmp3lame', '-b:a', '128k', '-metadata', 'title=Sweep', f'{out}/sweep.mp3')
# WAV: three tones (440, 880, 1320 Hz), 16-bit stereo at 22.05 kHz
tr = 22050; t = np.arange(int(1.5 * tr)) / tr
tones = np.concatenate([0.4 * np.sin(2 * np.pi * f * t) * np.minimum(1, (1.5 - t) * 20) for f in (440, 880, 1320)])
wav(f'{out}/tones.wav', np.stack([tones, tones * 0.8], 1), tr)
os.remove(f'{out}/speech.wav')
for n in sorted(os.listdir(out)): print(n, os.path.getsize(f'{out}/{n}'))
