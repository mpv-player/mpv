include ../config.mak

LIBNAME_MPLAYER = libgui.a

#CFLAGS += -DDEBUG

ifeq ($(TARGET_WIN32),yes)
SRCS_MPLAYER = bitmap.c \
               win32/dialogs.c \
               win32/gui.c \
               win32/interface.c \
               win32/playlist.c \
               win32/preferences.c \
               win32/skinload.c \
               win32/widgetrender.c \
               win32/wincfg.c \

else
SRCS_MPLAYER = app.c \
               bitmap.c \
               cfg.c \
               interface.c \
               mplayer/gui_common.c \
               mplayer/menu.c \
               mplayer/mw.c \
               mplayer/pb.c \
               mplayer/play.c \
               mplayer/sw.c \
               mplayer/widgets.c \
               mplayer/gtk/about.c \
               mplayer/gtk/eq.c \
               mplayer/gtk/fs.c \
               mplayer/gtk/gtk_common.c \
               mplayer/gtk/gtk_url.c \
               mplayer/gtk/mb.c \
               mplayer/gtk/menu.c \
               mplayer/gtk/opts.c \
               mplayer/gtk/pl.c \
               mplayer/gtk/sb.c \
               skin/cut.c \
               skin/font.c \
               skin/skin.c \
               wm/ws.c \
               wm/wsxdnd.c \

endif

include ../mpcommon.mak

clean::
	rm -f mplayer/*.o mplayer/*~ mplayer/gtk/*.o mplayer/gtk/*~ \
              skin/*.o skin/*~ \
              wm/*.o wm/*~ \
              win32/*.o win32/*~ \
