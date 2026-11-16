#!/usr/bin/env python3
"""pkg/mkskin.py - PsiAmp's own skin, "Coda", drawn here from nothing.

It is laid out as a classic Winamp 2 skin (the same files, sizes and sprite
positions: main.bmp, cbuttons.bmp, numbers.bmp, text.bmp, ...), so that
PsiAmp draws its own skin and a user's .wsz the same way. Every picture is
ours: no Winamp bitmap, logo or name is copied. The letters (4 x 5) and the
LED digits are drawn below. There is no mark or logo (0.03: the coda sign
sat on the playlist's LIST button and the title bar's options button). Colours are the 6 x 6 x 6 cube's (multiples of 0x33),
which the netBook's panel shows without dithering (Coda's design notes).

    python3 pkg/mkskin.py OUT.wsz [PREVIEW.png]
"""
import io
import sys
import zipfile
from PIL import Image, ImageDraw

# the cube's colours
BLACK, WHITE = (0, 0, 0), (255, 255, 255)
G1, G2, G3, G4 = (51, 51, 51), (102, 102, 102), (153, 153, 153), (204, 204, 204)
LED, LED_DIM, LED_MID = (0, 255, 0), (0, 51, 0), (0, 153, 0)
TEAL, TEAL_HI, TEAL_LO = (0, 153, 153), (51, 204, 204), (0, 102, 102)
AMBER, RED = (255, 204, 0), (255, 51, 0)
PANEL = (0, 0, 0)
FACE = G1                 # the case
FACE_HI, FACE_LO = G2, BLACK

# ---- the letters: 4 x 5, in 5 x 6 cells (text.bmp) -------------------------
F = {
    'A': '.##.|#..#|####|#..#|#..#', 'B': '###.|#..#|###.|#..#|###.', 'C': '.###|#...|#...|#...|.###',
    'D': '###.|#..#|#..#|#..#|###.', 'E': '####|#...|###.|#...|####', 'F': '####|#...|###.|#...|#...',
    'G': '.###|#...|#.##|#..#|.###', 'H': '#..#|#..#|####|#..#|#..#', 'I': '###.|.#..|.#..|.#..|###.',
    'J': '..##|...#|...#|#..#|.##.', 'K': '#..#|#.#.|##..|#.#.|#..#', 'L': '#...|#...|#...|#...|####',
    'M': '#..#|####|####|#..#|#..#', 'N': '#..#|##.#|#.##|#..#|#..#', 'O': '.##.|#..#|#..#|#..#|.##.',
    'P': '###.|#..#|###.|#...|#...', 'Q': '.##.|#..#|#..#|#.#.|.#.#', 'R': '###.|#..#|###.|#.#.|#..#',
    'S': '.###|#...|.##.|...#|###.', 'T': '###.|.#..|.#..|.#..|.#..', 'U': '#..#|#..#|#..#|#..#|.##.',
    'V': '#..#|#..#|#..#|.##.|.##.', 'W': '#..#|#..#|####|####|#..#', 'X': '#..#|.##.|.##.|.##.|#..#',
    'Y': '#.#.|#.#.|.#..|.#..|.#..', 'Z': '####|..#.|.#..|#...|####',
    '0': '.##.|#.##|##.#|#..#|.##.', '1': '.#..|##..|.#..|.#..|###.', '2': '###.|...#|.##.|#...|####',
    '3': '###.|...#|.##.|...#|###.', '4': '#..#|#..#|####|...#|...#', '5': '####|#...|###.|...#|###.',
    '6': '.##.|#...|###.|#..#|.##.', '7': '####|...#|..#.|.#..|.#..', '8': '.##.|#..#|.##.|#..#|.##.',
    '9': '.##.|#..#|.###|...#|.##.',
    '"': '#.#.|#.#.|....|....|....', '@': '.##.|#.##|#.##|#...|.##.', ' ': '....|....|....|....|....',
    '…': '....|....|....|....|#.#.', '.': '....|....|....|....|.#..', ':': '....|.#..|....|.#..|....',
    '(': '..#.|.#..|.#..|.#..|..#.', ')': '.#..|..#.|..#.|..#.|.#..', '-': '....|....|###.|....|....',
    "'": '.#..|.#..|....|....|....', '!': '.#..|.#..|.#..|....|.#..', '_': '....|....|....|....|####',
    '+': '....|.#..|###.|.#..|....', '\\': '#...|#...|.#..|..#.|..#.', '/': '..#.|..#.|.#..|#...|#...',
    '[': '.##.|.#..|.#..|.#..|.##.', ']': '.##.|..#.|..#.|..#.|.##.', '^': '.#..|#.#.|....|....|....',
    '&': '.#..|#.#.|.#.#|#.#.|.#.#', '%': '#..#|..#.|.#..|#...|#..#', ',': '....|....|....|.#..|#...',
    '=': '....|###.|....|###.|....', '$': '.###|##..|.##.|..##|###.', '#': '.#.#|####|.#.#|####|.#.#',
    'Å': '.##.|.##.|#..#|####|#..#', 'Ö': '#..#|.##.|#..#|#..#|.##.', 'Ä': '#..#|.##.|#..#|####|#..#',
    '?': '###.|...#|.##.|....|.#..', '*': '....|#.#.|.#..|#.#.|....',
}
# text.bmp's rows, as every classic skin has them (31 cells a row)
ROWS = ['ABCDEFGHIJKLMNOPQRSTUVWXYZ"@   ', '0123456789….:()-\'!_+\\/[]^&%,=$#', 'ÅÖÄ?*' + ' ' * 26]


