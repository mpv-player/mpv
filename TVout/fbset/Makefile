#
# Linux Frame Buffer Device Configuration
#

CFLAGS =	-Wall -O2 -I.
BISON =		bison -d
FLEX =		flex
INSTALL =	install
RM =		rm -f

All:		fbset


fbset:		fbset.o modes.tab.o lex.yy.o

fbset.o:	fbset.c fbset.h fb.h
modes.tab.o:	modes.tab.c fbset.h fb.h
lex.yy.o:	lex.yy.c fbset.h modes.tab.h

lex.yy.c:	modes.l
		$(FLEX) modes.l

modes.tab.c:	modes.y
		$(BISON) modes.y

install:	fbset
		if [ -f /sbin/fbset ]; then rm /sbin/fbset; fi
		$(INSTALL) fbset /usr/sbin
		$(INSTALL) fbset.8 /usr/man/man8
		$(INSTALL) fb.modes.5 /usr/man/man5
		if [ ! -c /dev/fb0 ]; then mknod /dev/fb0 c 29 0; fi
		if [ ! -c /dev/fb1 ]; then mknod /dev/fb1 c 29 32; fi
		if [ ! -c /dev/fb2 ]; then mknod /dev/fb2 c 29 64; fi
		if [ ! -c /dev/fb3 ]; then mknod /dev/fb3 c 29 96; fi
		if [ ! -c /dev/fb4 ]; then mknod /dev/fb4 c 29 128; fi
		if [ ! -c /dev/fb5 ]; then mknod /dev/fb5 c 29 160; fi
		if [ ! -c /dev/fb6 ]; then mknod /dev/fb6 c 29 192; fi
		if [ ! -c /dev/fb7 ]; then mknod /dev/fb7 c 29 224; fi

clean:
		$(RM) *.o fbset lex.yy.c modes.tab.c modes.tab.h
