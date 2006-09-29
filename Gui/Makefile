include ../config.mak

LIBNAME = libgui.a

INCDIR  = -I. -I.. -I../loader -I./wm -I./skin

CFLAGS = $(INCDIR) $(OPTFLAGS)
#CFLAGS += -DDEBUG

ifeq ($(TARGET_WIN32),yes)
SRCS = win32/dialogs.c \
       win32/gui.c \
       win32/interface.c \
       win32/playlist.c \
       win32/preferences.c \
       win32/skinload.c \
       win32/widgetrender.c \
       win32/wincfg.c \

else
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

endif
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
              wm/*.o wm/*~ \
              win32/*.o win32/*~ \

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