def glyph(d, x, y, ch, col):
    rows = F.get(ch.upper(), F[' ']).split('|')
    for j, r in enumerate(rows):
        for i, c in enumerate(r):
            if c == '#': d.point((x + i, y + j), fill=col)


def text(d, x, y, s, col, step=5):
    for k, ch in enumerate(s): glyph(d, x + k * step, y, ch, col)


def bevel(d, box, face, hi, lo, inset=False):
    x0, y0, x1, y1 = box
    d.rectangle(box, fill=face)
    a, b = (lo, hi) if inset else (hi, lo)
    d.line([(x0, y1), (x0, y0), (x1, y0)], fill=a)
    d.line([(x1, y0 + 1), (x1, y1), (x0 + 1, y1)], fill=b)


def new(w, h, col=FACE):
    im = Image.new('RGB', (w, h), col)
    return im, ImageDraw.Draw(im)


# ---- main.bmp: the case, 275 x 116 ------------------------------------------
def main_bmp():
    im, d = new(275, 116)
    # a faint texture of the cube's darkest grey, in rows
    for y in range(14, 116, 2): d.line([(0, y), (274, y)], fill=(51, 51, 51))
    for y in range(15, 116, 4): d.line([(0, y), (274, y)], fill=(0, 0, 0) if y % 8 == 3 else (51, 51, 51))
    d.rectangle([0, 0, 274, 115], outline=G2)
    d.line([(1, 115), (274, 115), (274, 1)], fill=BLACK)
    # the LED display: time, the play state, the visualiser
    bevel(d, (9, 20, 104, 62), PANEL, G2, BLACK, inset=True)
    d.rectangle([10, 21, 103, 61], fill=PANEL)
    # the time's colon
    for y in (31, 35): d.rectangle([73, y, 74, y + 1], fill=LED)
    # the song title's window, and the bitrate and rate's
    bevel(d, (108, 23, 266, 34), PANEL, G2, BLACK, inset=True)
    bevel(d, (108, 39, 128, 50), PANEL, G2, BLACK, inset=True)
    bevel(d, (153, 39, 167, 50), PANEL, G2, BLACK, inset=True)
    text(d, 131, 43, 'KBPS', G3)
    text(d, 170, 43, 'KHZ', G3)
    # under the volume and balance, and the position bar's well
    bevel(d, (14, 70, 265, 83), BLACK, BLACK, G2, inset=True)
    # the transport's ledge
    d.rectangle([12, 86, 160, 107], fill=BLACK)
    return im


# ---- titlebar.bmp: 344 x 87 ---------------------------------------------------
def titlebar_bmp():
    im, d = new(344, 87, BLACK)
    for y0, active in ((0, True), (15, False)):
        x0 = 27
        bar = TEAL_LO if active else G1
        line = TEAL_HI if active else G2
        d.rectangle([x0, y0, x0 + 274, y0 + 13], fill=bar)
        d.line([(x0, y0), (x0 + 274, y0)], fill=line)
        d.line([(x0, y0 + 13), (x0 + 274, y0 + 13)], fill=BLACK)
        # thin rules either side of the name, the gap for the buttons on the right
        for yy in (y0 + 4, y0 + 6, y0 + 8):
            d.line([(x0 + 20, yy), (x0 + 105, yy)], fill=line)
            d.line([(x0 + 170, yy), (x0 + 213, yy)], fill=line)
        name = 'PSIAMP'
        text(d, x0 + 122, y0 + 4, name, WHITE if active else G3, step=6)
    # the buttons: options, minimise, close; shade; each 9 x 9, pressed below
    def btn(x, y, kind, pressed):
        bevel(d, (x, y, x + 8, y + 8), G2 if not pressed else G1, G4, BLACK, inset=pressed)
        c = WHITE
        if kind == 'opt': d.line([(x + 2, y + 3), (x + 6, y + 3)], fill=c); d.line([(x + 3, y + 5), (x + 5, y + 5)], fill=c)
        if kind == 'min': d.line([(x + 2, y + 6), (x + 6, y + 6)], fill=c)
        if kind == 'close': d.line([(x + 2, y + 2), (x + 6, y + 6)], fill=c); d.line([(x + 6, y + 2), (x + 2, y + 6)], fill=c)
        if kind == 'shade': d.rectangle([x + 2, y + 2, x + 6, y + 3], outline=c)
    # the strip down the display's left edge (at 10, 22 in the main window): plain, with dots
    d.rectangle([304, 0, 311, 40], fill=PANEL)
    d.rectangle([304, 41, 311, 42], fill=G1)
    d.line([(304, 0), (304, 42)], fill=G1)
    for y in range(4, 40, 6): d.point((308, y), fill=LED_DIM)
    for p in (0, 1):
        btn(0, 9 * p, 'opt', p); btn(9, 9 * p, 'min', p); btn(18, 9 * p, 'close', p)
        btn(9 * p, 18, 'shade', p)
    return im


