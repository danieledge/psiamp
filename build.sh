#!/bin/bash
# Builds PsiAmp, the MP3 player for the Psion netBook, and packages
# dist/PsiAmp.sis:
#   psiamphx.lib   the C: Helix's MP3 decoder (helix/), the sound path and the
#                  skin reader (engine/), zlib's inflate (psion-core): built for
#                  the SA-1100 (ARMv4: smull and smlal; no Thumb, so no BX),
#                  put in the SDK's release folder for the program's link
#   PsiAmp.app     the player (app/), with its .rsc
#   PsiAmp.mdl     the recogniser: MP3, WAV and M3U files open in PsiAmp (rec/)
#   Coda.wsz and Epoc.wsz, its own skins (pkg/mkskin.py), PsiAmp.aif
#   (pkg/mkicon.py), Coda.mp3, the sample (pkg/Coda.mp3, made by
#   pkg/mksample.py), Licences.txt (LICENSE and Helix's RPSL), then
#   PsiAmp.sis
#
#   core/tools/docker/psibuild "bash build.sh"        everything: dist/PsiAmp.sis
#   core/tools/docker/psibuild "bash build.sh sis"    only the .sis, from dist/
#   python3 pkg/mksample.py pkg/Coda.mp3              the sample made again (on a
#                                                     PC with numpy and ffmpeg)
#
# The shared code (zlib, the BX check, the .aif writer) is psion-core, this
# checkout's core/ submodule (PSION_CORE points elsewhere).
set -e
: "${PSION_SDK:?set PSION_SDK to your psion_cpp_sdk_linux directory}"
HERE=$(cd "$(dirname "$0")" && pwd)
CORE=${PSION_CORE:-$HERE/core}
[ -f "$CORE/ssh/zlib/inflate.c" ] || { echo "psion-core is not here: git submodule update --init (or set PSION_CORE)"; exit 1; }
export EPOCROOT=$PSION_SDK/epoc_cpp_sdk
REL=$EPOCROOT/epoc32/release/marm/rel
mkdir -p "$HERE/build" "$HERE/dist"
export PATH=/usr/bin:/bin:$PSION_SDK/gcc-3.0-psion-98r2-9/bin:$EPOCROOT/epoc32/tools
export TMP=$EPOCROOT/tmp WINEDEBUG=-all
mkdir -p "$TMP" "$EPOCROOT/ptproj"
link() {
	if [ -e "$EPOCROOT/ptproj/$1" ] && [ ! -L "$EPOCROOT/ptproj/$1" ]; then
		echo "$EPOCROOT/ptproj/$1 exists and is not a link - move it away"; exit 1
	fi
	ln -sfn "$2" "$EPOCROOT/ptproj/$1"
}
build() {   # project folder, mmp name, built file, log name
	cd "$EPOCROOT/ptproj/$1"
	makmake "$2" marm > "$HERE/build/$4-makmake.log" 2>&1
	make -f "$2.marm" rel > "$HERE/build/$4.log" 2>&1 \
		|| { grep -A3 "\.c:[0-9]*:\|\.cpp:[0-9]*:\|rror\|undefined\|No rule" "$HERE/build/$4.log" | grep -v "^--" | head -60; exit 1; }
	# the netBook's SA-1100 has no BX (core/tools/v4bx.py)
	python3 "$CORE/tools/v4bx.py" --check "$REL/$3"
}
sis() {
	cd "$HERE/pkg"
	FILES="PsiAmp.app PsiAmp.rsc PsiAmp.aif PsiAmp.mdl Coda.wsz Epoc.wsz Licences.txt"
	for f in $FILES; do cp "$HERE/dist/$f" .; done
	# (pkg/Coda.mp3 is the sample itself, beside psiamp.pkg)
	# (the SDK's makesis.exe: the Linux makesis on the PATH writes a later format)
	WINEDEBUG=-all wine "$EPOCROOT/epoc32/tools/makesis.exe" psiamp.pkg psiamp.sis > "$HERE/build/sis.log" 2>&1 || { cat "$HERE/build/sis.log"; exit 1; }
	mv psiamp.sis "$HERE/dist/PsiAmp.sis"
	rm -f $FILES
	ls -la "$HERE/dist/PsiAmp.sis"
}

