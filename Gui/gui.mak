
SKINSRC = skin/skin.c skin/font.c skin/cut.c
SKINOBJ = skin/skin.o skin/font.o skin/cut.o

MPLAYERSRCS = $(MPLAYERDIR)mplayer.c $(MPLAYERDIR)widgets.c $(MPLAYERDIR)play.c \
	     $(MPLAYERDIR)psignal.c
MPLAYEROBJS = $(MPLAYERSRCS:.c=.o)

SRCS = $(SKINSRC) $(BITMAPSRCS) wm/ws.c wm/wsconv.c app.c events.c timer.c error.c
OBJS = $(SRCS:.c=.o)