# ---- cbuttons.bmp: the transport, 136 x 36 ------------------------------------
def cbuttons_bmp():
    im, d = new(136, 36, BLACK)
    specs = [('prev', 0, 23, 18), ('play', 23, 23, 18), ('pause', 46, 23, 18), ('stop', 69, 23, 18),
             ('next', 92, 22, 18), ('eject', 114, 22, 16)]
    for kind, x, w, h in specs:
        for pressed in (0, 1):
            y = (h if kind == 'eject' else 18) * pressed
            bevel(d, (x, y, x + w - 1, y + h - 1), G3 if not pressed else G2, G4 if not pressed else G1, BLACK if not pressed else G4)
            d.rectangle([x + 1, y + 1, x + w - 2, y + 1], fill=WHITE if not pressed else G2)
            cx, cy = x + w // 2 + pressed, y + h // 2 + pressed
            sym = BLACK
            if kind == 'play':
                d.polygon([(cx - 3, cy - 4), (cx + 3, cy), (cx - 3, cy + 4)], fill=(0, 102, 0) if not pressed else LED_DIM)
            elif kind == 'pause':
                d.rectangle([cx - 4, cy - 4, cx - 2, cy + 4], fill=sym); d.rectangle([cx + 1, cy - 4, cx + 3, cy + 4], fill=sym)
            elif kind == 'stop':
                d.rectangle([cx - 4, cy - 4, cx + 3, cy + 3], fill=sym)
            elif kind == 'prev':
                d.rectangle([cx - 5, cy - 4, cx - 4, cy + 4], fill=sym)
                d.polygon([(cx + 3, cy - 4), (cx - 3, cy), (cx + 3, cy + 4)], fill=sym)
            elif kind == 'next':
                d.polygon([(cx - 4, cy - 4), (cx + 2, cy), (cx - 4, cy + 4)], fill=sym)
                d.rectangle([cx + 3, cy - 4, cx + 4, cy + 4], fill=sym)
            elif kind == 'eject':
                d.polygon([(cx - 5, cy + 1), (cx, cy - 5), (cx + 5, cy + 1)], fill=sym)
                d.rectangle([cx - 5, cy + 3, cx + 5, cy + 4], fill=sym)
    return im


# ---- numbers.bmp: LED digits, 9 x 13 each, then a blank (99 x 13); and
# nums_ex.bmp with the minus too (108 x 13) -----------------------------------
SEG = {'0': 'abcdef', '1': 'bc', '2': 'abged', '3': 'abgcd', '4': 'fgbc', '5': 'afgcd', '6': 'afgedc',
       '7': 'abc', '8': 'abcdefg', '9': 'abcdfg', '-': 'g', ' ': ''}


def led_digit(d, x, ch):
    # segments 7 wide, 13 high, 2 thick; unlit ones dim (as an LED's)
    segs = {'a': [(x + 2, 0), (x + 6, 1)], 'g': [(x + 2, 6), (x + 6, 6)], 'd': [(x + 2, 11), (x + 6, 12)],
            'f': [(x + 1, 1), (x + 2, 5)], 'e': [(x + 1, 7), (x + 2, 11)],
            'b': [(x + 6, 1), (x + 7, 5)], 'c': [(x + 6, 7), (x + 7, 11)]}
    on = SEG[ch]
    for s, (p0, p1) in segs.items():
        d.rectangle([p0, p1], fill=LED if s in on else LED_DIM)


def numbers_bmp(ex=False):
    im, d = new(108 if ex else 99, 13, PANEL)
    for i, ch in enumerate('0123456789'): led_digit(d, 9 * i, ch)
    led_digit(d, 90, ' ')
    if ex: led_digit(d, 99, '-')
    return im


# ---- text.bmp: 155 x 18 ------------------------------------------------------
def text_bmp():
    im, d = new(155, 18, PANEL)
    for r, row in enumerate(ROWS):
        for c, ch in enumerate(row):
            glyph(d, c * 5, r * 6, ch, LED)
    return im


# ---- playpaus.bmp: 42 x 9 ----------------------------------------------------
def playpaus_bmp():
    im, d = new(42, 9, PANEL)
    d.polygon([(2, 1), (7, 4), (2, 7)], fill=LED)                         # playing
    d.rectangle([11, 1, 12, 7], fill=AMBER); d.rectangle([14, 1, 15, 7], fill=AMBER)   # paused, at 9
    d.rectangle([20, 1, 25, 6], fill=RED)                                 # stopped, at 18
    d.point((37, 4), fill=LED)                                            # the "working" dots
    d.point((40, 4), fill=LED_DIM)
    return im


# ---- monoster.bmp: 56 x 24 ---------------------------------------------------
def monoster_bmp():
    im, d = new(56, 24, FACE)
    for y, lit in ((0, True), (12, False)):
        c = LED if lit else G2
        text(d, 0, y + 3, 'STEREO', c)
        text(d, 33, y + 3, 'MONO', c)
    return im


