# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected
#			* DVD support

include config.mak

ifeq ($(ENABLE_XP),yes)
PRG = mplayerxp
PRG_HQ = mplayerHQxp
PRG_AVIP = aviparsexp
PRG_FIBMAP = fibmap_mplayerxp
PRG_TV = tvisionxp
PRG_CFG = codec-cfg-xp
PRG_MENCODER = mencoderxp
else
PRG = mplayer
PRG_HQ = mplayerHQ
PRG_AVIP = aviparse
PRG_FIBMAP = fibmap_mplayer
PRG_TV = tvision
PRG_CFG = codec-cfg
PRG_MENCODER = mencoder
endif
# these subdirectories required installation due binaries within them
ifeq ($(VIDIX),yes)
SUBDIRS += libdha vidix
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@; done
endif

#prefix = /usr/local
BINDIR = ${prefix}/bin
# BINDIR = /usr/local/bin
MANDIR = ${prefix}/man

# a BSD compatible 'install' program
INSTALL = install

SRCS_COMMON = cyuv.c adpcm.c xacodec.c cpudetect.c mp_msg.c ac3-iec958.c dec_audio.c msvidc.c cinepak.c fli.c qtrle.c codec-cfg.c cfgparser.c my_profile.c RTjpegN.c minilzo.c nuppelvideo.c spudec.c playtree.c playtreeparser.c asxparser.c qtsmc.c ducktm1.c roqav.c qtrpza.c
SRCS_MENCODER = mencoder.c $(SRCS_COMMON) libao2/afmt.c divx4_vbr.c libvo/aclib.c libvo/img_format.c libvo/osd.c me-opt-reg.c
SRCS_MPLAYER = mplayer.c $(SRCS_COMMON) find_sub.c subreader.c lirc_mp.c mixer.c vobsub.c mp-opt-reg.c

OBJS_MENCODER = $(SRCS_MENCODER:.c=.o)
OBJS_MPLAYER = $(SRCS_MPLAYER:.c=.o)

ifeq ($(PNG),yes)
SRCS_COMMON += mpng.c
endif

ifeq ($(VO2),yes)
VO_LIBS = -Llibvo2 -lvo2
VO_INC = -Ilibvo2
else
VO_LIBS = -Llibvo -lvo
VO_INC = -Ilibvo
endif
V_LIBS = $(X_LIB) $(MP1E_LIB) $(GGI_LIB) $(MLIB_LIB) $(SDL_LIB) $(SVGA_LIB) $(AA_LIB) $(DIRECTFB_LIB)

AO_LIBS = -Llibao2 -lao2
A_LIBS = $(ALSA_LIB) $(NAS_LIB) $(MAD_LIB) $(VORBIS_LIB) $(FAAD_LIB) $(SGIAUDIO_LIB)

CODEC_LIBS = -Llibmpcodecs -lmpcodecs -Lg72x -lg72x -Lmp3lib -lMP3 -Llibac3 -lac3 -Lliba52 -la52 -Lxa -lxa -Llibmpeg2 -lmpeg2 $(AV_LIB) -Llibfame -lfame
COMMON_LIBS = $(CODEC_LIBS) -Llibmpdemux -lmpdemux  $(NEW_INPUT_LIB)  $(LIB_LOADER) $(A_LIBS) $(CSS_LIB) $(ARCH_LIB) -Lpostproc -lpostproc $(DECORE_LIB) -Llinux -losdep $(TERMCAP_LIB)  $(STREAMING_LIB) $(Z_LIB) $(GTK_LIBS) $(PNG_LIB) $(JPEG_LIB) -lm
ifeq ($(VIDIX),yes)
MISC_LIBS += -Llibdha -ldha -Lvidix -lvidix
endif
CFLAGS = $(OPTFLAGS) -Ilibmpdemux -Iloader $(VO_INC) $(EXTRA_INC) # -Wall

PARTS = libfame g72x libmpdemux libmpcodecs mp3lib libac3 liba52 libmp1e libmpeg2 libavcodec opendivx libao2 drivers drivers/syncfb linux postproc xa input
ifeq ($(VIDIX),yes)
PARTS += libdha vidix
endif
ifeq ($(VO2),yes)
PARTS += libvo2
else
PARTS += libvo
endif

ifeq ($(GUI),yes)
PARTS += Gui
endif

ifneq ($(W32_LIB),)
PARTS += loader loader/dshow
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

COMMON_DEPS = libfame/libfame.a g72x/libg72x.a libmpdemux/libmpdemux.a libmpcodecs/libmpcodecs.a libao2/libao2.a libac3/libac3.a liba52/liba52.a mp3lib/libMP3.a libmpeg2/libmpeg2.a linux/libosdep.a postproc/libpostproc.a opendivx/libdecore.a xa/libxa.a input/libinput.a

ifeq ($(VIDIX),yes)
COMMON_DEPS += libdha/libdha.so vidix/libvidix.a
endif
ifeq ($(VO2),yes)
COMMON_DEPS += libvo2/libvo2.a
else
COMMON_DEPS += libvo/libvo.a
endif

loader/libloader.a:
	$(MAKE) -C loader

libfame/libfame.a:
	$(MAKE) -C libfame

libmpdemux/libmpdemux.a:
	$(MAKE) -C libmpdemux

libmpcodecs/libmpcodecs.a:
	$(MAKE) -C libmpcodecs

loader/DirectShow/libDS_Filter.a:
	$(MAKE) -C loader/DirectShow

loader/dshow/libDS_Filter.a:
	$(MAKE) -C loader/dshow

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

