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

# Do not strip the binaries at installation
ifeq ($(STRIPBINARIES),yes)
INSTALLSTRIP = -s
endif

# These subdirectories require installation due to binaries within them.
ifeq ($(VIDIX),yes)
SUBDIRS += libdha vidix
DO_MAKE = @ for i in $(SUBDIRS); do $(MAKE) -C $$i $@; done
endif

SRCS_COMMON = cpudetect.c codec-cfg.c cfgparser.c my_profile.c spudec.c playtree.c playtreeparser.c asxparser.c vobsub.c subreader.c sub_cc.c find_sub.c m_config.c m_option.c parser-cfg.c m_struct.c
SRCS_MENCODER = mencoder.c mp_msg-mencoder.c $(SRCS_COMMON) libao2/afmt.c divx4_vbr.c libvo/aclib.c libvo/osd.c libvo/sub.c libvo/font_load.c libvo/font_load_ft.c xvid_vbr.c parser-mecmd.c
SRCS_MPLAYER = mplayer.c mp_msg.c $(SRCS_COMMON) mixer.c parser-mpcmd.c

ifeq ($(UNRARLIB),yes)
SRCS_COMMON += unrarlib.c
endif

OBJS_MENCODER = $(SRCS_MENCODER:.c=.o)
OBJS_MPLAYER = $(SRCS_MPLAYER:.c=.o)

VO_LIBS = $(AA_LIB) $(X_LIB) $(SDL_LIB) $(GGI_LIB) $(MP1E_LIB) $(MLIB_LIB) $(SVGA_LIB) $(DIRECTFB_LIB) 
AO_LIBS = $(ARTS_LIB) $(ESD_LIB) $(NAS_LIB) $(SGIAUDIO_LIB)
CODEC_LIBS = $(AV_LIB) $(FAME_LIB) $(MAD_LIB) $(VORBIS_LIB) $(FAAD_LIB) $(LIBLZO_LIB) $(DECORE_LIB) $(XVID_LIB) $(PNG_LIB) $(Z_LIB) $(JPEG_LIB) $(ALSA_LIB) $(XMMS_LIB)
COMMON_LIBS = libmpcodecs/libmpcodecs.a mp3lib/libMP3.a liba52/liba52.a libmpeg2/libmpeg2.a $(W32_LIB) $(DS_LIB) libaf/libaf.a libmpdemux/libmpdemux.a input/libinput.a $(PP_LIB) postproc/libswscale.a linux/libosdep.a $(CSS_LIB) $(CODEC_LIBS) $(FREETYPE_LIB) $(TERMCAP_LIB) $(CDPARANOIA_LIB) $(STREAMING_LIB) $(WIN32_LIB) $(GIF_LIB)

CFLAGS = $(OPTFLAGS) -Ilibmpdemux -Iloader -Ilibvo $(FREETYPE_INC) $(EXTRA_INC) $(CDPARANOIA_INC) $(SDL_INC) # -Wall

ifeq ($(TARGET_ALTIVEC),yes)
ifeq ($(TARGET_OS),Darwin)
CFLAGS += -faltivec
else
CFLAGS += -maltivec -mabi=altivec
endif
endif

PARTS = libmpdemux libmpcodecs mp3lib liba52 libmpeg2 libavcodec libao2 drivers linux postproc input libvo libaf
ifeq ($(VIDIX),yes)
PARTS += libdha vidix
endif
ifeq ($(FAME),yes)
PARTS += libfame
endif
ifeq ($(DVDKIT2),yes)
PARTS += libmpdvdkit2
else
ifeq ($(DVDKIT),yes)
PARTS += libmpdvdkit
endif
endif
ifeq ($(GUI),yes)
PARTS += Gui
endif
ifneq ($(W32_LIB),)
PARTS += loader loader/dshow loader/dmo
endif
ifeq ($(LIBMENU),yes)
PARTS += libmenu
endif

ALL_PRG = $(PRG)
ifeq ($(MENCODER),yes)
ALL_PRG += $(PRG_MENCODER)
endif
ifeq ($(CSS_USE),yes)
ALL_PRG += $(PRG_FIBMAP)
endif

COMMON_DEPS = $(W32_DEP) $(DS_DEP) $(MP1E_DEP) $(AV_DEP) libmpdemux/libmpdemux.a libmpcodecs/libmpcodecs.a libao2/libao2.a liba52/liba52.a mp3lib/libMP3.a libmpeg2/libmpeg2.a linux/libosdep.a postproc/libswscale.a input/libinput.a libvo/libvo.a libaf/libaf.a
ifeq (($SHARED_PP),yes)
COMMON_DEPS += postproc/libpostproc.so
else
COMMON_DEPS += postproc/libpostproc.a
endif

ifeq ($(VIDIX),yes)
COMMON_DEPS += libdha/libdha.so vidix/libvidix.a
endif
ifeq ($(FAME),yes)
COMMON_DEPS += libfame/libfame.a
endif
ifeq ($(DVDKIT2),yes)
ifeq ($(DVDKIT_SHARED),yes)
COMMON_DEPS += libmpdvdkit2/libmpdvdkit.so
else
COMMON_DEPS += libmpdvdkit2/libmpdvdkit.a
endif
endif