# ---- posbar.bmp: 307 x 10 ----------------------------------------------------
def posbar_bmp():
    im, d = new(307, 10, BLACK)
    d.rectangle([0, 0, 247, 9], fill=BLACK)
    d.line([(0, 4), (247, 4)], fill=G1); d.line([(0, 5), (247, 5)], fill=G1)
    for x0, face in ((248, G3), (278, G4)):
        bevel(d, (x0, 0, x0 + 28, 9), face, WHITE, G1)
        for k in (-4, 0, 4): d.line([(x0 + 14 + k, 2), (x0 + 14 + k, 7)], fill=G1)
    return im


# ---- volume.bmp and balance.bmp: 68 x 433 ------------------------------------
def slider_bmp(kind):
    im, d = new(68, 433, FACE)
    for i in range(28):
        y = 15 * i
        d.rectangle([0, y, 67, y + 12], fill=BLACK)
        if kind == 'volume':
            # a wedge that fills and goes from green to red with the level
            fill = int(65 * i / 27)
            col = LED_MID if i < 14 else AMBER if i < 22 else RED
            for x in range(1, 1 + fill):
                top = y + 10 - (x * 8) // 66
                d.line([(x, top), (x, y + 10)], fill=col)
            for x in range(1 + fill, 67):
                top = y + 10 - (x * 8) // 66
                d.point((x, top), fill=G1)
        else:
            # the balance's (only its middle frame is used: the netBook is mono)
            d.line([(9, y + 6), (46, y + 6)], fill=G2)
            d.line([(27, y + 2), (27, y + 10)], fill=G2)
    for x0, face in ((15, G3), (0, G4)):
        bevel(d, (x0, 422, x0 + 13, 432), face, WHITE, G1)
        d.line([(x0 + 6, 424), (x0 + 6, 430)], fill=G1)
    return im


