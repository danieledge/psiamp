# Notices and credits

PsiAmp is two kinds of code under two licences.

## PsiAmp's own code: MIT

Copyright (c) 2026 Daniel Edge. MIT licence, in [`LICENSE`](LICENSE).

That is everything outside `helix/` and `core/`: the program (`app/`), the
sound path and skin reader (`engine/`), the recogniser (`rec/`), the
packaging and the generators of the skins, icon and sample (`pkg/`), and the
tests (`test/`).

## The Helix MP3 decoder: RPSL 1.0 (or RCSL)

`helix/` is RealNetworks' Helix fixed-point MP3 decoder, from
[libhelix-mp3](https://github.com/ultraembedded/libhelix-mp3) (commit
`0a0e0673`, 2019), a copy of the Helix DNA decoder.

    Copyright (c) 1995-2004 RealNetworks, Inc. All Rights Reserved.

It is under the RealNetworks Public Source License 1.0 (`helix/RPSL.txt`), or,
for those who hold one, the RealNetworks Community Source License
(`helix/RCSL.txt`); `helix/LICENSE.txt` says which applies. These files stay
under those licences, not MIT.

PsiAmp's changes to it are listed in
[`helix/PSIAMP-CHANGES.txt`](helix/PSIAMP-CHANGES.txt), and each changed
file is marked at its top (RPSL 2.1(c)): an SA-1100 block and a host block in
`real/assembly.h`, a band limit (`MP3SetBandLimit`) in `mp3dec.c`,
`pub/mp3dec.h` and `pub/mp3common.h`, and writable tables made `const` in
`real/dqchan.c` and `real/imdct.c`. The changes are under the RPSL, as the
files they are in.

The object code notice (RPSL Exhibit A), shown in PsiAmp's About box:

    Helix DNA Client technology included. Copyright (c) RealNetworks, Inc.,
    1995-2002. All rights reserved.

The About box also says where the source is (this repository), and
`PsiAmp.sis` installs `Licences.txt` beside the program: this repository's
`LICENSE` and the RPSL's text.

PsiAmp as a whole is distributed under the RPSL for the Helix code and MIT
(one of the RPSL's Exhibit B licences) for the rest, with all of its source
here.

## zlib

The skin reader inflates `.wsz` files with [zlib](https://zlib.net/)'s
inflate, from [psion-core](https://github.com/danieledge/psion-core) (the
`core/` submodule, `ssh/zlib/`), by Jean-loup Gailly and Mark Adler, under
the zlib licence.

## Our own material

- The skins Coda and EPOC (`pkg/mkskin.py`), the icon (`pkg/mkicon.py`) and
  the sample tune "Coda" (`pkg/mksample.py`, encoded with LAME, which puts no
  licence on its output) are drawn and composed in code, here. They are MIT,
  as the code that makes them.
- The test sounds (`test/mkaudio.py`) and the Amber test skin
  (`test/mkskin2.py`) are made in code too.
- PsiAmp reads skins in the classic Winamp 2 skin file layout, an
  interoperable format. No Winamp or Nullsoft bitmap, logo or name is used,
  and this repository holds no skin made by anyone else. Winamp is a
  trademark of its owners; it is named only to say which skins PsiAmp reads.
- `pkg/epoc256.py` is EPOC's 256-colour palette (`TRgb::Color256`): the
  colour numbers the system uses, as data, for the colour icon.

## Not in this repository

- The EPOC R5 C++ SDK (Psion's) that PsiAmp is built with: see
  psion-core's [docs/BUILDING.md](https://github.com/danieledge/psion-core/blob/main/docs/BUILDING.md).
- MP3's patents have expired (2017).
