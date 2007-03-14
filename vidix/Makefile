include ../config.mak

LIBNAME_MPLAYER = libvidix.a

SRCS_MPLAYER    = vidixlib.c

libs:
	$(MAKE) -C drivers

clean::
	$(MAKE) -C drivers clean

distclean::
	$(MAKE) -C drivers distclean

dep depend::
	$(MAKE) -C drivers depend

include ../mpcommon.mak
