
include ../../config.mak

LDLIBS=-lm $(shell freetype-config --libs)
CFLAGS=$(OPTFLAGS) $(shell freetype-config --cflags)

#CFLAGS+=-DOLD_FREETYPE2
#CFLAGS+=-g
#CFLAGS+=-DDEBUG


subfont:	subfont.o

subfont.S:	subfont.c
	$(CC) $(CFLAGS) -S $^ -o $@

clean:
	rm -f subfont subfont.o core
