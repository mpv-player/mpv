# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected
#			* DVD support

include config.mak

LIBAV_INC =
ifeq ($(CONFIG_LIBAVUTIL),yes)
LIBAV_INC += -I./libavutil
endif
ifeq ($(CONFIG_LIBAVCODEC),yes)
LIBAV_INC += -I./libavcodec
endif

# Do not strip the binaries at installation
ifeq ($(STRIPBINARIES),yes)
INSTALLSTRIP = -s
endif

SRCS_COMMON = asxparser.c \
              codec-cfg.c \
              cpudetect.c \
              edl.c \
              find_sub.c \
              m_config.c \
              m_option.c \
              m_struct.c \
              parser-cfg.c \
              playtree.c \
              playtreeparser.c \
              spudec.c \
              sub_cc.c \
              subreader.c \
              vobsub.c \

SRCS_MENCODER = mencoder.c \
                mp_msg-mencoder.c \
                $(SRCS_COMMON) \
                libvo/aclib.c \
                libvo/font_load.c \
                libvo/osd.c \
                libvo/sub.c \
                parser-mecmd.c \
                xvid_vbr.c \

SRCS_MPLAYER = mplayer.c \
               m_property.c \
               mp_msg.c \
               $(SRCS_COMMON) \
               mixer.c \
               parser-mpcmd.c \
               subopt-helper.c \

ifeq ($(UNRARLIB),yes)
SRCS_COMMON += unrarlib.c
endif

OBJS_MENCODER = $(SRCS_MENCODER:.c=.o)
OBJS_MPLAYER = $(SRCS_MPLAYER:.c=.o)

VO_LIBS = $(AA_LIB) \
          $(X_LIB) \
          $(SDL_LIB) \
          $(GGI_LIB) \
          $(MLIB_LIB) \
          $(SVGA_LIB) \
          $(DIRECTFB_LIB) \
          $(CACA_LIB) \
	  $(VESA_LIB) \

ifeq ($(EXTERNAL_VIDIX),yes)
VO_LIBS += $(EXTERNAL_VIDIX_LIB)
endif

AO_LIBS = $(ARTS_LIB) \
          $(ESD_LIB) \
          $(JACK_LIB) \
          $(OPENAL_LIB) \
          $(NAS_LIB) \
          $(SGIAUDIO_LIB) \
          $(POLYP_LIB) \

CODEC_LIBS = $(AV_LIB) \
             $(FAME_LIB) \
             $(MAD_LIB) \
             $(VORBIS_LIB) \
             $(THEORA_LIB) \
             $(FAAD_LIB) \
             $(LIBLZO_LIB) \
             $(DECORE_LIB) \
             $(XVID_LIB) \
             $(DTS_LIB) \
             $(PNG_LIB) \
             $(Z_LIB) \
             $(JPEG_LIB) \
             $(ALSA_LIB) \
             $(XMMS_LIB) \
             $(X264_LIB) \
             $(MUSEPACK_LIB) \
             $(SPEEX_LIB) \

COMMON_LIBS = libmpcodecs/libmpcodecs.a \
              $(W32_LIB) \
              libaf/libaf.a \
              libmpdemux/libmpdemux.a \
              libswscale/libswscale.a \
              osdep/libosdep.a \
              $(DVDREAD_LIB) \
              $(DVDNAV_LIB) \
              $(CODEC_LIBS) \
              $(TERMCAP_LIB) \
              $(CDPARANOIA_LIB) \
              $(MPLAYER_NETWORK_LIB) \
              $(LIBCDIO_LIB) \
              $(WIN32_LIB) \
              $(GIF_LIB) \
              $(MACOSX_FRAMEWORKS) \
              $(SMBSUPPORT_LIB) \
              $(FRIBIDI_LIB) \
              $(ENCA_LIB) \

CFLAGS = $(OPTFLAGS) -I. $(LIBAV_INC)

#CFLAGS += -Wall

ifeq ($(TOOLAME),yes)
CFLAGS += $(TOOLAME_EXTRAFLAGS) 
CODEC_LIBS += $(TOOLAME_LIB)
endif