ifeq ($(GUI),yes)
COMMON_DEPS += Gui/libgui.a
GUI_LIBS = Gui/libgui.a
endif


.SUFFIXES: .cc .c .o

# .PHONY: $(COMMON_DEPS)

all:	version.h $(ALL_PRG)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

libaf/libaf.a:
	$(MAKE) -C libaf

libmpdvdkit2/libmpdvdkit.a:
	$(MAKE) -C libmpdvdkit2

libmpdvdkit2/libmpdvdkit.so:
	$(MAKE) -C libmpdvdkit2 libmpdvdkit.so

loader/libloader.a:
	$(MAKE) -C loader

libfame/libfame.a:
	$(MAKE) -C libfame

libmpdemux/libmpdemux.a:
	$(MAKE) -C libmpdemux

libmpcodecs/libmpcodecs.a:
	$(MAKE) -C libmpcodecs

loader/dshow/libDS_Filter.a:
	$(MAKE) -C loader/dshow

loader/dmo/libDMO_Filter.a:
	$(MAKE) -C loader/dmo

libavcodec/libavcodec.a:
	$(MAKE) -C libavcodec LIBPREF=lib LIBSUF=.a

libmpeg2/libmpeg2.a:
	$(MAKE) -C libmpeg2

libvo/libvo.a:
	$(MAKE) -C libvo

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

postproc/libswscale.a:
	$(MAKE) -C postproc

postproc/libpostproc.a:
	$(MAKE) -C postproc

postproc/libpostproc.so:
	$(MAKE) -C postproc

input/libinput.a:
	$(MAKE) -C input

libmenu/libmenu.a:
	$(MAKE) -C libmenu

MPLAYER_DEP = $(OBJS_MPLAYER) $(COMMON_DEPS)

ifeq ($(LIBMENU),yes)
MPLAYER_DEP += libmenu/libmenu.a
MENU_LIBS = libmenu/libmenu.a
PARTS += libmenu
else
MENU_LIBS =
endif

MENCODER_DEP = $(OBJS_MENCODER) $(COMMON_DEPS)

ifeq ($(VIDIX),yes)
VIDIX_LIBS = vidix/libvidix.a
else
VIDIX_LIBS =
endif

$(PRG):	$(MPLAYER_DEP)
	./darwinfixlib.sh $(MPLAYER_DEP)
	$(CC) $(CFLAGS) -o $(PRG) $(OBJS_MPLAYER) libvo/libvo.a libao2/libao2.a $(MENU_LIBS) $(VIDIX_LIBS) $(GUI_LIBS) $(COMMON_LIBS) $(GTK_LIBS) $(VO_LIBS) $(AO_LIBS) $(EXTRA_LIB) $(LIRC_LIB) $(STATIC_LIB) $(ARCH_LIB) $(I18NLIBS) -lm

mplayer.exe.spec.c: libmpcodecs/libmpcodecs.a
	winebuild -fPIC -o mplayer.exe.spec.c -exe mplayer.exe -mcui \
	libmpcodecs/ad_qtaudio.o libmpcodecs/vd_qtvideo.o \
	-L/usr/local/lib/wine -lkernel32

mplayer.exe.so:	$(MPLAYER_DEP) mplayer.exe.spec.c
	$(CC) $(CFLAGS) -Wall -shared  -Wl,-rpath,/usr/local/lib -Wl,-Bsymbolic  -o mplayer.exe.so $(OBJS_MPLAYER) mplayer.exe.spec.c libvo/libvo.a libao2/libao2.a $(MENU_LIBS) $(VIDIX_LIBS) $(GUI_LIBS) $(COMMON_LIBS) $(GTK_LIBS) $(VO_LIBS) $(AO_LIBS) $(EXTRA_LIB) $(LIRC_LIB) $(STATIC_LIB) $(ARCH_LIB) -lwine -lm 

mplayer_wine.so:	$(MPLAYER_DEP)
	./darwinfixlib.sh $(MPLAYER_DEP)
	$(CC) $(CFLAGS) -shared -Wl,-Bsymbolic -o mplayer_wine.so mplayer_wine.spec.c $(OBJS_MPLAYER) libvo/libvo.a libao2/libao2.a $(MENU_LIBS) $(VIDIX_LIBS) $(GUI_LIBS) $(COMMON_LIBS) $(GTK_LIBS) $(VO_LIBS) $(AO_LIBS) $(EXTRA_LIB) $(LIRC_LIB) $(STATIC_LIB) -lwine $(ARCH_LIB) -lm

$(PRG_FIBMAP): fibmap_mplayer.o
	$(CC) -o $(PRG_FIBMAP) fibmap_mplayer.o

