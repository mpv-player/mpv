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

SRCS_MENCODER = libao2/afmt.c divx4_vbr.c mencoder.c libvo/aclib.c libvo/img_format.c ima4.c xacodec.c cpudetect.c mp_msg.c ac3-iec958.c dec_audio.c dec_video.c msvidc.c fli.c codec-cfg.c cfgparser.c my_profile.c
OBJS_MENCODER = $(SRCS_MENCODER:.c=.o)

SRCS_MPLAYER = mplayer.c ima4.c xacodec.c cpudetect.c mp_msg.c ac3-iec958.c find_sub.c dec_audio.c dec_video.c msvidc.c fli.c codec-cfg.c subreader.c lirc_mp.c cfgparser.c mixer.c spudec.c my_profile.c
OBJS_MPLAYER = $(SRCS_MPLAYER:.c=.o)

CFLAGS = $(OPTFLAGS) -Ilibmpdemux -Iloader -Ilibvo $(EXTRA_INC) # -Wall
VO_LIBS = -Llibvo -lvo $(X_LIB) $(DXR3_LIB) $(GGI_LIB) $(MLIB_LIB) $(PNG_LIB) $(SDL_LIB) $(SVGA_LIB) $(AA_LIB) $(DIRECTFB_LIB)
ifeq ($(VO2),yes)
CFLAGS = $(OPTFLAGS) -Ilibmpdemux -Iloader -Ilibvo2 $(EXTRA_INC) # -Wall
VO_LIBS = -Llibvo2 -lvo2 $(X_LIB) $(DXR3_LIB) $(GGI_LIB) $(MLIB_LIB) $(PNG_LIB) $(SDL_LIB) $(SVGA_LIB)
endif

A_LIBS = -Lmp3lib -lMP3 -Llibac3 -lac3 $(ALSA_LIB) $(MAD_LIB) $(VORBIS_LIB) $(SGIAUDIO_LIB)

OSDEP_LIBS = -Llinux -losdep
PP_LIBS = -Lpostproc -lpostproc
XA_LIBS = -Lxa -lxa

# SRCS = $(SRCS_MENCODER) $(SRCS_MPLAYER)
# OBJS = $(OBJS_MENCODER) $(OBJS_MPLAYER)

PARTS = libmpdemux mp3lib libac3 libmp1e libmpeg2 opendivx libavcodec libvo libao2 drivers drivers/syncfb linux postproc xa
ifeq ($(VO2),yes)
PARTS = libmpdemux mp3lib libac3 libmp1e libmpeg2 opendivx libavcodec libvo2 libao2 drivers drivers/syncfb linux postproc xa
endif


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
ifeq ($(MENCODER),yes)
ALL_PRG += $(PRG_MENCODER)
endif
ifeq ($(CSS_USE),yes)
ALL_PRG += $(PRG_FIBMAP)
endif

.SUFFIXES: .cc .c .o

# .PHONY: all clean

all:	$(ALL_PRG)

# $(PRG_AVIP)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

COMMONLIBS = libmpdemux/libmpdemux.a libvo/libvo.a libao2/libao2.a libac3/libac3.a mp3lib/libMP3.a libmp1e/libmp1e.a libmpeg2/libmpeg2.a opendivx/libdecore.a linux/libosdep.a postproc/libpostproc.a xa/libxa.a
ifeq ($(VO2),yes)
COMMONLIBS = libmpdemux/libmpdemux.a libvo2/libvo2.a libao2/libao2.a libac3/libac3.a mp3lib/libMP3.a libmp1e/libmp1e.a libmpeg2/libmpeg2.a opendivx/libdecore.a linux/libosdep.a postproc/libpostproc.a xa/libxa.a
endif

loader/libloader.a:
	$(MAKE) -C loader

libmpdemux/libmpdemux.a:
	$(MAKE) -C libmpdemux

loader/DirectShow/libDS_Filter.a:
	$(MAKE) -C loader/DirectShow

libmp1e/libmp1e.a:
	$(MAKE) -C libmp1e

libavcodec/libavcodec.a:
	$(MAKE) -C libavcodec

libmpeg2/libmpeg2.a:
	$(MAKE) -C libmpeg2

libvo/libvo.a:
	$(MAKE) -C libvo

libvo2/libvo2.a:
	$(MAKE) -C libvo2

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

g72x/libg72x.a:
	$(MAKE) -C libg72x

MPLAYER_DEP = $(OBJS_MPLAYER) $(LOADER_DEP) $(AV_DEP) $(COMMONLIBS) 
MENCODER_DEP = $(OBJS_MENCODER) $(LOADER_DEP) $(AV_DEP) $(COMMONLIBS)

