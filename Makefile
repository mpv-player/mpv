# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected
#			* DVD support

include config.mak

#install...
OWNER = root
GROUP = root
PERM = 755

PRG = mplayer
PRG_HQ = mplayerHQ
PRG_AVIP = aviparse
PRG_TV = tvision
PRG_CFG = codec-cfg

prefix = /usr/local
BINDIR = ${prefix}/bin
# BINDIR = /usr/local/bin
SRCS = asf_streaming.c find_sub.c aviprint.c dll_init.c dec_audio.c aviwrite.c aviheader.c asfheader.c demux_avi.c demux_asf.c demux_mpg.c demuxer.c stream.c codec-cfg.c subreader.c linux/getch2.c linux/timer-lx.c linux/shmem.c xa/xa_gsm.c lirc_mp.c cfgparser.c mixer.c dvdauth.c spudec.c
OBJS = $(SRCS:.c=.o)
CFLAGS = $(OPTFLAGS) -Iloader -Ilibvo $(CSS_INC) # -Wall
A_LIBS = -Lmp3lib -lMP3 -Llibac3 -lac3
VO_LIBS = -Llibvo -lvo $(X_LIBS)

.SUFFIXES: .c .o

# .PHONY: all clean

all:	version.h config.h $(PRG)
# $(PRG_AVIP)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

COMMONLIBS = libvo/libvo.a libac3/libac3.a mp3lib/libMP3.a

loader/libloader.a:
	$(MAKE) -C loader

loader/DirectShow/libDS_Filter.a:
	$(MAKE) -C loader/DirectShow

libmpeg2/libmpeg2.a:
	$(MAKE) -C libmpeg2

libvo/libvo.a:
	$(MAKE) -C libvo

libac3/libac3.a:
	$(MAKE) -C libac3

mp3lib/libMP3.a:
	$(MAKE) -C mp3lib

opendivx/libdecore.a:
	$(MAKE) -C opendivx

encore/libencore.a:
	$(MAKE) -C encore

mplayerwithoutlink:	.depend mplayer.o $(OBJS) loader/libloader.a $(DS_DEP) libmpeg2/libmpeg2.a opendivx/libdecore.a $(COMMONLIBS) encore/libencore.a
	@for a in mp3lib libac3 libmpeg2 libvo opendivx encore loader/DirectShow ; do $(MAKE) -C $$a all ; done

$(PRG):	.depend mplayer.o $(OBJS) loader/libloader.a $(DS_DEP) libmpeg2/libmpeg2.a opendivx/libdecore.a $(COMMONLIBS) encore/libencore.a
	$(CC) $(CFLAGS) -o $(PRG) mplayer.o $(OBJS) $(XMM_LIBS) $(LIRC_LIBS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader $(DS_LIB) -ldl -Llibmpeg2 -lmpeg2 -Lopendivx -ldecore $(VO_LIBS) $(CSS_LIB) -Lencore -lencore -lpthread

# $(PRG_HQ):	.depend mplayerHQ.o $(OBJS) loader/libloader.a libmpeg2/libmpeg2.a opendivx/libdecore.a $(COMMONLIBS) encore/libencore.a
# 	$(CC) $(CFLAGS) -o $(PRG_HQ) mplayerHQ.o $(OBJS) $(XMM_LIBS) $(LIRC_LIBS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader -ldl -Llibmpeg2 -lmpeg2 -Lopendivx -ldecore $(VO_LIBS) -Lencore -lencore -lpthread

# $(PRG_AVIP):	.depend aviparse.o $(OBJS) loader/libloader.a $(COMMONLIBS)
# 	$(CC) $(CFLAGS) -o $(PRG_AVIP) aviparse.o $(OBJS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader -ldl $(VO_LIBS) -lpthread

# $(PRG_TV):	.depend tvision.o $(OBJS) $(COMMONLIBS)
# 	$(CC) $(CFLAGS) -o $(PRG_TV) tvision.o $(OBJS) -lm $(TERMCAP_LIB) $(VO_LIBS)

$(PRG_CFG):        .depend codec-cfg.c codec-cfg.h
	$(CC) $(CFLAGS) -g codec-cfg.c -o $(PRG_CFG) -DCODECS2HTML

install: $(PRG)
	install -g $(GROUP) -o $(OWNER) -m $(PERM) -s $(PRG) $(BINDIR)
	install -D -m 644 DOCS/mplayer.1 $(prefix)/man/man1/mplayer.1

clean:
	rm -f *.o *~ $(OBJS)

distclean:
	@for a in mp3lib libac3 libmpeg2 opendivx encore libvo loader loader/DirectShow drivers drivers/syncfb ; do $(MAKE) -C $$a distclean ; done
	rm -f *~ $(PRG) $(PRG_HQ) $(PRG_AVIP) $(PRG_TV) $(OBJS) *.o *.a .depend

dep:	depend

depend: .depend
	@for a in mp3lib libac3 libmpeg2 libvo opendivx encore loader/DirectShow ; do $(MAKE) -C $$a dep ; done

.depend: Makefile config.mak config.h
	$(CC) -MM $(CFLAGS) mplayer.c $(SRCS) 1>.depend

# ./configure must be run if it changed in CVS
config.h: configure
	@echo "############################################################"
	@echo "####### Please run ./configure again - it's changed! #######"
	@echo "############################################################"
	@exit 1

# rebuild at every config.h/config.mak change:
version.h: config.h config.mak Makefile
	$(MAKE) distclean
	./version.sh

# rebuild at every CVS update:
ifneq ($(wildcard CVS/Entries),)
version.h: CVS/Entries
endif

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