ifeq ($(TWOLAME),yes)
CODEC_LIBS += $(TWOLAME_LIB)
endif

ifeq ($(FAAC),yes)
CODEC_LIBS += $(FAAC_LIB)
endif

PARTS = libmpdemux \
        libmpcodecs \
        libavutil \
        libavcodec \
        libpostproc \
        libavformat \
        libswscale \
        libao2 \
        osdep \
        input \
        libvo \
        libaf \

ifeq ($(MP3LIB),yes)
PARTS += mp3lib
endif
ifeq ($(LIBA52),yes)
PARTS += liba52
endif
ifeq ($(LIBMPEG2),yes)
PARTS += libmpeg2
endif
ifeq ($(INTERNAL_FAAD),yes)
COMMON_LIBS += libfaad2/libfaad2.a 
PARTS += libfaad2
endif
ifeq ($(VIDIX),yes)
PARTS += libdha vidix
endif
ifeq ($(FAME),yes)
PARTS += libfame
endif
ifeq ($(DVDKIT2),yes)
PARTS += libmpdvdkit2
else
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
ifeq ($(TREMOR),yes)
PARTS += tremor
endif

ALL_PRG = $(PRG)
ifeq ($(MENCODER),yes)
ALL_PRG += $(PRG_MENCODER)
endif

COMMON_DEPS = $(W32_DEP) \
              $(AV_DEP) \
              libmpdemux/libmpdemux.a \
              libmpcodecs/libmpcodecs.a \
              libao2/libao2.a \
              osdep/libosdep.a \
              libswscale/libswscale.a \
              input/libinput.a \
              libvo/libvo.a \
              libaf/libaf.a \

ifeq ($(MP3LIB),yes)
COMMON_DEPS += mp3lib/libMP3.a
COMMON_LIBS += mp3lib/libMP3.a
endif
ifeq ($(LIBA52),yes)
COMMON_DEPS += liba52/liba52.a
COMMON_LIBS += liba52/liba52.a
endif
ifeq ($(LIBMPEG2),yes)
COMMON_DEPS += libmpeg2/libmpeg2.a
COMMON_LIBS += libmpeg2/libmpeg2.a
endif
ifeq ($(INTERNAL_FAAD),yes)
COMMON_DEPS += libfaad2/libfaad2.a
endif
ifeq ($(TREMOR),yes)
COMMON_DEPS += tremor/libvorbisidec.a
COMMON_LIBS += tremor/libvorbisidec.a
endif
ifeq ($(VIDIX),yes)
COMMON_DEPS += libdha/libdha.so vidix/libvidix.a
endif
ifeq ($(FAME),yes)
COMMON_DEPS += libfame/libfame.a
endif
ifeq ($(DVDKIT2),yes)
COMMON_DEPS += libmpdvdkit2/libmpdvdkit.a
endif
ifeq ($(CONFIG_ASS),yes)
COMMON_DEPS += libass/libass.a
COMMON_LIBS += libass/libass.a
PARTS += libass
endif
# FontConfig and FreeType need to come after ASS to avoid link failures on MinGW
COMMON_LIBS += $(FONTCONFIG_LIB)
ifeq ($(FREETYPE),yes)
SRCS_MENCODER += libvo/font_load_ft.c
COMMON_LIBS += $(FREETYPE_LIB)
endif
ifeq ($(GUI),yes)
COMMON_DEPS += Gui/libgui.a
GUI_LIBS = Gui/libgui.a $(GTK_LIBS)
endif

.SUFFIXES: .cc .c .o

#.PHONY: $(COMMON_DEPS)

all:	$(ALL_PRG)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

libaf/libaf.a:
	$(MAKE) -C libaf

libmpdvdkit2/libmpdvdkit.a:
	$(MAKE) -C libmpdvdkit2

loader/libloader.a:
	$(MAKE) -C loader

libfame/libfame.a:
	$(MAKE) -C libfame

libass/libass.a:
	$(MAKE) -C libass

libmpdemux/libmpdemux.a:
	$(MAKE) -C libmpdemux

