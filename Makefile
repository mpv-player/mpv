# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected
#			* DVD support

include config.mak

PRG = mplayer
PRG_HQ = mplayerHQ
PRG_AVIP = aviparse
PRG_FIBMAP = fibmap_mplayer
PRG_TV = tvision
PRG_CFG = codec-cfg

#prefix = /usr/local
BINDIR = ${prefix}/bin
# BINDIR = /usr/local/bin
SRCS = find_sub.c aviprint.c dll_init.c dec_audio.c aviwrite.c aviheader.c asfheader.c demux_avi.c demux_asf.c demux_mpg.c demuxer.c stream.c codec-cfg.c subreader.c linux/getch2.c linux/timer-lx.c linux/shmem.c xa/xa_gsm.c lirc_mp.c cfgparser.c mixer.c dvdauth.c spudec.c $(STREAM_SRCS)
OBJS = $(SRCS:.c=.o)
CFLAGS = $(OPTFLAGS) -Iloader -Ilibvo $(CSS_INC) # -Wall
A_LIBS = -Lmp3lib -lMP3 -Llibac3 -lac3 $(ALSA_LIB) $(ESD_LIB)
VO_LIBS = -Llibvo -lvo $(X_LIBS)

.SUFFIXES: .c .o

# .PHONY: all clean

all:	$(PRG) $(PRG_FIBMAP)
# $(PRG_AVIP)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

COMMONLIBS = libvo/libvo.a libao2/libao2.a libac3/libac3.a mp3lib/libMP3.a

loader/libloader.a:
	$(MAKE) -C loader

loader/DirectShow/libDS_Filter.a:
	$(MAKE) -C loader/DirectShow

libmpeg2/libmpeg2.a:
	$(MAKE) -C libmpeg2

libvo/libvo.a:
	$(MAKE) -C libvo

libao2/libao2.a:
	$(MAKE) -C libao2

libac3/libac3.a:
	$(MAKE) -C libac3

mp3lib/libMP3.a:
	$(MAKE) -C mp3lib

opendivx/libdecore.a:
	$(MAKE) -C opendivx

encore/libencore.a:
	$(MAKE) -C encore

mplayerwithoutlink:	version.h mplayer.o $(OBJS) loader/libloader.a $(DS_DEP) libmpeg2/libmpeg2.a opendivx/libdecore.a $(COMMONLIBS) encore/libencore.a
	@for a in mp3lib libac3 libmpeg2 libvo opendivx encore loader/DirectShow ; do $(MAKE) -C $$a all ; done

$(PRG):	version.h mplayer.o $(OBJS) loader/libloader.a $(DS_DEP) libmpeg2/libmpeg2.a opendivx/libdecore.a $(COMMONLIBS) encore/libencore.a
	$(CC) $(CFLAGS) -o $(PRG) mplayer.o $(OBJS) $(XMM_LIBS) $(LIRC_LIBS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader $(DS_LIB) -Llibmpeg2 -lmpeg2 -Lopendivx -ldecore -Llibao2 -lao2 $(VO_LIBS) $(CSS_LIB) -Lencore -lencore $(ARCH_LIBS)

$(PRG_FIBMAP): fibmap_mplayer.o
	$(CC) -o $(PRG_FIBMAP) fibmap_mplayer.o

# $(PRG_HQ):	depfile mplayerHQ.o $(OBJS) loader/libloader.a libmpeg2/libmpeg2.a opendivx/libdecore.a $(COMMONLIBS) encore/libencore.a
# 	$(CC) $(CFLAGS) -o $(PRG_HQ) mplayerHQ.o $(OBJS) $(XMM_LIBS) $(LIRC_LIBS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader -ldl -Llibmpeg2 -lmpeg2 -Lopendivx -ldecore $(VO_LIBS) -Lencore -lencore -lpthread

# $(PRG_AVIP):	depfile aviparse.o $(OBJS) loader/libloader.a $(COMMONLIBS)
# 	$(CC) $(CFLAGS) -o $(PRG_AVIP) aviparse.o $(OBJS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader -ldl $(VO_LIBS) -lpthread

# $(PRG_TV):	depfile tvision.o $(OBJS) $(COMMONLIBS)
# 	$(CC) $(CFLAGS) -o $(PRG_TV) tvision.o $(OBJS) -lm $(TERMCAP_LIB) $(VO_LIBS)

$(PRG_CFG):        version.h codec-cfg.c codec-cfg.h
	$(CC) $(CFLAGS) -g codec-cfg.c -o $(PRG_CFG) -DCODECS2HTML

install: $(PRG) $(PRG_FIBMAP)
	install -D -m 755 -s $(PRG) $(BINDIR)/$(PRG)
	install -D -m 644 DOCS/mplayer.1 $(prefix)/man/man1/mplayer.1
	@echo "Following task requires root privs. If it fails don't panic"
	@echo "however it means you can't use fibmap_mplayer."
	@echo "Without this (or without running mplayer as root) you won't be"
	@echo "able to play DVDs."
	install -D -o root -g root -m 4755 -s $(PRG_FIBMAP) $(BINDIR)/$(PRG_FIBMAP)

clean:
	rm -f *.o *~ $(OBJS)

distclean:
	@for a in mp3lib libac3 libmpeg2 opendivx encore libvo libao2 loader loader/DirectShow drivers drivers/syncfb ; do $(MAKE) -C $$a distclean ; done
	rm -f *~ $(PRG) $(PRG_FIBMAP) $(PRG_HQ) $(PRG_AVIP) $(PRG_TV) $(OBJS) *.o *.a .depend

dep:	depend

depend:
	./version.sh
	$(CC) -MM $(CFLAGS) mplayer.c $(SRCS) 1>.depend
	@for a in mp3lib libac3 libmpeg2 libvo libao2 opendivx encore loader/DirectShow ; do $(MAKE) -C $$a dep ; done

# ./configure must be run if it changed in CVS
config.h: configure
	@echo "############################################################"
	@echo "####### Please run ./configure again - it's changed! #######"
	@echo "############################################################"
	@exit 1

# rebuild at every config.h/config.mak change:
version.h: config.h config.mak Makefile
	./version.sh
	$(MAKE) distclean
	$(MAKE) depend

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


