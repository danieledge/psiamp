"""aif8.py - a program's .aif in colour, as the netBook's own icons are:
8 bits a pixel in EPOC's 256-colour palette (epoc256.py), each icon with a
1-bit mask, and the kinds of file the program opens (its data types).

psion-core's tools/icon/aif.py writes the captions, the UID checksum and
grey bitmaps; the colour bitmaps and the data types are written here. The
data types follow the capability as apparc writes them in ER5
(EAifVersionAddsDataType): the version (1), then the types, each a stream
holding the MIME type and a 16-bit priority (10000: high), as the ROM's
Web.aif has them."""
import os
import struct
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from epoc256 import PALETTE  # noqa: E402

RGB = [(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff) for v in PALETTE]


def nearest(c):
    r, g, b = c[:3]
    return min(range(256), key=lambda i: (RGB[i][0] - r) ** 2 + (RGB[i][1] - g) ** 2 + (RGB[i][2] - b) ** 2)


def bitmap8(img):
    """CFbsBitmap::ExternalizeL, uncompressed, 8 bpp colour (EColor256)"""
    w, h = img.size
    stride = (w + 3) & ~3
    cache = {}
    data = bytearray()
    for y in range(h):
        row = bytearray(stride)
        for x in range(w):
            c = img.getpixel((x, y))
            if c not in cache:
                cache[c] = nearest(c)
            row[x] = cache[c]
        data += row
    return struct.pack('<IIiiiiIIII', 40 + len(data), 40, w, h, 0, 0, 8, 1, 0, 0) + bytes(data)


def write_aif(path, core, uid, captions, icons, datatypes=()):
    """captions: [(language, text)]; icons: [(RGB image, 1-bit mask)];
    datatypes: [(MIME type, priority)]; core: the psion-core checkout"""
    sys.path.insert(0, core + '/tools/icon')
    from aif import bitmap_stream, des8, uid_checksum
    out = bytearray(20)
    cap_hdr, icon_hdr, dt_hdr = [], [], []
    for lang, text in captions:
        cap_hdr.append((len(out), lang)); out += des8(text)
    for img, mask in icons:
        icon_hdr.append((len(out), img.size[0]))
        out += bitmap8(img) + bitmap_stream(mask)
    for mime, prio in datatypes:
        dt_hdr.append((len(out), prio)); out += des8(mime)
    root = len(out)
    out += bytes([len(cap_hdr) << 1])
    for sid, lang in cap_hdr: out += struct.pack('<Ih', sid, lang)
    out += bytes([len(icon_hdr) << 1])
    for sid, side in icon_hdr: out += struct.pack('<Ih', sid, side)
    out += struct.pack('<iiii', 1, 0, 0, 0)            # capability: version 1, not embeddable, no new file, shown
    if dt_hdr:
        out += struct.pack('<i', 1)                     # EAifVersionAddsDataType
        out += bytes([len(dt_hdr) << 1])
        for sid, prio in dt_hdr: out += struct.pack('<Ih', sid, prio)
    struct.pack_into('<IIIII', out, 0, 0x10000037, 0x1000006A, uid,
                     uid_checksum(0x10000037, 0x1000006A, uid), root)
    open(path, 'wb').write(out)
    return len(out)


def preview(path, pics):
    """the pictures side by side, three times their size, as a PNG"""
    w = sum(p.size[0] for p in pics) + 10 * len(pics)
    h = max(p.size[1] for p in pics)
    prev = Image.new('RGB', (w, h), (200, 200, 200))
    x = 0
    for p in pics:
        prev.paste(p, (x, 0)); x += p.size[0] + 10
    prev.resize((w * 3, h * 3), Image.NEAREST).save(path)
