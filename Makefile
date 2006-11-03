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

CFLAGS = $(OPTFLAGS) -I. $(LIBAV_INC)

#CFLAGS += -Wall

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

ifeq ($(UNRARLIB),yes)
SRCS_COMMON += unrarlib.c
endif

SRCS_MENCODER = mencoder.c \
                mp_msg-mencoder.c \
                $(SRCS_COMMON) \
                libvo/aclib.c \
                libvo/osd.c \
                libvo/sub.c \
                parser-mecmd.c \
                xvid_vbr.c \

ifeq ($(BITMAP_FONT),yes)
SRCS_MENCODER += libvo/font_load.c
endif

SRCS_MPLAYER = mplayer.c \
               m_property.c \
               mp_msg.c \
               $(SRCS_COMMON) \
               mixer.c \
               parser-mpcmd.c \
               subopt-helper.c \

OBJS_MENCODER = $(SRCS_MENCODER:.c=.o)
OBJS_MPLAYER = $(SRCS_MPLAYER:.c=.o)

ifeq ($(VIDIX),yes)
VO_LIBS += vidix/libvidix.a
endif

COMMON_LIBS = libmpcodecs/libmpcodecs.a \
              libaf/libaf.a \
              libmpdemux/libmpdemux.a \
              stream/stream.a \
              libswscale/libswscale.a \
              osdep/libosdep.a \
              $(AV_LIB) \
              $(EXTRA_LIB)\
              $(EXTRALIBS) \

PARTS = libmpdemux \
        stream \
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
ifeq ($(FAAD_INTERNAL),yes)
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
PARTS += libdvdcss
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
ifeq ($(TREMOR_INTERNAL),yes)
PARTS += tremor
endif

ALL_PRG = $(PRG)
ifeq ($(MENCODER),yes)
ALL_PRG += $(PRG_MENCODER)
endif

COMMON_DEPS = $(W32_DEP) \
              $(AV_DEP) \
              libmpdemux/libmpdemux.a \
              stream/stream.a \
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
ifeq ($(FAAD_INTERNAL),yes)
COMMON_DEPS += libfaad2/libfaad2.a
endif
ifeq ($(TREMOR_INTERNAL),yes)
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
COMMON_LIBS += libmpdvdkit2/libmpdvdkit.a
COMMON_DEPS += libdvdcss/libdvdcss.a
COMMON_LIBS += libdvdcss/libdvdcss.a
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

libdvdcss/libdvdcss.a:
	$(MAKE) -C libdvdcss

loader/libloader.a:
	$(MAKE) -C loader

libfame/libfame.a:
	$(MAKE) -C libfame

libass/libass.a:
	$(MAKE) -C libass

libmpdemux/libmpdemux.a:
	$(MAKE) -C libmpdemux

stream/stream.a:
	$(MAKE) -C stream

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
endif

MENCODER_DEP = $(OBJS_MENCODER) $(COMMON_DEPS) libmpcodecs/libmpencoders.a

ifeq ($(TARGET_WIN32),yes)
OBJS_MPLAYER += osdep/mplayer-rc.o
endif

LIBS_MPLAYER = libvo/libvo.a \
               libao2/libao2.a \
               input/libinput.a \
               $(MENU_LIBS) \
               $(GUI_LIBS) \
               $(COMMON_LIBS) \
               $(VO_LIBS) \
               $(AO_LIBS) \

$(PRG):	$(MPLAYER_DEP)
	$(CC) -o $(PRG) $(OBJS_MPLAYER) $(LIBS_MPLAYER)

ifeq ($(MENCODER),yes)
LIBS_MENCODER = libmpcodecs/libmpencoders.a \
                $(MP3LAME_LIB) \
                $(COMMON_LIBS) \

$(PRG_MENCODER): $(MENCODER_DEP)
	$(CC) -o $(PRG_MENCODER) $(OBJS_MENCODER) $(LIBS_MENCODER)
endif

