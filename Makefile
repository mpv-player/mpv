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
PRG_MENCODER = mencoder

#prefix = /usr/local
BINDIR = ${prefix}/bin
# BINDIR = /usr/local/bin

# a BSD compatible 'install' program
INSTALL = install

SRCS_MENCODER = divx4_vbr.c mencoder.c libvo/aclib.c libvo/img_format.c ima4.c xacodec.c cpudetect.c mp_msg.c ac3-iec958.c dec_audio.c dec_video.c codec-cfg.c cfgparser.c
OBJS_MENCODER = $(SRCS_MENCODER:.c=.o)

SRCS_MPLAYER = mplayer.c ima4.c xacodec.c cpudetect.c mp_msg.c ac3-iec958.c find_sub.c dec_audio.c dec_video.c codec-cfg.c subreader.c lirc_mp.c cfgparser.c mixer.c spudec.c
OBJS_MPLAYER = $(SRCS_MPLAYER:.c=.o)
CFLAGS = $(OPTFLAGS) -Ilibmpdemux -Iloader -Ilibvo $(EXTRA_INC) $(MADLIB_INC) # -Wall
A_LIBS = -Lmp3lib -lMP3 -Llibac3 -lac3 $(ALSA_LIB) $(ESD_LIB) $(MADLIB_LIB) $(SGI_AUDIO_LIB)
VO_LIBS = -Llibvo -lvo $(MLIB_LIB) $(X_LIBS)
OSDEP_LIBS = -Llinux -losdep
PP_LIBS = -Lpostproc -lpostproc
XA_LIBS = -Lxa -lxa

# SRCS = $(SRCS_MENCODER) $(SRCS_MPLAYER)
# OBJS = $(OBJS_MENCODER) $(OBJS_MPLAYER)

PARTS = libmpdemux mp3lib libac3 libmpeg2 opendivx libavcodec libvo libao2 drivers drivers/syncfb linux postproc xa

ifeq ($(GUI),yes)
PARTS += Gui
endif

ifneq ($(W32_LIB),)
PARTS += loader loader/DirectShow
SRCS_MPLAYER += dll_init.c
SRCS_MENCODER += dll_init.c
# SRCS += dll_init.c
endif
LOADER_DEP = $(W32_DEP) $(DS_DEP)
LIB_LOADER = $(W32_LIB) $(DS_LIB)

ALL_PRG = $(PRG)
ifeq ($(CSS_USE),yes)
ALL_PRG += $(PRG_FIBMAP)
endif

.SUFFIXES: .cc .c .o

# .PHONY: all clean

all:	$(ALL_PRG)

# $(PRG_AVIP)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

COMMONLIBS = libmpdemux/libmpdemux.a libvo/libvo.a libao2/libao2.a libac3/libac3.a mp3lib/libMP3.a libmpeg2/libmpeg2.a opendivx/libdecore.a linux/libosdep.a postproc/libpostproc.a xa/libxa.a

loader/libloader.a:
	$(MAKE) -C loader

libmpdemux/libmpdemux.a:
	$(MAKE) -C libmpdemux

loader/DirectShow/libDS_Filter.a:
	$(MAKE) -C loader/DirectShow

libavcodec/libavcodec.a:
	$(MAKE) -C libavcodec

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

# encore/libencore.a:
# 	$(MAKE) -C encore

Gui/libgui.a:
	$(MAKE) -C Gui

linux/libosdep.a:
	$(MAKE) -C linux

postproc/libpostproc.a:
	$(MAKE) -C postproc

xa/libxa.a:
	$(MAKE) -C xa

MPLAYER_DEP = $(OBJS_MPLAYER) $(LOADER_DEP) $(AV_DEP) $(COMMONLIBS) 
MENCODER_DEP = $(OBJS_MENCODER) $(LOADER_DEP) $(AV_DEP) $(COMMONLIBS)

ifeq ($(GUI),yes)
MPLAYER_DEP += Gui/libgui.a
MENCODER_DEP += Gui/libgui.a
endif

$(PRG):	$(MPLAYER_DEP)
	$(CC) -rdynamic $(CFLAGS) -o $(PRG) $(OBJS_MPLAYER) -Llibmpdemux -lmpdemux $(XMM_LIBS) $(LIRC_LIBS) $(LIB_LOADER) $(AV_LIB) -Llibmpeg2 -lmpeg2 -Llibao2 -lao2 $(A_LIBS) $(VO_LIBS) $(CSS_LIB) $(GUI_LIBS) $(ARCH_LIBS) $(OSDEP_LIBS) $(PP_LIBS) $(XA_LIBS) $(DECORE_LIBS) $(TERMCAP_LIB) -lm