ifeq ($(MENCODER),yes)
$(PRG_MENCODER): $(MENCODER_DEP)
	./darwinfixlib.sh $(MENCODER_DEP) libmpcodecs/libmpencoders.a
	$(CC) $(CFLAGS) -o $(PRG_MENCODER) $(OBJS_MENCODER) libmpcodecs/libmpencoders.a $(ENCORE_LIB) $(COMMON_LIBS) $(EXTRA_LIB) $(MLIB_LIB) $(LIRC_LIB) $(ARCH_LIB) $(I18NLIBS) -lm 
endif

codecs.conf.h: $(PRG_CFG)
	./$(PRG_CFG) ./etc/codecs.conf > $@

codec-cfg.o: codecs.conf.h

# Every mplayer dependency depends on version.h, to force building version.h
# first (in serial mode) before any other of the dependencies for a parallel make
# run.  This is necessary, because the make rule for version.h removes objects
# in a recursive "make distclean" and we must wait for this "make distclean" to
# finish before we can start building new object files.
$(MPLAYER_DEP): version.h
$(MENCODER_DEP): version.h

$(PRG_CFG): version.h codec-cfg.c codec-cfg.h
	$(CC) $(CFLAGS) -g codec-cfg.c mp_msg.c -o $(PRG_CFG) -DCODECS2HTML $(I18NLIBS)

install: $(ALL_PRG)
ifeq ($(VIDIX),yes)
	$(DO_MAKE)
endif
ifeq ($(SHARED_PP),yes)
	$(MAKE) install -C postproc 
endif
	if test ! -d $(BINDIR) ; then mkdir -p $(BINDIR) ; fi
	$(INSTALL) -m 755 $(INSTALLSTRIP) $(PRG) $(BINDIR)/$(PRG)
ifeq ($(GUI),yes)
	-ln -sf $(PRG) $(BINDIR)/gmplayer
endif
	if test ! -d $(MANDIR)/man1 ; then mkdir -p $(MANDIR)/man1; fi
	$(INSTALL) -c -m 644 DOCS/mplayer.1 $(MANDIR)/man1/mplayer.1
ifeq ($(MENCODER),yes)
	$(INSTALL) -m 755 $(INSTALLSTRIP) $(PRG_MENCODER) $(BINDIR)/$(PRG_MENCODER)
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
ifeq ($(DVDKIT_SHARED),yes)
ifeq ($(DVDKIT2),yes)
	if test ! -d $(LIBDIR) ; then mkdir -p $(LIBDIR) ; fi
	$(INSTALL) -m 755 $(INSTALLSTRIP) libmpdvdkit2/libmpdvdkit.so $(LIBDIR)/libmpdvdkit.so
else
ifeq ($(DVDKIT),yes)
	if test ! -d $(LIBDIR) ; then mkdir -p $(LIBDIR) ; fi
	$(INSTALL) -m 755 $(INSTALLSTRIP) libmpdvdkit/libmpdvdkit.so $(LIBDIR)/libmpdvdkit.so
endif
endif
endif
ifeq ($(CSS_USE),yes)
	@echo "The following task requires root privileges. If it fails don't panic,"
	@echo "however it means you can't use fibmap_mplayer."
	@echo "Without this (or without running mplayer as root) you won't be"
	@echo "able to play encrypted DVDs."
	-$(INSTALL) -o 0 -g 0 -m 4755 $(INSTALLSTRIP) $(PRG_FIBMAP) $(BINDIR)/$(PRG_FIBMAP)
endif

uninstall:
	-rm -f $(BINDIR)/$(PRG) $(BINDIR)/gmplayer $(MANDIR)/man1/mplayer.1
	-rm -f $(BINDIR)/$(PRG_FIBMAP)
	-rm -f  $(BINDIR)/$(PRG_MENCODER) $(MANDIR)/man1/mencoder.1
	@echo "Uninstall completed"

clean:
	-rm -f *.o *~ $(OBJS) codecs.conf.h

distclean:
	-rm -f *~ $(PRG) $(PRG_FIBMAP) $(PRG_MENCODER) $(PRG_CFG) $(OBJS)
	-rm -f *.o *.a .depend configure.log codecs.conf.h
	@for a in $(PARTS); do $(MAKE) -C $$a distclean; done

strip:
	strip -s $(ALL_PRG)

dep:	depend

depend:
	./version.sh `$(CC) -dumpversion`
	$(CC) -MM $(CFLAGS) -DCODECS2HTML mplayer.c mencoder.c $(SRCS_MPLAYER) $(SRCS_MENCODER) 1>.depend
	@for a in $(PARTS); do $(MAKE) -C $$a dep; done

# ./configure must be run if it changed in CVS
config.h: configure
	@echo "############################################################"
	@echo "####### Please run ./configure again - it's changed! #######"
	@echo "############################################################"
ifeq ($(wildcard .developer),)
	@exit 1
endif

# do not rebuild after cvs commits if .developer file is present!

# rebuild at every config.h/config.mak change:
version.h:
	./version.sh `$(CC) -dumpversion`
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
# include dependencies to get make to recurse into lib dirs,
# if the user desires such behavior
#
ifneq ($(wildcard .libdeps),)
include .libdeps
endif

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
