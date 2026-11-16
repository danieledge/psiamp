#!/bin/bash
# PsiAmp built, its test sounds made (test/mkaudio.py: speech from ffmpeg's
# flite and a tune synthesised there, nothing recorded), then a netBook test
# image with them in Z:\Pictures and a card for the emulator. The image is
# made by Coda's image builder (tools/mkimage.mts in a psion-netbook
# checkout beside this one, or $CODA), which takes PsiAmp from
# $PSIAMP_SIS (here: this checkout's dist/PsiAmp.sis):
#   test/emu/mkimg.sh OUTDIR [mkimage options...]   -> OUTDIR/OS.IMG, OUTDIR/card.img
# Then e.g.: python3 test/emu/emu.py OUTDIR/card.img OUT SCRIPT --exc
set -e
TOP=$(cd "$(dirname "$0")/../.." && pwd)
CODA=${CODA:-$TOP/../psion-netbook}
OUT=$(mkdir -p "$1" && cd "$1" && pwd); shift
(cd "$TOP" && PSION_WORK=${PSION_WORK:-$TOP/..} core/tools/docker/psibuild "bash build.sh" > /dev/null)
python3 "$TOP/test/mkaudio.py" "$OUT/audio" > /dev/null
P=$OUT/pictures; rm -rf "$P"; mkdir -p "$P"
cp "$OUT/audio/psiamp-test.mp3" "$OUT/audio/tune22.mp3" "$P/"
# a short WAV (the tones at 11,025 a second, 16-bit stereo), a playlist, and a second skin
ffmpeg -hide_banner -loglevel error -y -i "$OUT/audio/tones.wav" -ar 11025 -t 3 "$P/tones.wav"
printf '#EXTM3U\r\npsiamp-test.mp3\r\ntune22.mp3\r\ntones.wav\r\n' > "$P/test.m3u"
python3 "$TOP/test/mkskin2.py" "$P/Amber.wsz"
# PSIAMP_TEST_SKINS=DIR: other skins (.wsz) for this local test image only
# (other people's artwork: never in a committed file or a published shot)
[ -n "$PSIAMP_TEST_SKINS" ] && cp "$PSIAMP_TEST_SKINS"/*.wsz "$P/"
cd "$CODA" && PSIAMP_SIS=$TOP/dist/PsiAmp.sis node --experimental-strip-types tools/mkimage.mts --psi-on-card --smooth-text --font-choice --test-pictures="$P" --out "$OUT" "$@" 2>&1 | grep -E "PsiAmp|free inside|wrote|test pictures"
node --experimental-strip-types tools/mkcard.mts "$OUT/OS.IMG" "$OUT/card.img" 64 | tail -1