ifeq ($(GUI),yes)
MPLAYER_DEP += Gui/libgui.a
MENCODER_DEP += Gui/libgui.a
endif

$(PRG):	$(MPLAYER_DEP)
	$(CC) $(CFLAGS) -o $(PRG) $(OBJS_MPLAYER) -Llibmpdemux -lmpdemux $(AV_LIB) $(EXTRA_LIB) $(LIRC_LIB) $(LIB_LOADER) -Llibmpeg2 -lmpeg2 -Llibao2 -lao2 $(A_LIBS) $(VO_LIBS) $(CSS_LIB) $(ARCH_LIB) $(OSDEP_LIBS) $(PP_LIBS) $(XA_LIBS) $(DECORE_LIB) $(TERMCAP_LIB) -Llibmp1e -lmp1e -lm $(STATIC_LIB) $(GUI_LIBS) $(PNG_LIB) $(Z_LIB)

$(PRG_FIBMAP): fibmap_mplayer.o
	$(CC) -o $(PRG_FIBMAP) fibmap_mplayer.o

ifeq ($(MENCODER),yes)
$(PRG_MENCODER): $(MENCODER_DEP)
	$(CC) $(CFLAGS) -o $(PRG_MENCODER) $(OBJS_MENCODER) -Llibmpeg2 -lmpeg2 -Llibmpdemux -lmpdemux -Llibmp1e -lmp1e $(X_LIBS) $(LIB_LOADER) $(AV_LIB) -lmp3lame $(A_LIBS) $(CSS_LIB) $(GUI_LIBS) $(PNG_LIB) $(Z_LIB) $(ARCH_LIB) $(OSDEP_LIBS) $(PP_LIBS) $(XA_LIBS) $(DECORE_LIB) $(ENCORE_LIB) $(TERMCAP_LIB) -lm

endif

# $(PRG_HQ):	depfile mplayerHQ.o $(OBJS) loader/libloader.a libmpeg2/libmpeg2.a opendivx/libdecore.a $(COMMONLIBS) encore/libencore.a
# 	$(CC) $(CFLAGS) -o $(PRG_HQ) mplayerHQ.o $(OBJS) $(LIRC_LIB) $(A_LIBS) -lm $(TERMCAP_LIB) -Lloader -lloader -ldl -Llibmpeg2 -lmpeg2 -Lopendivx -ldecore $(VO_LIBS) -Lencore -lencore -lpthread

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
ifeq ($(GUI),yes)
	-ln -sf $(BINDIR)/$(PRG) $(BINDIR)/gmplayer
endif
	if test ! -d $(prefix)/man/man1 ; then mkdir -p $(prefix)/man/man1; fi
	$(INSTALL) -c -m 644 DOCS/mplayer.1 $(prefix)/man/man1/mplayer.1
ifeq ($(MENCODER),yes)
	$(INSTALL) -m 755 -s $(PRG_MENCODER) $(BINDIR)/$(PRG_MENCODER)
	$(INSTALL) -c -m 644 DOCS/mencoder.1 $(prefix)/man/man1/mencoder.1
endif

ifeq ($(CSS_USE),yes)
	@echo "Following task requires root privs. If it fails don't panic"
	@echo "however it means you can't use fibmap_mplayer."
	@echo "Without this (or without running mplayer as root) you won't be"
	@echo "able to play encrypted DVDs."
	-$(INSTALL) -o 0 -g 0 -m 4755 -s $(PRG_FIBMAP) $(BINDIR)/$(PRG_FIBMAP)
endif

uninstall:
	-rm -f $(BINDIR)/$(PRG) $(BINDIR)/gmplayer $(prefix)/man/man1/mplayer.1
	-rm -f $(BINDIR)/$(PRG_FIBMAP)
	-rm -f  $(BINDIR)/$(PRG_MENCODER) $(prefix)/man/man1/mencoder.1
	@echo "Uninstall completed"

clean:
	-rm -f *.o *~ $(OBJS)

distclean:
	-rm -f *~ $(PRG) $(PRG_FIBMAP) $(PRG_HQ) $(PRG_AVIP) $(PRG_TV) $(OBJS) $(PRG_MENCODER)
	-rm -f *.o *.a .depend configure.log
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

# do not rebuild after cvs commits if .developer file is present!

# rebuild at every config.h/config.mak change:
version.h: config.h config.mak Makefile
	./version.sh
ifeq ($(wildcard .developer),)
	$(MAKE) distclean
endif
	$(MAKE) depend

# rebuild at every CVS update:
ifeq ($(wildcard .developer),)
ifneq ($(wildcard CVS/Entries),)
version.h: CVS/Entries
endif
endif

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
