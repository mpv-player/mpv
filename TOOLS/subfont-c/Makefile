#font="/mnt/win/windows/fonts/arial.ttf"
#font="/mnt/win/windows/fonts/comic.ttf"
#font="/mnt/win/windows/fonts/verdanai.ttf"
font="/mnt/win/windows/fonts/verdana.ttf"

#encoding=windows-1250
encoding=iso-8859-2

fontsize=20

include ../../config.mak

LDLIBS=-lm $(shell freetype-config --libs)
CFLAGS=$(OPTFLAGS) $(shell freetype-config --cflags)

subfont:	subfont.o

run:	subfont
	./subfont $(encoding) $(fontsize) $(font)
	cat font.desc.tail >> font.desc
	cp font.desc *.raw ~/.mplayer/font/

clean:
	rm -f subfont subfont.o