osdep/mplayer-rc.o: osdep/mplayer.rc
	windres -o $@ osdep/mplayer.rc

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
			cd $(MANDIR)/man1 && ln -sf mplayer.1 mencoder.1 ; \
		else \
			cd $(MANDIR)/$$i/man1 && ln -sf mplayer.1 mencoder.1 ; \
		fi ; \
	done
endif
	@$(INSTALL) -d $(DATADIR)
	@$(INSTALL) -d $(DATADIR)/font
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

dirclean:
	-rm -f *.o *.a *~

clean: dirclean
	@for a in $(PARTS); do $(MAKE) -C $$a clean; done

distclean: dirclean doxygen_clean
	@for a in $(PARTS); do $(MAKE) -C $$a distclean; done
	-rm -f *~ $(PRG) $(PRG_MENCODER) codec-cfg codecs2html codecs.conf.h \
          .depend configure.log config.mak config.h help_mp.h version.h

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
# Now all directories are recursed by default because these rules do not
# consider dependencies on files in other directories, while the recursively
# invoked Makefiles do. Conditional recursion only to the directories with
# changed files can be enabled by creating a file named ".norecurse" and
# optionally giving it a timestamp in the past. Directories whose .a files
# are newer than the timestamp and newer than other files in the directory
# will not be recursed.
.norecurse:

libvo/libvo.a: .norecurse $(wildcard libvo/*.[ch])
libao2/libao2.a: .norecurse $(wildcard libao2/*.[ch])
osdep/libosdep.a: .norecurse $(wildcard osdep/*.[ch])
input/libinput.a: .norecurse $(wildcard input/*.[ch])

libmenu/libmenu.a: .norecurse $(wildcard libmenu/*.[ch])
libaf/libaf.a: .norecurse $(wildcard libaf/*.[ch])
libmpdvdkit2/libmpdvdkit.a: .norecurse $(wildcard libmpdvdkit2/*.[ch])
libdvdcss/libdvdcss.a: .norecurse $(wildcard libdvdcss/*.[ch])

libmpdemux/libmpdemux.a: .norecurse $(wildcard libmpdemux/*.[ch] libmpdemux/*/*.[ch])
stream/stream.a: .norecurse $(wildcard stream/*.[ch] stream/*/*.[ch])
libmpcodecs/libmpcodecs.a: .norecurse $(wildcard libmpcodecs/*.[ch]) $(wildcard libmpcodecs/native/*.[ch])
libmpcodecs/libmpencoders.a: .norecurse $(wildcard libmpcodecs/*.[ch])

libavutil/libavutil.a: .norecurse $(wildcard libavutil/*.[ch])
libavcodec/libavcodec.a: .norecurse $(wildcard libavcodec/*.[ch] libavcodec/*/*.[chS])
libavformat/libavformat.a: .norecurse $(wildcard libavformat/*.[ch])
libswscale/libswscale.a: .norecurse $(wildcard libswscale/*.[ch])

libmpeg2/libmpeg2.a: .norecurse $(wildcard libmpeg2/*.[ch])
liba52/liba52.a: .norecurse $(wildcard liba52/*.[ch])
mp3lib/libMP3.a: .norecurse $(wildcard mp3lib/*.[ch])
libfaad2/libfaad2.a: .norecurse $(wildcard libfaad2/*.[ch] libfaad2/*/*.[ch])

loader/libloader.a: .norecurse $(wildcard loader/*.[chSs])
loader/dmo/libDMO_Filter.a: .norecurse $(wildcard loader/dmo/*.[ch])
loader/dshow/libDS_Filter.a: .norecurse $(wildcard loader/dshow/*.[ch])

libdha/libdha.so: .norecurse $(wildcard libdha/*.[ch])
vidix/libvidix.a: .norecurse $(wildcard vidix/*.[ch])
Gui/libgui.a: .norecurse $(wildcard Gui/*.[ch] Gui/*/*.[ch] Gui/*/*/*.[ch])

libass/libass.a: .norecurse $(wildcard libass/*.[ch])

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
