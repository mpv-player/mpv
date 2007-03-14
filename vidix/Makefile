include ../config.mak

LIBNAME_MPLAYER = libvidix.a

SRCS_MPLAYER    = vidixlib.c
OBJS_MPLAYER	= $(SRCS_MPLAYER:.c=.o)

CFLAGS  = $(OPTFLAGS)

all:	$(LIBNAME_MPLAYER)
	$(MAKE) -C drivers

$(LIBNAME_MPLAYER): $(OBJS_MPLAYER)
	$(AR) r $@ $^
	$(RANLIB) $@

clean:
	rm -f *.o *.a *~
	$(MAKE) -C drivers clean

distclean: clean
	rm -f .depend
	$(MAKE) -C drivers distclean

dep depend:
	$(CC) -MM $(CFLAGS) $(SRCS_MPLAYER) 1>.depend
	$(MAKE) -C drivers depend

-include .depend