if [ "$1" = sis ]; then sis; exit 0; fi

echo "== psiamphx.lib"
L=$HERE/build/lib; rm -rf "$L"; mkdir -p "$L"; : > "$HERE/build/lib.log"
CFLAGS="-c -nostdinc -I$HERE/engine/libc -I$HERE/helix/pub -I$HERE/helix/real -I$HERE/engine -I$CORE/ssh/zlib \
 -DPSIAMP_ARM -DNO_GZIP -DZ_SOLO -march=armv4 -mno-thumb-interwork -mapcs-32 -msoft-float -O2 -fomit-frame-pointer"
for f in "$HERE"/helix/real/*.c "$HERE"/helix/mp3dec.c "$HERE"/helix/mp3tabs.c "$HERE"/engine/pacore.c "$HERE"/engine/paskinc.c; do
	arm-pe-gcc $CFLAGS -o "$L/$(basename "$f" .c).o" "$f" 2>> "$HERE/build/lib.log" || { tail -20 "$HERE/build/lib.log"; exit 1; }
done
for f in inflate inftrees inffast zutil adler32 crc32; do
	arm-pe-gcc $CFLAGS -o "$L/z_$f.o" "$CORE/ssh/zlib/$f.c" 2>> "$HERE/build/lib.log" || { tail -20 "$HERE/build/lib.log"; exit 1; }
done
# nothing writable in a program DLL (petran refuses .data): only constant tables
for o in "$L"/*.o; do
	arm-pe-objdump -h "$o" | awk -v o="$(basename "$o")" '$2==".data" && $3!="00000000" {print "writable data in " o ": " $3; bad=1} END {exit bad}'
done
rm -f "$REL/psiamphx.lib"
arm-pe-ar rcs "$REL/psiamphx.lib" "$L"/*.o
python3 "$CORE/tools/v4bx.py" --check "$REL/psiamphx.lib"

echo "== PsiAmp.app"
D=$HERE/build/app; rm -rf "$D"; mkdir -p "$D"
cp "$HERE"/app/* "$HERE"/engine/pacore.h "$HERE"/engine/paskinc.h "$HERE"/helix/pub/mp3dec.h "$HERE"/helix/pub/mp3common.h "$D/"
link psiamp "$D"
build psiamp psiamp psiamp.app app
cp "$REL/psiamp.app" "$HERE/dist/PsiAmp.app"
cp "$REL/psiamp.rsc" "$HERE/dist/PsiAmp.rsc"

echo "== PsiAmp.mdl"
R=$HERE/build/rec; rm -rf "$R"; mkdir -p "$R"
cp "$HERE"/rec/* "$R/"
link parec "$R"
build parec parec parec.mdl rec
cp "$REL/parec.mdl" "$HERE/dist/PsiAmp.mdl"

echo "== skins, icon, licences"
cd "$HERE"
python3 pkg/mkskin.py dist/Coda.wsz
python3 pkg/mkskin.py --epoc dist/Epoc.wsz
python3 pkg/mkicon.py dist "$CORE"
# the licences, beside the program (RPSL 2.1(e): the RPSL's text with the
# object code; LICENSE for PsiAmp's own code)
{
	printf 'PsiAmp - licences\r\n\r\nSource: https://github.com/danieledge/psiamp\r\n\r\n'
	printf "PsiAmp's own code is under the MIT licence:\r\n\r\n"
	sed 's/$/\r/' LICENSE
	printf '\r\n----------------------------------------------------------------------\r\n\r\n'
	printf "PsiAmp includes RealNetworks' Helix fixed-point MP3 decoder, under the\r\n"
	printf 'RealNetworks Public Source License 1.0 (below). Its source, with our\r\n'
	printf 'changes marked (helix/PSIAMP-CHANGES.txt), is at the address above.\r\n\r\n'
	printf 'Helix DNA Client technology included. Copyright (c) RealNetworks, Inc.,\r\n'
	printf '1995-2002. All rights reserved.\r\n\r\n'
	sed 's/\r$//; s/$/\r/' helix/RPSL.txt
} > dist/Licences.txt

echo "== PsiAmp.sis"
sis
ls -la "$HERE/dist/"
