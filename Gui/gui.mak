
SKINSRC = skin/skin.c skin/font.c skin/cut.c
SKINOBJ = skin/skin.o skin/font.o skin/cut.o

GTKSRCS = $(MPLAYERDIR)gtk/menu.c $(MPLAYERDIR)gtk/mb.c $(MPLAYERDIR)gtk/about.c \
	     $(MPLAYERDIR)gtk/pl.c $(MPLAYERDIR)gtk/sb.c $(MPLAYERDIR)gtk/fs.c \
	     $(MPLAYERDIR)gtk/opts.c  $(MPLAYERDIR)gtk/url.c

MPLAYERSRCS = $(MPLAYERDIR)mplayer.c $(MPLAYERDIR)widgets.c $(MPLAYERDIR)play.c \
	      $(GTKSRCS)
MPLAYEROBJS = $(MPLAYERSRCS:.c=.o)

SRCS = $(SKINSRC) $(BITMAPSRCS) wm/ws.c wm/wsconv.c app.c events.c interface.c
OBJS = $(SRCS:.c=.o)