# ---- shufrep.bmp: 92 x 85 ----------------------------------------------------
def shufrep_bmp():
    im, d = new(92, 85, BLACK)
    def b(x, y, w, h, label, on, pressed):
        bevel(d, (x, y, x + w - 1, y + h - 1), G2 if not pressed else G1, G3, BLACK, inset=pressed)
        d.rectangle([x + 3, y + h // 2 - 1, x + 5, y + h // 2 + 1], fill=LED if on else LED_DIM)
        text(d, x + 9, y + (h - 5) // 2, label, WHITE if on else G4)
    for k, (on, pr) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
        b(0, 15 * k, 28, 15, 'REP', on, pr)
        b(28, 15 * k, 47, 15, 'SHUFFL', on, pr)
    for on in (0, 1):
        for pr in (0, 1):
            b(46 * pr, 61 + 12 * on, 23, 12, 'EQ', on, pr)
            b(23 + 46 * pr, 61 + 12 * on, 23, 12, 'PL', on, pr)
    return im


# ---- pledit.bmp: the playlist's frame, 280 x 186 ------------------------------
def pledit_bmp():
    im, d = new(280, 186, FACE)
    for y0, active in ((0, True), (21, False)):
        bar = TEAL_LO if active else G1
        line = TEAL_HI if active else G2
        for x0, w in ((0, 25), (26, 100), (127, 25), (153, 25)):
            d.rectangle([x0, y0, x0 + w - 1, y0 + 19], fill=FACE)
            d.rectangle([x0, y0 + 3, x0 + w - 1, y0 + 15], fill=bar)
            d.line([(x0, y0 + 3), (x0 + w - 1, y0 + 3)], fill=line)
        text(d, 26 + 22, y0 + 7, 'PLAYLIST', WHITE if active else G3, step=7)
        # the close button in the top right corner (at its width - 11)
        x = 153 + 25 - 11
        bevel(d, (x, y0 + 5, x + 8, y0 + 13), G2, G4, BLACK)
        d.line([(x + 2, y0 + 7), (x + 6, y0 + 11)], fill=WHITE); d.line([(x + 6, y0 + 7), (x + 2, y0 + 11)], fill=WHITE)
    # the sides: 12 on the left, 20 on the right with the scroll bar's groove
    d.rectangle([0, 42, 11, 70], fill=FACE); d.line([(11, 42), (11, 70)], fill=BLACK)
    d.rectangle([31, 42, 50, 70], fill=FACE); d.line([(31, 42), (31, 70)], fill=G2)
    d.rectangle([36, 42, 44, 70], fill=BLACK)
    # the scroll bar's handle, and pressed
    for x0, face in ((52, G3), (61, G4)):
        bevel(d, (x0, 53, x0 + 7, 70), face, WHITE, G1)
    # the bottom: the left corner with the buttons' labels, the tile, the right corner
    d.rectangle([0, 72, 124, 109], fill=FACE)
    for i, lab in enumerate(('ADD', 'REM', 'SEL', 'MISC')):
        x = 14 + 29 * i
        bevel(d, (x, 72 + 8, x + 22, 72 + 25), G2, G3, BLACK)
        text(d, x + 3 + (1 if len(lab) == 3 else -1), 72 + 15, lab, WHITE)
    d.rectangle([179, 0, 203, 37], fill=FACE)
    d.rectangle([126, 72, 275, 109], fill=FACE)
    bevel(d, (126 + 150 - 44, 72 + 8, 126 + 150 - 22, 72 + 25), G2, G3, BLACK)
    text(d, 126 + 150 - 41, 72 + 15, 'LIST', WHITE)
    bevel(d, (126 + 7, 72 + 8, 126 + 92, 72 + 18), PANEL, BLACK, G2, inset=True)   # the running time
    d.rectangle([205, 0, 279, 37], fill=FACE)
    return im


# ---- EPOC: the second skin, in the netBook's own style ------------------------
# Drawn in key colours (each a role, not a colour: none is a 6 x 6 x 6 cube
# colour, so none can be mistaken for one): PsiAmp swaps each for one of
# EIKON's logical colours as it loads the skin (paskin.cpp, KPaEpocKeys), so
# the skin takes the colours of the theme in use (Coda, Classic, Slate,
# Graphite, Teal, Dark...). The LCD is an LCD's colours in every theme.
K = {
    'face':      (1, 2, 10),     # EEikColorDialogBackground: the body
    'light':     (1, 2, 20),     # the face lightened: bevels' light edges
    'shadow':    (1, 2, 30),     # the face darkened: bevels' dark edges
    'edge':      (1, 2, 40),     # EEikColorButtonText: buttons' outer edges
    'title':     (1, 2, 50),     # EEikColorDialogTitle
    'titletext': (1, 2, 60),     # EEikColorDialogTitleText
    'button':    (1, 2, 70),     # EEikColorButtonFaceClear
    'glyph':     (1, 2, 80),     # EEikColorButtonText
    'pressed':   (1, 2, 90),     # EEikColorButtonFaceSetPressed
    'text':      (1, 2, 100),    # EEikColorDialogText: labels on the body
    'hilite':    (1, 2, 110),    # EEikColorControlHighlightBackground: what is on
    'well':      (1, 2, 120),    # EEikColorControlBackground: wells and the list
}
LCD, LCD_ON, LCD_OFF = (153, 204, 153), (0, 51, 0), (102, 153, 102)   # the LCD, lit and unlit segments


def ebevel(d, box, face, inset=False, edge=True):
    """an EIKON button's bevel: a dark outer edge, light top-left, shadow bottom-right"""
    x0, y0, x1, y1 = box
    d.rectangle(box, fill=K[face])
    if edge:
        d.rectangle(box, outline=K['edge'])
        x0, y0, x1, y1 = x0 + 1, y0 + 1, x1 - 1, y1 - 1
    a, b = (K['shadow'], K['light']) if inset else (K['light'], K['shadow'])
    d.line([(x0, y1), (x0, y0), (x1, y0)], fill=a)
    d.line([(x1, y0 + 1), (x1, y1), (x0 + 1, y1)], fill=b)


def lcd_panel(d, box):
    x0, y0, x1, y1 = box
    d.rectangle(box, fill=LCD)
    d.line([(x0, y1), (x0, y0), (x1, y0)], fill=K['shadow'])
    d.line([(x1, y0 + 1), (x1, y1), (x0 + 1, y1)], fill=K['light'])


def epoc_main():
    im, d = new(275, 116, K['face'])
    d.rectangle([0, 0, 274, 115], outline=K['light'])
    d.line([(1, 115), (274, 115), (274, 1)], fill=K['shadow'])
    lcd_panel(d, (9, 20, 104, 62))
    for y in (31, 35): d.rectangle([73, y, 74, y + 1], fill=LCD_ON)
    lcd_panel(d, (108, 23, 266, 34))
    lcd_panel(d, (108, 39, 128, 50))
    lcd_panel(d, (153, 39, 167, 50))
    text(d, 131, 43, 'KBPS', K['text'])
    text(d, 170, 43, 'KHZ', K['text'])
    ebevel(d, (14, 70, 265, 83), 'well', inset=True, edge=False)
    return im


def epoc_titlebar():
    im, d = new(344, 87, K['face'])
    for y0, active in ((0, True), (15, False)):
        x0 = 27
        d.rectangle([x0, y0, x0 + 274, y0 + 13], fill=K['title'] if active else K['shadow'])
        c = K['titletext'] if active else K['face']
        text(d, x0 + 120, y0 + 4, 'PSIAMP', c, step=6)
    def btn(x, y, kind, pressed):
        ebevel(d, (x, y, x + 8, y + 8), 'button', inset=pressed)
        c = K['glyph']
        if kind == 'opt': d.line([(x + 2, y + 4), (x + 6, y + 4)], fill=c)
        if kind == 'min': d.line([(x + 2, y + 6), (x + 6, y + 6)], fill=c)
        if kind == 'close': d.line([(x + 2, y + 2), (x + 6, y + 6)], fill=c); d.line([(x + 6, y + 2), (x + 2, y + 6)], fill=c)
        if kind == 'shade': d.rectangle([x + 2, y + 2, x + 6, y + 3], outline=c)
    for p in (0, 1):
        btn(0, 9 * p, 'opt', p); btn(9, 9 * p, 'min', p); btn(18, 9 * p, 'close', p)
        btn(9 * p, 18, 'shade', p)
    # the strip down the display's left edge: the LCD's own colour
    d.rectangle([304, 0, 311, 40], fill=LCD)
    d.rectangle([304, 41, 311, 42], fill=K['face'])
    return im


def epoc_cbuttons():
    im, d = new(136, 36, K['face'])
    specs = [('prev', 0, 23, 18), ('play', 23, 23, 18), ('pause', 46, 23, 18), ('stop', 69, 23, 18),
             ('next', 92, 22, 18), ('eject', 114, 22, 16)]
    for kind, x, w, h in specs:
        for pressed in (0, 1):
            y = (h if kind == 'eject' else 18) * pressed
            ebevel(d, (x, y, x + w - 1, y + h - 1), 'pressed' if pressed else 'button', inset=pressed)
            cx, cy = x + w // 2 + pressed, y + h // 2 + pressed
            g = K['glyph']
            if kind == 'play': d.polygon([(cx - 3, cy - 4), (cx + 3, cy), (cx - 3, cy + 4)], fill=g)
            elif kind == 'pause':
                d.rectangle([cx - 4, cy - 4, cx - 2, cy + 4], fill=g); d.rectangle([cx + 1, cy - 4, cx + 3, cy + 4], fill=g)
            elif kind == 'stop': d.rectangle([cx - 4, cy - 4, cx + 3, cy + 3], fill=g)
            elif kind == 'prev':
                d.rectangle([cx - 5, cy - 4, cx - 4, cy + 4], fill=g)
                d.polygon([(cx + 3, cy - 4), (cx - 3, cy), (cx + 3, cy + 4)], fill=g)
            elif kind == 'next':
                d.polygon([(cx - 4, cy - 4), (cx + 2, cy), (cx - 4, cy + 4)], fill=g)
                d.rectangle([cx + 3, cy - 4, cx + 4, cy + 4], fill=g)
            elif kind == 'eject':
                d.polygon([(cx - 5, cy + 1), (cx, cy - 5), (cx + 5, cy + 1)], fill=g)
                d.rectangle([cx - 5, cy + 3, cx + 5, cy + 4], fill=g)
    return im


def epoc_numbers(ex=False):
    im, d = new(108 if ex else 99, 13, LCD)
    def dig(x, ch):
        segs = {'a': [(x + 2, 0), (x + 6, 1)], 'g': [(x + 2, 6), (x + 6, 6)], 'd': [(x + 2, 11), (x + 6, 12)],
                'f': [(x + 1, 1), (x + 2, 5)], 'e': [(x + 1, 7), (x + 2, 11)],
                'b': [(x + 6, 1), (x + 7, 5)], 'c': [(x + 6, 7), (x + 7, 11)]}
        for sname, (p0, p1) in segs.items():
            d.rectangle([p0, p1], fill=LCD_ON if sname in SEG[ch] else LCD_OFF)
    for i, ch in enumerate('0123456789'): dig(9 * i, ch)
    dig(90, ' ')
    if ex: dig(99, '-')
    return im


def epoc_text():
    im, d = new(155, 18, LCD)
    for r, row in enumerate(ROWS):
        for c, ch in enumerate(row): glyph(d, c * 5, r * 6, ch, LCD_ON)
    return im


def epoc_playpaus():
    im, d = new(42, 9, LCD)
    d.polygon([(2, 1), (7, 4), (2, 7)], fill=LCD_ON)
    d.rectangle([11, 1, 12, 7], fill=LCD_ON); d.rectangle([14, 1, 15, 7], fill=LCD_ON)
    d.rectangle([20, 1, 25, 6], fill=LCD_ON)
    d.point((37, 4), fill=LCD_ON)
    return im


def epoc_monoster():
    im, d = new(56, 24, K['face'])
    for y, lit in ((0, True), (12, False)):
        c = K['text'] if lit else K['shadow']
        text(d, 0, y + 3, 'STEREO', c)
        text(d, 33, y + 3, 'MONO', c)
    return im


def epoc_posbar():
    im, d = new(307, 10, K['well'])
    d.line([(0, 4), (247, 4)], fill=K['shadow']); d.line([(0, 5), (247, 5)], fill=K['light'])
    for x0, pressed in ((248, 0), (278, 1)):
        ebevel(d, (x0, 0, x0 + 28, 9), 'pressed' if pressed else 'button', inset=pressed)
        for k in (-3, 0, 3): d.line([(x0 + 14 + k, 3), (x0 + 14 + k, 6)], fill=K['shadow'])
    return im


def epoc_slider(kind):
    im, d = new(68, 433, K['face'])
    for i in range(28):
        y = 15 * i
        ebevel(d, (0, y + 3, 67, y + 9), 'well', inset=True, edge=False)
        if kind == 'volume':
            fill = int(64 * i / 27)
            if fill: d.rectangle([2, y + 5, 2 + fill, y + 7], fill=K['hilite'])
        else:
            d.line([(33, y + 4), (33, y + 8)], fill=K['shadow'])
    for x0, pressed in ((15, 0), (0, 1)):
        ebevel(d, (x0, 422, x0 + 13, 432), 'pressed' if pressed else 'button', inset=pressed)
        d.line([(x0 + 6, 425), (x0 + 6, 429)], fill=K['shadow'])
    return im


def epoc_shufrep():
    im, d = new(92, 85, K['face'])
    def b(x, y, w, h, label, on, pressed):
        ebevel(d, (x, y, x + w - 1, y + h - 1), 'pressed' if pressed else 'button', inset=pressed)
        d.rectangle([x + 3, y + h // 2 - 1, x + 5, y + h // 2 + 1], fill=K['hilite'] if on else K['shadow'])
        text(d, x + 9, y + (h - 5) // 2, label, K['glyph'])
    for k, (on, pr) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
        b(0, 15 * k, 28, 15, 'REP', on, pr)
        b(28, 15 * k, 47, 15, 'SHUFFL', on, pr)
    for on in (0, 1):
        for pr in (0, 1):
            b(46 * pr, 61 + 12 * on, 23, 12, 'EQ', on, pr)
            b(23 + 46 * pr, 61 + 12 * on, 23, 12, 'PL', on, pr)
    return im


def epoc_pledit():
    im, d = new(280, 186, K['face'])
    for y0, active in ((0, True), (21, False)):
        bar = K['title'] if active else K['shadow']
        c = K['titletext'] if active else K['face']
        for x0, w in ((0, 25), (26, 100), (127, 25), (153, 25)):
            d.rectangle([x0, y0, x0 + w - 1, y0 + 19], fill=K['face'])
            d.rectangle([x0, y0 + 3, x0 + w - 1, y0 + 16], fill=bar)
        text(d, 26 + 26, y0 + 7, 'PLAYLIST', c, step=6)
        x = 153 + 25 - 11
        ebevel(d, (x, y0 + 5, x + 8, y0 + 13), 'button')
        d.line([(x + 2, y0 + 7), (x + 6, y0 + 11)], fill=K['glyph']); d.line([(x + 6, y0 + 7), (x + 2, y0 + 11)], fill=K['glyph'])
    d.rectangle([0, 42, 11, 70], fill=K['face']); d.line([(11, 42), (11, 70)], fill=K['shadow'])
    d.rectangle([31, 42, 50, 70], fill=K['face']); d.line([(31, 42), (31, 70)], fill=K['light'])
    ebevel(d, (36, 42, 44, 70), 'well', inset=True, edge=False)
    for x0, pressed in ((52, 0), (61, 1)):
        ebevel(d, (x0, 53, x0 + 7, 70), 'pressed' if pressed else 'button', inset=pressed)
    d.rectangle([0, 72, 124, 109], fill=K['face'])
    for i, lab in enumerate(('ADD', 'REM', 'SEL', 'MISC')):
        x = 14 + 29 * i
        ebevel(d, (x, 72 + 8, x + 22, 72 + 25), 'button')
        text(d, x + 3 + (1 if len(lab) == 3 else -1), 72 + 15, lab, K['glyph'])
    d.rectangle([179, 0, 203, 37], fill=K['face'])
    d.rectangle([126, 72, 275, 109], fill=K['face'])
    ebevel(d, (126 + 150 - 44, 72 + 8, 126 + 150 - 22, 72 + 25), 'button')
    text(d, 126 + 150 - 41, 72 + 15, 'LIST', K['glyph'])
    lcd_panel(d, (126 + 7, 72 + 8, 126 + 92, 72 + 18))
    d.rectangle([205, 0, 279, 37], fill=K['face'])
    return im


EPOC_VISCOLOR = [LCD, LCD] + [LCD_ON] * 4 + [(0, 102, 0)] * 6 + [(51, 102, 51)] * 6 + [LCD_ON] * 5 + [(0, 0, 0)]
# (the list's colours are EIKON's own at run time; these are for other players)
EPOC_PLEDIT = """[Text]
Normal=#000000
Current=#000080
NormalBG=#FFFFFF
SelectedBG=#000080
Font=Arial
"""


def epoc_skin():
    return {
        'main.bmp': epoc_main(), 'titlebar.bmp': epoc_titlebar(), 'cbuttons.bmp': epoc_cbuttons(),
        'numbers.bmp': epoc_numbers(), 'nums_ex.bmp': epoc_numbers(True), 'text.bmp': epoc_text(),
        'playpaus.bmp': epoc_playpaus(), 'monoster.bmp': epoc_monoster(), 'posbar.bmp': epoc_posbar(),
        'volume.bmp': epoc_slider('volume'), 'balance.bmp': epoc_slider('balance'),
        'shufrep.bmp': epoc_shufrep(), 'pledit.bmp': epoc_pledit(),
    }


def themed(files, colours):
    """the EPOC skin with its key colours replaced (for the preview)"""
    out = {}
    for n, im in files.items():
        im = im.copy(); px = im.load()
        for y in range(im.size[1]):
            for x in range(im.size[0]):
                for role, key in K.items():
                    if px[x, y] == key: px[x, y] = colours[role]; break
        out[n] = im
    return out


VISCOLOR = [BLACK, G1] + \
    [RED, RED, (255, 102, 0), (255, 102, 0), AMBER, AMBER, (204, 255, 0), (204, 255, 0),
     (153, 255, 0), (102, 255, 0), (51, 255, 0), (0, 255, 0), (0, 204, 0), (0, 204, 0), (0, 153, 0), (0, 153, 0)] + \
    [(0, 255, 0), (0, 204, 0), (0, 153, 0), (0, 102, 0), (0, 51, 0), WHITE]

PLEDIT = """[Text]
Normal=#00FF00
Current=#FFFFFF
NormalBG=#000000
SelectedBG=#006666
Font=Arial
"""


def bmp_bytes(im):
    b = io.BytesIO()
    im.save(b, 'BMP')
    return b.getvalue()


def skin():
    files = {
        'main.bmp': main_bmp(), 'titlebar.bmp': titlebar_bmp(), 'cbuttons.bmp': cbuttons_bmp(),
        'numbers.bmp': numbers_bmp(), 'nums_ex.bmp': numbers_bmp(True), 'text.bmp': text_bmp(),
        'playpaus.bmp': playpaus_bmp(), 'monoster.bmp': monoster_bmp(), 'posbar.bmp': posbar_bmp(),
        'volume.bmp': slider_bmp('volume'), 'balance.bmp': slider_bmp('balance'),
        'shufrep.bmp': shufrep_bmp(), 'pledit.bmp': pledit_bmp(),
    }
    return files


def preview(files, path):
    """the main window and the playlist as PsiAmp composes them (roughly), 2x"""
    m = files['main.bmp'].copy()
    m.paste(files['titlebar.bmp'].crop((27, 0, 302, 14)), (0, 0))
    cb = files['cbuttons.bmp']
    for (sx, w, h), x in zip(((0, 23, 18), (23, 23, 18), (46, 23, 18), (69, 23, 18), (92, 22, 18), (114, 22, 16)),
                             (16, 39, 62, 85, 108, 136)):
        m.paste(cb.crop((sx, 0, sx + w, h)), (x, 88 if x != 136 else 89))
    nb = files['numbers.bmp']
    for x, dg in zip((48, 60, 78, 90), (0, 1, 2, 3)): m.paste(nb.crop((9 * dg, 0, 9 * dg + 9, 13)), (x, 26))
    m.paste(files['posbar.bmp'].crop((0, 0, 248, 10)), (16, 72))
    m.paste(files['posbar.bmp'].crop((248, 0, 277, 10)), (60, 72))
    m.paste(files['volume.bmp'].crop((0, 15 * 20, 68, 15 * 20 + 13)), (107, 57))
    m.paste(files['volume.bmp'].crop((15, 422, 29, 433)), (150, 58))
    m.paste(files['balance.bmp'].crop((9, 0, 47, 13)), (177, 57))
    sr = files['shufrep.bmp']
    m.paste(sr.crop((28, 30, 75, 45)), (164, 89)); m.paste(sr.crop((0, 0, 28, 15)), (210, 89))
    m.paste(sr.crop((0, 61, 23, 73)), (219, 58)); m.paste(sr.crop((23, 73, 46, 85)), (242, 58))
    m.paste(files['monoster.bmp'].crop((0, 0, 29, 12)), (239, 41)); m.paste(files['monoster.bmp'].crop((29, 12, 56, 24)), (212, 41))
    m.paste(files['playpaus.bmp'].crop((0, 0, 9, 9)), (26, 28))
    tb = files['text.bmp']
    s = 'PSIAMP - CODA TEST TUNE'
    for i, ch in enumerate(s):
        for r, row in enumerate(ROWS):
            if ch in row:
                c = row.index(ch); m.paste(tb.crop((c * 5, r * 6, c * 5 + 5, r * 6 + 6)), (111 + 5 * i, 27)); break
    out = Image.new('RGB', (550, 232 + 20 + 164), (102, 102, 102))
    out.paste(m.resize((550, 232), Image.NEAREST), (0, 0))
    out.paste(files['pledit.bmp'].resize((560, 372), Image.NEAREST).crop((0, 0, 550, 164)), (0, 252))
    out.save(path)


def write(path, files, vis, pledit, readme):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, im in files.items():
            z.writestr(name, bmp_bytes(im))
        z.writestr('viscolor.txt', ''.join(f'{r},{g},{b}\r\n' for r, g, b in vis))
        z.writestr('pledit.txt', pledit.replace('\n', '\r\n'))
        z.writestr('readme.txt', readme)


if __name__ == '__main__':
    if sys.argv[1] == '--epoc':
        # python3 pkg/mkskin.py --epoc OUT.wsz [PREVIEW.png]: EPOC, in key colours
        files = epoc_skin()
        write(sys.argv[2], files, EPOC_VISCOLOR, EPOC_PLEDIT,
              'EPOC, PsiAmp\'s second skin: the netBook\'s own look, in key colours PsiAmp replaces with the theme\'s.\r\n')
        if len(sys.argv) > 3:
            classic = {'face': (204, 204, 204), 'light': (255, 255, 255), 'shadow': (153, 153, 153), 'edge': (0, 0, 0),
                       'title': (0, 0, 136), 'titletext': (255, 255, 255), 'button': (204, 204, 204), 'glyph': (0, 0, 0),
                       'pressed': (153, 153, 153), 'text': (0, 0, 0), 'hilite': (0, 0, 136), 'well': (255, 255, 255)}
            preview(themed(files, classic), sys.argv[3])
        import os
        print(sys.argv[2], os.path.getsize(sys.argv[2]), 'bytes')
        sys.exit(0)
    files = skin()
    write(sys.argv[1], files, VISCOLOR, PLEDIT, 'Coda, PsiAmp\'s own skin: drawn for PsiAmp, in the classic skin layout.\r\n')
    if len(sys.argv) > 2:
        preview(files, sys.argv[2])
    import os
    print(sys.argv[1], os.path.getsize(sys.argv[1]), 'bytes')