$(PRG_FIBMAP): fibmap_mplayer.o
	$(CC) -o $(PRG_FIBMAP) fibmap_mplayer.o

$(PRG_MENCODER): $(MENCODER_DEP)
	$(CC) -rdynamic $(CFLAGS) -o $(PRG_MENCODER) $(OBJS_MENCODER) -Llibmpeg2 -lmpeg2 -Llibmpdemux -lmpdemux $(X_LIBS) $(XMM_LIBS) $(LIB_LOADER) $(AV_LIB) -lmp3lame $(A_LIBS) $(CSS_LIB) $(GUI_LIBS) $(ARCH_LIBS) $(OSDEP_LIBS) $(PP_LIBS) $(XA_LIBS) $(DECORE_LIBS) $(TERMCAP_LIB) -lm

# $(PRG_HQ):	depfile mplayerHQ.o $(OBJS) loader/libloader.a libmpeg2/libmpeg2.a opendivx/libdecore.a $(COMMONLIBS) encore/libencore.a
# 	$(CC) $(CFLAGS) -o $(PRG_HQ) mplayerHQ.o $(OBJS) $(XMM_LIBS) $(LIRC_LIBS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader -ldl -Llibmpeg2 -lmpeg2 -Lopendivx -ldecore $(VO_LIBS) -Lencore -lencore -lpthread

# $(PRG_AVIP):	depfile aviparse.o $(OBJS) loader/libloader.a $(COMMONLIBS)
# 	$(CC) $(CFLAGS) -o $(PRG_AVIP) aviparse.o $(OBJS) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader -ldl $(VO_LIBS) -lpthread

#$(PRG_TV):	depfile tvision.o $(OBJS) $(COMMONLIBS)
#	$(CC) $(CFLAGS) -o $(PRG_TV) tvision.o $(OBJS) -lm $(TERMCAP_LIB) $(VO_LIBS)

# Every mplayer dependancy depends on version.h, to force building version.h
# first (in serial mode) before any other of the dependancies for a parallel make
# run.  This is necessary, because the make rule for version.h removes objects
# in a recursive "make distclean" and we must wait for this "make distclean" to
# finish before be can start builing new object files.
$(MPLAYER_DEP): version.h
$(MENCODER_DEP): version.h

$(PRG_CFG): version.h codec-cfg.c codec-cfg.h
	$(CC) $(CFLAGS) -g codec-cfg.c -o $(PRG_CFG) -DCODECS2HTML

install: $(ALL_PRG)
	if test ! -d $(BINDIR) ; then mkdir -p $(BINDIR) ; fi
	$(INSTALL) -m 755 -s $(PRG) $(BINDIR)/$(PRG)
	if test -x $(PRG_MENCODER) ; then $(INSTALL) -m 755 -s $(PRG_MENCODER) $(BINDIR)/$(PRG_MENCODER) ; fi
ifeq ($(GUI),yes)
	-ln -sf $(BINDIR)/$(PRG) $(BINDIR)/gmplayer
endif
	if test ! -d $(prefix)/man/man1 ; then mkdir -p $(prefix)/man/man1; fi
	$(INSTALL) -c -m 644 DOCS/mplayer.1 $(prefix)/man/man1/mplayer.1
	if test -x $(PRG_MENCODER) ; then $(INSTALL) -c -m 644 DOCS/mencoder.1 $(prefix)/man/man1/mencoder.1 ; fi
ifeq ($(CSS_USE),yes)
	@echo "Following task requires root privs. If it fails don't panic"
	@echo "however it means you can't use fibmap_mplayer."
	@echo "Without this (or without running mplayer as root) you won't be"
	@echo "able to play encrypted DVDs."
	-$(INSTALL) -o 0 -g 0 -m 4755 -s $(PRG_FIBMAP) $(BINDIR)/$(PRG_FIBMAP)
endif

uninstall:
	rm -f $(BINDIR)/$(PRG)
	rm -f $(BINDIR)/gmplayer
	rm -f $(prefix)/man/man1/mplayer.1
	rm -f $(BINDIR)/$(PRG_FIBMAP)
	@echo "Uninstall completed"

clean:
	rm -f *.o *~ $(OBJS)

distclean:
	rm -f *~ $(PRG) $(PRG_FIBMAP) $(PRG_HQ) $(PRG_AVIP) $(PRG_TV) $(OBJS) $(PRG_MENCODER) *.o *.a .depend
	@for a in $(PARTS); do $(MAKE) -C $$a distclean; done

dep:	depend

depend:
	./version.sh
	$(CC) -MM $(CFLAGS) mplayer.c mencoder.c $(SRCS_MPLAYER) $(SRCS_MENCODER) 1>.depend
	@for a in $(PARTS); do $(MAKE) -C $$a dep; done

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
