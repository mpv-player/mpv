# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected
#			* DVD support

include config.mak

PRG = mplayer
PRG_FIBMAP = fibmap_mplayer
PRG_CFG = codec-cfg
PRG_MENCODER = mencoder

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

SRCS_COMMON = xacodec.c cpudetect.c codec-cfg.c cfgparser.c my_profile.c spudec.c playtree.c playtreeparser.c asxparser.c vobsub.c subreader.c find_sub.c
SRCS_MENCODER = mencoder.c mp_msg-mencoder.c $(SRCS_COMMON) libao2/afmt.c divx4_vbr.c libvo/aclib.c libvo/osd.c libvo/sub.c libvo/font_load.c me-opt-reg.c
SRCS_MPLAYER = mplayer.c mp_msg.c $(SRCS_COMMON) lirc_mp.c mixer.c mp-opt-reg.c

OBJS_MENCODER = $(SRCS_MENCODER:.c=.o)
OBJS_MPLAYER = $(SRCS_MPLAYER:.c=.o)

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

CODEC_LIBS = -Llibmpcodecs -lmpcodecs -Lmp3lib -lMP3 -Lliba52 -la52 -Llibmpeg2 -lmpeg2 $(AV_LIB) $(FAME_LIB)
COMMON_LIBS = $(CODEC_LIBS) -Llibmpdemux -lmpdemux  $(NEW_INPUT_LIB)  $(LIB_LOADER) $(A_LIBS) $(CSS_LIB) $(ARCH_LIB) -Lpostproc -lpostproc $(DECORE_LIB) -Llinux -losdep $(TERMCAP_LIB)  $(STREAMING_LIB) $(Z_LIB) $(GTK_LIBS) $(PNG_LIB) $(JPEG_LIB) $(GIF_LIB) -lm
ifeq ($(VIDIX),yes)
MISC_LIBS += -Llibdha -ldha -Lvidix -lvidix
endif
CFLAGS = $(OPTFLAGS) -Ilibmpdemux -Iloader $(VO_INC) $(EXTRA_INC) # -Wall

PARTS = libfame libmpdemux libmpcodecs mp3lib liba52 libmp1e libmpeg2 libavcodec libao2 drivers drivers/syncfb linux postproc input libmpdvdkit
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

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

COMMON_DEPS = libmpdemux/libmpdemux.a libmpcodecs/libmpcodecs.a libao2/libao2.a liba52/liba52.a mp3lib/libMP3.a libmpeg2/libmpeg2.a linux/libosdep.a postproc/libpostproc.a input/libinput.a

ifeq ($(VIDIX),yes)
COMMON_DEPS += libdha/libdha.so vidix/libvidix.a
endif
ifeq ($(VO2),yes)
COMMON_DEPS += libvo2/libvo2.a
else
COMMON_DEPS += libvo/libvo.a
endif
ifeq ($(FAME),yes)
COMMON_DEPS += libfame/libfame.a
endif
ifeq ($(DVDKIT),yes)
ifeq ($(DVDKIT_SHARED),yes)
COMMON_DEPS += libmpdvdkit/libmpdvdkit.so
else
COMMON_DEPS += libmpdvdkit/libmpdvdkit.a
endif
endif

libmpdvdkit/libmpdvdkit.a:
	$(MAKE) -C libmpdvdkit

libmpdvdkit/libmpdvdkit.so:
	$(MAKE) -C libmpdvdkit libmpdvdkit.so

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

liba52/liba52.a:
	$(MAKE) -C liba52

mp3lib/libMP3.a:
	$(MAKE) -C mp3lib

libdha/libdha.so:
	$(MAKE) -C libdha

vidix/libvidix.a:
	$(MAKE) -C vidix

Gui/libgui.a:
	$(MAKE) -C Gui

linux/libosdep.a:
	$(MAKE) -C linux

postproc/libpostproc.a:
	$(MAKE) -C postproc

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
	./darwinfixlib.sh $(MPLAYER_DEP)
	$(CC) $(CFLAGS) -o $(PRG) $(OBJS_MPLAYER) $(VO_LIBS)  $(COMMON_LIBS) $(EXTRA_LIB) $(AO_LIBS) $(V_LIBS) $(VIDIX_LIBS) $(GUI_LIBS)  $(LIRC_LIB) $(STATIC_LIB) 

$(PRG_FIBMAP): fibmap_mplayer.o
	$(CC) -o $(PRG_FIBMAP) fibmap_mplayer.o

ifeq ($(MENCODER),yes)
$(PRG_MENCODER): $(MENCODER_DEP)
	./darwinfixlib.sh $(MENCODER_DEP) libmpcodecs/libmpencoders.a
	$(CC) $(CFLAGS) -o $(PRG_MENCODER) $(OBJS_MENCODER) -Llibmpcodecs -lmpencoders $(COMMON_LIBS) $(EXTRA_LIB) $(ENCORE_LIB) $(MLIB_LIB)
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
	-ln -sf $(PRG) $(BINDIR)/gmplayer
endif
	if test ! -d $(MANDIR)/man1 ; then mkdir -p $(MANDIR)/man1; fi
	$(INSTALL) -c -m 644 DOCS/mplayer.1 $(MANDIR)/man1/mplayer.1
ifeq ($(MENCODER),yes)
	$(INSTALL) -m 755 -s $(PRG_MENCODER) $(BINDIR)/$(PRG_MENCODER)
	-ln -sf mplayer.1 $(MANDIR)/man1/mencoder.1
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
ifeq ($(DVDKIT),yes)
ifeq ($(DVDKIT_SHARED),yes)
	if test ! -d $(LIBDIR) ; then mkdir -p $(LIBDIR) ; fi
	$(INSTALL) -m 755 -s libmpdvdkit/libmpdvdkit.so $(LIBDIR)/libmpdvdkit.so
endif
endif
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
	-rm -f *~ $(PRG) $(PRG_FIBMAP) $(PRG_MENCODER) $(OBJS)
	-rm -f *.o *.a .depend configure.log
	@for a in $(PARTS); do $(MAKE) -C $$a distclean; done

dep:	depend

depend:
	./version.sh `$(CC) --version`
	$(CC) -MM $(CFLAGS) mplayer.c mencoder.c $(SRCS_MPLAYER) $(SRCS_MENCODER) 1>.depend
	@for a in $(PARTS); do $(MAKE) -C $$a dep; done

# do not rebuild after cvs commits if .developer file is present!

# rebuild at every config.h/config.mak change:
version.h:
	./version.sh `$(CC) --version`
ifeq ($(wildcard .developer),)
	$(MAKE) distclean
endif
	$(MAKE) depend

# rebuild at every CVS update or config/makefile change:
ifeq ($(wildcard .developer),)
ifneq ($(wildcard CVS/Entries),)
version.h: CVS/Entries
endif
version.h: config.h config.mak Makefile
endif

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
