
LIB	= libgui.a

include ../config.mak
include config.mak

include bitmap/bitmap.mak
MPLAYERDIR = mplayer/
include gui.mak

INCDIR  = -I. -I./event -I./wm -I./skin $(GTKINC) $(EXTRA_INC)

OPTIMIZE =  $(OPTFLAGS) -fomit-frame-pointer \
            -fexpensive-optimizations -fschedule-insns2 -Wall
ifeq ($(TARGET_ARCH_X86),yes)
OPTIMIZE += -malign-double
endif

CFLAGS = $(OPTIMIZE) $(INCDIR) $(DEBUG) 

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

### TARGETS ###

$(LIB):	.depend	$(OBJS)
	$(MAKE) -C mplayer
	rm -f $(LIB)
	$(AR) rc $(LIB) $(OBJS) $(MPLAYEROBJS)

all:	$(LIB)

clean:
	$(MAKE) -C mplayer clean
	rm -rf $(OBJS) *.o *~ *.bak main $(PRG) $(LIB) .depend

#dep:	depend
#
#depend:
#	makedepend -- $(CFLAGS) -- $(SRCS) &>/dev/null
#

distclean: clean

dep:    depend

depend: .depend

.depend: Makefile config.mak gui.mak bitmap/bitmap.mak
	$(MAKE) -C mplayer depend
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