libmpcodecs/libmpcodecs.a:
	$(MAKE) -C libmpcodecs

loader/dshow/libDS_Filter.a:
	$(MAKE) -C loader/dshow

loader/dmo/libDMO_Filter.a:
	$(MAKE) -C loader/dmo

libavutil/libavutil.a:
	$(MAKE) -C libavutil LIBPREF=lib LIBSUF=.a

libavcodec/libavcodec.a:
	$(MAKE) -C libavcodec LIBPREF=lib LIBSUF=.a

libpostproc/libpostproc.a:
	$(MAKE) -C libpostproc LIBPREF=lib LIBSUF=.a

libavformat/libavformat.a:
	$(MAKE) -C libavformat LIBPREF=lib LIBSUF=.a

libswscale/libswscale.a:
	$(MAKE) -C libswscale LIBPREF=lib LIBSUF=.a

libmpeg2/libmpeg2.a:
	$(MAKE) -C libmpeg2

libvo/libvo.a:
	$(MAKE) -C libvo

libao2/libao2.a:
	$(MAKE) -C libao2

liba52/liba52.a:
	$(MAKE) -C liba52

libfaad2/libfaad2.a:
	$(MAKE) -C libfaad2

mp3lib/libMP3.a:
	$(MAKE) -C mp3lib

tremor/libvorbisidec.a:
	$(MAKE) -C tremor

libdha/libdha.so:
	$(MAKE) -C libdha

vidix/libvidix.a: libdha/libdha.so
	$(MAKE) -C vidix

Gui/libgui.a:
	$(MAKE) -C Gui

osdep/libosdep.a:
	$(MAKE) -C osdep

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

MENCODER_DEP = $(OBJS_MENCODER) $(COMMON_DEPS) libmpcodecs/libmpencoders.a

ifeq ($(VIDIX),yes)
VIDIX_LIBS = vidix/libvidix.a
else
VIDIX_LIBS =
endif

ifeq ($(TARGET_WIN32),yes)
OBJS_MPLAYER += osdep/mplayer-rc.o
endif

LIBS_MPLAYER = libvo/libvo.a \
               libao2/libao2.a \
               input/libinput.a \
               $(MENU_LIBS) \
               $(VIDIX_LIBS) \
               $(GUI_LIBS) \
               $(COMMON_LIBS) \
               $(VO_LIBS) \
               $(AO_LIBS) \
               $(EXTRA_LIB)\
               $(LIRC_LIB) \
               $(LIRCC_LIB) \
               $(STATIC_LIB) \
               $(ARCH_LIB) \
               $(MATH_LIB) \
               $(LIBC_LIB) \

$(PRG):	$(MPLAYER_DEP)
    ifeq ($(TARGET_WIN32),yes)
	windres -o osdep/mplayer-rc.o osdep/mplayer.rc
    endif
	$(CC) $(CFLAGS) -o $(PRG) $(OBJS_MPLAYER) $(LIBS_MPLAYER)

ifeq ($(MENCODER),yes)
LIBS_MENCODER = libmpcodecs/libmpencoders.a \
                $(ENCORE_LIB) \
                $(COMMON_LIBS) \
                $(EXTRA_LIB) \
                $(MLIB_LIB) \
                $(LIRC_LIB) \
                $(LIRCC_LIB) \
                $(ARCH_LIB) \
                $(MATH_LIB) \
                $(LIBC_LIB) \

$(PRG_MENCODER): $(MENCODER_DEP)
	$(CC) $(CFLAGS) -o $(PRG_MENCODER) $(OBJS_MENCODER) $(LIBS_MENCODER)
endif

codec-cfg: codec-cfg.c codec-cfg.h help_mp.h
	$(HOST_CC) -I. -DCODECS2HTML codec-cfg.c -o $@

codecs.conf.h: codec-cfg etc/codecs.conf
	./codec-cfg ./etc/codecs.conf > $@

codec-cfg.o: codecs.conf.h

codecs2html: mp_msg.o
	$(CC) -DCODECS2HTML codec-cfg.c mp_msg.o -o $@

