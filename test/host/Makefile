# test/host: PsiAmp's decoder (Helix) and sound path on the PC
H = ../../helix
SRC = $(wildcard $(H)/real/*.c) $(H)/mp3dec.c $(H)/mp3tabs.c ../../engine/pacore.c padec.c
padec: $(SRC)
	gcc -O2 -DPSIAMP_HOST -I$(H)/pub -I$(H)/real -I../../engine -o $@ $(SRC)
clean:
	rm -f padec
