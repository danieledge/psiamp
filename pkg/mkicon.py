#!/usr/bin/env python3
"""pkg/mkicon.py - PsiAmp's icon and .aif, in colour as Coda's other icons
(EPOC's 256-colour palette: aif8.py, epoc256.py).

    python3 pkg/mkicon.py DIST PSION_CORE [PREVIEW.png]   (PSION_CORE: core/)
        DIST/PsiAmp.aif: the name, the icon (24, 32, 48: a little player,
        its green display with level bars, the coda sign), and the kinds of
        file it opens (audio/mpeg, audio/x-wav, audio/x-mpegurl and skins,
        interface/x-winamp-skin: PsiAmp.mdl
        recognises them, and the System screen then opens them in PsiAmp).
"""
import os
import sys
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aif8 import write_aif, preview  # noqa: E402  (8 bpp colour, EPOC's palette)

UID = 0x01000AFA
BLACK, G1, G2, G3, G4 = (0, 0, 0), (51, 51, 51), (102, 102, 102), (153, 153, 153), (204, 204, 204)
LED, TEAL = (0, 255, 0), (51, 204, 204)
AMBER, RED = (255, 204, 0), (255, 51, 0)


def icon(n):
    im = Image.new('RGB', (n, n), (255, 255, 255))
    mk = Image.new('1', (n, n), 1)
    d, m = ImageDraw.Draw(im), ImageDraw.Draw(mk)
    s = n / 48.0
    R = lambda v: int(round(v * s))
    body = [R(2), R(8), R(45), R(40)]
    d.rectangle(body, fill=G1, outline=BLACK)
    m.rectangle(body, fill=0)
    d.rectangle([R(2), R(8), R(45), R(13)], fill=(0, 102, 102), outline=BLACK)
    # the display, with level bars
    disp = [R(6), R(16), R(41), R(30)]
    d.rectangle(disp, fill=BLACK, outline=G2)
    bars = [3, 6, 9, 7, 10, 5, 8, 4]
    bw = max(1, R(4) - 1)
    for i, h in enumerate(bars):
        x = R(9) + i * R(4)
        for k in range(h):
            y = R(28) - k * max(1, R(1.2))
            col = LED if k < 6 else AMBER if k < 9 else RED
            d.line([(x, y), (x + bw - 1, y)], fill=col)
    # the transport: play, and the coda sign on the right
    d.polygon([(R(8), R(33)), (R(14), R(35.5)), (R(8), R(38))], fill=LED)
    for x in (R(18), R(24)):
        d.rectangle([x, R(33), x + R(4), R(38)], fill=G3, outline=BLACK)
    cx, cy, r = R(38), R(35.5), max(2, R(3))
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=TEAL)
    d.line([(cx, cy - r - 1), (cx, cy + r + 1)], fill=TEAL)
    d.line([(cx - r - 1, cy), (cx + r + 1, cy)], fill=TEAL)
    return im, mk


if __name__ == '__main__':
    dist, core = sys.argv[1], sys.argv[2]
    icons = [icon(n) for n in (24, 32, 48)]
    types = [('audio/mpeg', 10000), ('audio/x-wav', 10000), ('audio/x-mpegurl', 10000), ('interface/x-winamp-skin', 10000)]
    print(write_aif(f'{dist}/PsiAmp.aif', core, UID, [(1, 'PsiAmp'), (10, 'PsiAmp')], icons, types), 'bytes')
    if len(sys.argv) > 3:
        preview(sys.argv[3], [i for i, _ in icons])
