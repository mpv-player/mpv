include ../config.mak
include config.mak

LIBNAME = libgui.a

INCDIR  = -I. -I.. -I../loader -I./wm -I./skin $(FREETYPE_INC) $(GTKINC) $(EXTRA_INC) 

OPTIMIZE =  $(OPTFLAGS)

CFLAGS = $(OPTIMIZE) $(INCDIR) $(DEBUG) 

SRCS = wm/ws.c \
       wm/wsxdnd.c \
       app.c \
       interface.c \
       cfg.c \
       bitmap.c \
       skin/skin.c \
       skin/font.c \
       skin/cut.c \
       mplayer/widgets.c \
       mplayer/play.c \
       mplayer/mw.c \
       mplayer/sw.c \
       mplayer/menu.c \
       mplayer/pb.c \
       mplayer/common.c \
       mplayer/gtk/menu.c \
       mplayer/gtk/mb.c \
       mplayer/gtk/about.c \
       mplayer/gtk/pl.c \
       mplayer/gtk/sb.c \
       mplayer/gtk/fs.c \
       mplayer/gtk/opts.c \
       mplayer/gtk/url.c \
       mplayer/gtk/eq.c \
       mplayer/gtk/common.c \

OBJS = $(SRCS:.c=.o)

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

### TARGETS ###

$(LIBNAME): $(OBJS)
	rm -f $(LIBNAME)
	$(AR) rc $(LIBNAME) $(OBJS) $(MPLAYEROBJS)
	$(RANLIB) $(LIBNAME)

all:	$(LIBNAME)

clean:
	rm -f *.o *.a *~ \
              mplayer/*.o mplayer/*~ \
              mplayer/gtk/*.o mplayer/gtk/*~ \
              skin/*.o skin/*~ \
              wm/*.o wm/*~

#dep:	depend
#
#depend:
#	makedepend -- $(CFLAGS) -- $(SRCS) &>/dev/null
#

distclean: clean
	rm -f .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