install: $(ALL_PRG)
ifeq ($(VIDIX),yes)
	$(MAKE) -C libdha install
	$(MAKE) -C vidix install
endif
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 755 $(INSTALLSTRIP) $(PRG) $(BINDIR)/$(PRG)
ifeq ($(GUI),yes)
	-ln -sf $(PRG) $(BINDIR)/gmplayer
endif
	$(INSTALL) -d $(MANDIR)/man1
	for i in $(MAN_LANG); do \
		if test "$$i" = en ; then \
			$(INSTALL) -c -m 644 DOCS/man/en/mplayer.1 $(MANDIR)/man1/mplayer.1 ; \
		else \
			$(INSTALL) -d $(MANDIR)/$$i/man1 ; \
			$(INSTALL) -c -m 644 DOCS/man/$$i/mplayer.1 $(MANDIR)/$$i/man1/mplayer.1 ; \
		fi ; \
	done
ifeq ($(MENCODER),yes)
	$(INSTALL) -m 755 $(INSTALLSTRIP) $(PRG_MENCODER) $(BINDIR)/$(PRG_MENCODER)
	for i in $(MAN_LANG); do \
		if test "$$i" = en ; then \
			ln -sf mplayer.1 $(MANDIR)/man1/mencoder.1 ; \
		else \
			ln -sf mplayer.1 $(MANDIR)/$$i/man1/mencoder.1 ; \
		fi ; \
	done
endif
	@$(INSTALL) -d $(DATADIR)
	@$(INSTALL) -d $(DATADIR)/font
	@if test ! -f $(DATADIR)/font/font.desc ; then \
	echo "*** Download font at http://www.mplayerhq.hu/dload.html" ; \
	echo "*** for OSD/Subtitles support and extract to $(DATADIR)/font/" ; \
	fi
ifeq ($(GUI),yes)
	@$(INSTALL) -d $(DATADIR)/skins
	@echo "*** Download skin(s) at http://www.mplayerhq.hu/dload.html"
	@echo "*** for GUI, and extract to $(DATADIR)/skins/"
	@$(INSTALL) -d $(prefix)/share/pixmaps
	$(INSTALL) -m 644 etc/mplayer.xpm $(prefix)/share/pixmaps/mplayer.xpm
	@$(INSTALL) -d $(prefix)/share/applications
	$(INSTALL) -m 644 etc/mplayer.desktop $(prefix)/share/applications/mplayer.desktop
endif
	@$(INSTALL) -d $(CONFDIR)
	@if test -f $(CONFDIR)/codecs.conf ; then mv -f $(CONFDIR)/codecs.conf $(CONFDIR)/codecs.conf.old ; fi

uninstall:
	-rm -f $(BINDIR)/$(PRG) $(BINDIR)/gmplayer $(MANDIR)/man1/mplayer.1
	-rm -f  $(BINDIR)/$(PRG_MENCODER) $(MANDIR)/man1/mencoder.1
	-rm -f $(prefix)/share/pixmaps/mplayer.xpm
	-rm -f $(prefix)/share/applications/mplayer.desktop
	for l in $(MAN_LANG); do \
	  if test "$$l" != "en"; then \
	    rm -f $(MANDIR)/$$l/man1/mplayer.1    \
	          $(MANDIR)/$$l/man1/mencoder.1   \
	          $(MANDIR)/$$l/man1/gmplayer.1 ; \
	  fi ; \
	done
ifeq ($(VIDIX),yes)
	$(MAKE) -C libdha uninstall
	$(MAKE) -C vidix uninstall
endif
	@echo "Uninstall completed"

clean:
	-rm -f *.o *.a *~

distclean: clean doxygen_clean
	-rm -f *~ $(PRG) $(PRG_MENCODER) codec-cfg codecs2html
	-rm -f .depend configure.log codecs.conf.h help_mp.h
	@for a in $(PARTS); do $(MAKE) -C $$a distclean; done

strip:
	strip -s $(ALL_PRG)

dep:	depend

depend: help_mp.h version.h
	$(CC) -MM $(CFLAGS) -DCODECS2HTML mplayer.c mencoder.c $(SRCS_MPLAYER) $(SRCS_MENCODER) 1>.depend
	@for a in $(PARTS); do $(MAKE) -C $$a dep; done