liba52/liba52.a:
	$(MAKE) -C liba52

mp3lib/libMP3.a:
	$(MAKE) -C mp3lib

opendivx/libdecore.a:
	$(MAKE) -C opendivx

libdha/libdha.so:
	$(MAKE) -C libdha

vidix/libvidix.a:
	$(MAKE) -C vidix

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
	$(MAKE) -C g72x

input/libinput.a:
	$(MAKE) -C input

MPLAYER_DEP = $(OBJS_MPLAYER) $(LOADER_DEP) $(MP1E_DEP) $(AV_DEP) $(COMMON_DEPS)
MENCODER_DEP = $(OBJS_MENCODER) $(LOADER_DEP) $(MP1E_DEP) $(AV_DEP) $(COMMON_DEPS)

ifeq ($(GUI),yes)
MPLAYER_DEP += Gui/libgui.a
MENCODER_DEP += Gui/libgui.a
GUI_LIBS = -LGui -lgui
endif

VIDIX_LIBS =
ifeq ($(VIDIX),yes)
VIDIX_LIBS += -Lvidix -lvidix
endif

$(PRG):	$(MPLAYER_DEP)
	$(CC) $(CFLAGS) -o $(PRG) $(OBJS_MPLAYER) $(VO_LIBS)  $(COMMON_LIBS) $(EXTRA_LIB) $(AO_LIBS) $(V_LIBS) $(VIDIX_LIBS) $(GUI_LIBS)  $(LIRC_LIB) $(STATIC_LIB) 

$(PRG_FIBMAP): fibmap_mplayer.o
	$(CC) -o $(PRG_FIBMAP) fibmap_mplayer.o

ifeq ($(MENCODER),yes)
$(PRG_MENCODER): $(MENCODER_DEP)
	$(CC) $(CFLAGS) -o $(PRG_MENCODER) $(OBJS_MENCODER) $(COMMON_LIBS) $(EXTRA_LIB) $(ENCORE_LIB) $(MLIB_LIB)
endif

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
ifeq ($(VIDIX),yes)
	$(DO_MAKE)
endif
	if test ! -d $(BINDIR) ; then mkdir -p $(BINDIR) ; fi
	$(INSTALL) -m 755 -s $(PRG) $(BINDIR)/$(PRG)
ifeq ($(GUI),yes)
	-ln -sf $(BINDIR)/$(PRG) $(BINDIR)/gmplayer
endif
	if test ! -d $(MANDIR)/man1 ; then mkdir -p $(MANDIR)/man1; fi
	$(INSTALL) -c -m 644 DOCS/mplayer.1 $(MANDIR)/man1/mplayer.1
ifeq ($(MENCODER),yes)
	$(INSTALL) -m 755 -s $(PRG_MENCODER) $(BINDIR)/$(PRG_MENCODER)
	$(INSTALL) -c -m 644 DOCS/mencoder.1 $(MANDIR)/man1/mencoder.1
endif
	@if test ! -d $(DATADIR) ; then mkdir -p $(DATADIR) ; fi
	@if test ! -d $(DATADIR)/font ; then mkdir -p $(DATADIR)/font ; fi
	@if test ! -f $(DATADIR)/font/font.desc ; then \
	echo "*** Download font at http://www.mplayerhq.hu/homepage/dload.html" ; \
	echo "*** for OSD/Subtitles support and extract to $(DATADIR)/font/" ; \
	fi
ifeq ($(GUI),yes)
	@if test ! -d $(DATADIR)/Skin ; then mkdir -p $(DATADIR)/Skin ; fi
	@echo "*** Download skin(s) at http://www.mplayerhq.hu/homepage/dload.html"
	@echo "*** for GUI, and extract to $(DATADIR)/Skin/"
endif
	@if test ! -d $(CONFDIR) ; then mkdir -p $(CONFDIR) ; fi
	@if test -f $(CONFDIR)/codecs.conf.old ; then mv -f $(CONFDIR)/codecs.conf.old $(CONFDIR)/codecs.conf.older ; fi
	@if test -f $(CONFDIR)/codecs.conf ; then mv -f $(CONFDIR)/codecs.conf $(CONFDIR)/codecs.conf.old ; fi
	$(INSTALL) -c -m 644 etc/codecs.conf $(CONFDIR)/codecs.conf

ifeq ($(CSS_USE),yes)
	@echo "Following task requires root privs. If it fails don't panic"
	@echo "however it means you can't use fibmap_mplayer."
	@echo "Without this (or without running mplayer as root) you won't be"
	@echo "able to play encrypted DVDs."
	-$(INSTALL) -o 0 -g 0 -m 4755 -s $(PRG_FIBMAP) $(BINDIR)/$(PRG_FIBMAP)
endif

uninstall:
	-rm -f $(BINDIR)/$(PRG) $(BINDIR)/gmplayer $(MANDIR)/man1/mplayer.1
	-rm -f $(BINDIR)/$(PRG_FIBMAP)
	-rm -f  $(BINDIR)/$(PRG_MENCODER) $(MANDIR)/man1/mencoder.1
	@echo "Uninstall completed"

clean:
	-rm -f *.o *~ $(OBJS)

distclean:
	-rm -f *~ $(PRG) $(PRG_FIBMAP) $(PRG_HQ) $(PRG_AVIP) $(PRG_TV) $(OBJS) $(PRG_MENCODER)
	-rm -f *.o *.a .depend configure.log
	@for a in $(PARTS); do $(MAKE) -C $$a distclean; done

dep:	depend

depend:
	./version.sh `$(CC) --version`
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
	./version.sh `$(CC) --version`
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
