#!/usr/bin/env python3
"""test/mkskin2.py - "Amber", a test skin: Coda (pkg/mkskin.py) with its
colours changed (amber LEDs, a navy case), in another skin's file, to test
choosing a skin. Ours too: nothing from any other player.
    python3 test/mkskin2.py OUT.wsz
"""
import io, os, sys, zipfile
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '../pkg'))
import mkskin  # noqa: E402

SWAP = {(0, 255, 0): (255, 204, 0), (0, 51, 0): (51, 51, 0), (0, 153, 0): (204, 153, 0),
        (51, 51, 51): (0, 0, 102), (102, 102, 102): (51, 51, 153), (0, 102, 102): (102, 0, 51),
        (0, 153, 153): (153, 0, 51), (51, 204, 204): (255, 102, 102)}

def recolour(im):
    px = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            c = px[x, y]
            if c in SWAP: px[x, y] = SWAP[c]
    return im

files = mkskin.skin()
with zipfile.ZipFile(sys.argv[1], 'w', zipfile.ZIP_DEFLATED) as z:
    for name, im in files.items():
        z.writestr('Amber/' + name.upper() if name == 'main.bmp' else 'Amber/' + name, mkskin.bmp_bytes(recolour(im)))
    z.writestr('Amber/viscolor.txt', ''.join(f'{r},{g},{b}\r\n' for r, g, b in (SWAP.get(c, c) for c in mkskin.VISCOLOR)))
    z.writestr('Amber/pledit.txt', '[Text]\r\nNormal=#FFCC00\r\nCurrent=#FFFFFF\r\nNormalBG=#000033\r\nSelectedBG=#660033\r\n')
print(sys.argv[1], os.path.getsize(sys.argv[1]))