# ./configure must be rerun if it changed
config.h: configure
	@echo "############################################################"
	@echo "####### Please run ./configure again - it's changed! #######"
	@echo "############################################################"

# rebuild at every config.h/config.mak/Makefile change:
version.h: config.h config.mak Makefile
	./version.sh `$(CC) -dumpversion`

doxygen:
	doxygen DOCS/tech/Doxyfile

doxygen_clean:
	-rm -rf DOCS/tech/doxygen

help_mp.h: help/help_mp-en.h $(HELP_FILE)
	@echo '// WARNING! This is a generated file. Do NOT edit.' > help_mp.h
	@echo '// See the help/ subdir for the editable files.' >> help_mp.h
ifeq ($(CHARSET),)
	@echo '#include "$(HELP_FILE)"' >> help_mp.h
else
	iconv -f `cat $(HELP_FILE).charset` -t $(CHARSET) "$(HELP_FILE)" >> help_mp.h
endif

ifneq ($(HELP_FILE),help/help_mp-en.h)
	@echo "Adding untranslated messages to help_mp.h"
	@echo '// untranslated messages from the English master file:' >> help_mp.h
	@help/help_diff.sh $(HELP_FILE) < help/help_mp-en.h >> help_mp.h
endif

# explicit dependencies to force version.h to be built even if .depend is missing
mplayer.o mencoder.o vobsub.o: version.h

# temporary measure to make sure help_mp.h is built. we desperately need correct deps!
$(MPLAYER_DEP) $(MENCODER_DEP): help_mp.h

#
# the following lines provide _partial_ dependency information
# for the 'library' directories under main dir, in order to cause
# the build process to recursively descend into them if something
# has changed. ideally this will be replaced with a single
# nonrecursive makefile for the whole project.
#

libvo/libvo.a: $(wildcard libvo/*.[ch])
libao2/libao2.a: $(wildcard libao2/*.[ch])
osdep/libosdep.a: $(wildcard osdep/*.[ch])
input/libinput.a: $(wildcard input/*.[ch])

libmenu/libmenu.a: $(wildcard libmenu/*.[ch])
libaf/libaf.a: $(wildcard libaf/*.[ch])
libmpdvdkit2/libmpdvdkit.a: $(wildcard libmpdvdkit2/*.[ch])

libmpdemux/libmpdemux.a: $(wildcard libmpdemux/*.[ch] libmpdemux/*/*.[ch])
libmpcodecs/libmpcodecs.a: $(wildcard libmpcodecs/*.[ch]) $(wildcard libmpcodecs/native/*.[ch])
libmpcodecs/libmpencoders.a: $(wildcard libmpcodecs/*.[ch])

libavutil/libavutil.a: $(wildcard libavutil/*.[ch])
libavcodec/libavcodec.a: $(wildcard libavcodec/*.[ch] libavcodec/*/*.[chS])
libavformat/libavformat.a: $(wildcard libavformat/*.[ch])
libswscale/libswscale.a: $(wildcard libswscale/*.[ch])

libmpeg2/libmpeg2.a: $(wildcard libmpeg2/*.[ch])
liba52/liba52.a: $(wildcard liba52/*.[ch])
mp3lib/libMP3.a: $(wildcard mp3lib/*.[ch])
libfaad2/libfaad2.a: $(wildcard libfaad2/*.[ch] libfaad2/*/*.[ch])

loader/libloader.a: $(wildcard loader/*.[chSs])
loader/dmo/libDMO_Filter.a: $(wildcard loader/dmo/*.[ch])
loader/dshow/libDS_Filter.a: $(wildcard loader/dshow/*.[ch])

libdha/libdha.so: $(wildcard libdha/*.[ch])
vidix/libvidix.a: $(wildcard vidix/*.[ch])
Gui/libgui.a: $(wildcard Gui/*.[ch] Gui/*/*.[ch] Gui/*/*/*.[ch])

libass/libass.a: $(wildcard libass/*.[ch])

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
