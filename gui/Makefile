include ../config.mak

LIBNAME_MPLAYER = libgui.a

#CFLAGS += -DDEBUG

ifeq ($(TARGET_WIN32),yes)
SRCS_MPLAYER = win32/dialogs.c \
               win32/gui.c \
               win32/interface.c \
               win32/playlist.c \
               win32/preferences.c \
               win32/skinload.c \
               win32/widgetrender.c \
               win32/wincfg.c \

else
SRCS_MPLAYER = wm/ws.c \
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

include ../mpcommon.mak

clean::
	rm -f mplayer/*.o mplayer/*~ mplayer/gtk/*.o mplayer/gtk/*~ \
              skin/*.o skin/*~ \
              wm/*.o wm/*~ \
              win32/*.o win32/*~ \
