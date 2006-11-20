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

SRCS_MPLAYER = mplayer.c \
               m_property.c \
               mp_msg.c \
               $(SRCS_COMMON) \
               mixer.c \
               parser-mpcmd.c \
               subopt-helper.c \

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

COMMON_LIBS = libmpcodecs/libmpcodecs.a \
              libaf/libaf.a \
              libmpdemux/libmpdemux.a \
              stream/stream.a \
              libswscale/libswscale.a \
              osdep/libosdep.a \
              $(AV_LIB) \
              $(EXTRA_LIB)\
              $(EXTRALIBS) \

LIBS_MPLAYER = libvo/libvo.a \
               libao2/libao2.a \
               input/libinput.a \
               $(COMMON_LIBS) \
               $(VO_LIBS) \
               $(AO_LIBS) \

LIBS_MENCODER = libmpcodecs/libmpencoders.a \
                $(EXTRA_LIB_MENCODER) \
                $(COMMON_LIBS) \

OBJS_MPLAYER  = $(SRCS_MPLAYER:.c=.o)
OBJS_MENCODER = $(SRCS_MENCODER:.c=.o)

MPLAYER_DEPS  = $(OBJS_MPLAYER)  $(LIBS_MPLAYER)  $(COMMON_LIBS)
MENCODER_DEPS = $(OBJS_MENCODER) $(LIBS_MENCODER) $(COMMON_LIBS)

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

ifeq ($(WIN32DLL),yes)
PARTS += loader loader/dshow loader/dmo
endif
ifeq ($(MP3LIB),yes)
COMMON_LIBS += mp3lib/libMP3.a
PARTS += mp3lib
endif
ifeq ($(LIBA52),yes)
COMMON_LIBS += liba52/liba52.a
PARTS += liba52
endif
ifeq ($(LIBMPEG2),yes)
COMMON_LIBS += libmpeg2/libmpeg2.a
PARTS += libmpeg2
endif
ifeq ($(FAAD_INTERNAL),yes)
COMMON_LIBS += libfaad2/libfaad2.a
PARTS += libfaad2
endif
ifeq ($(TREMOR_INTERNAL),yes)
COMMON_LIBS += tremor/libvorbisidec.a
PARTS += tremor
endif
ifeq ($(VIDIX),yes)
VO_LIBS += libdha/libdha.so vidix/libvidix.a
PARTS += libdha vidix
endif
ifeq ($(DVDREAD_INTERNAL),yes)
COMMON_LIBS += dvdread/libdvdread.a
PARTS += dvdread
endif
ifeq ($(DVDCSS_INTERNAL),yes)
COMMON_LIBS += libdvdcss/libdvdcss.a
PARTS += libdvdcss
endif
ifeq ($(CONFIG_ASS),yes)
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
LIBS_MPLAYER += Gui/libgui.a $(GTK_LIBS)
PARTS += Gui
endif
ifeq ($(LIBMENU),yes)
LIBS_MPLAYER += libmenu/libmenu.a
PARTS += libmenu
endif
ifeq ($(TARGET_WIN32),yes)
OBJS_MPLAYER += osdep/mplayer-rc.o
endif

ALL_PRG = mplayer$(EXESUF)
ifeq ($(MENCODER),yes)
ALL_PRG += mencoder$(EXESUF)
endif

.SUFFIXES: .cc .c .o

all:	$(ALL_PRG)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

libaf/libaf.a:
	$(MAKE) -C libaf

dvdread/libdvdread.a:
	$(MAKE) -C dvdread

libdvdcss/libdvdcss.a:
	$(MAKE) -C libdvdcss

loader/libloader.a:
	$(MAKE) -C loader

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

mplayer$(EXESUF): $(MPLAYER_DEPS)
	$(CC) -o $@ $(OBJS_MPLAYER) $(LIBS_MPLAYER)

mencoder$(EXESUF): $(MENCODER_DEPS)
	$(CC) -o $@ $(OBJS_MENCODER) $(LIBS_MENCODER)

osdep/mplayer-rc.o: osdep/mplayer.rc
	windres -o $@ osdep/mplayer.rc

codec-cfg$(EXESUF): codec-cfg.c codec-cfg.h help_mp.h
	$(HOST_CC) -I. -DCODECS2HTML codec-cfg.c -o $@

codecs.conf.h: codec-cfg$(EXESUF) etc/codecs.conf
	./codec-cfg$(EXESUF) ./etc/codecs.conf > $@

codec-cfg.o: codecs.conf.h

codecs2html$(EXESUF): mp_msg.o
	$(CC) -DCODECS2HTML codec-cfg.c mp_msg.o -o $@

install: $(ALL_PRG)
ifeq ($(VIDIX),yes)
	$(MAKE) -C libdha install
	$(MAKE) -C vidix install
endif
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 755 $(INSTALLSTRIP) mplayer$(EXESUF) \
		$(BINDIR)/mplayer$(EXESUF)
ifeq ($(GUI),yes)
	-ln -sf mplayer$(EXESUF) $(BINDIR)/gmplayer$(EXESUF)
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
	$(INSTALL) -m 755 $(INSTALLSTRIP) mencoder$(EXESUF) \
		$(BINDIR)/mencoder$(EXESUF)
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
	-rm -f $(BINDIR)/mplayer$(EXESUF) $(BINDIR)/gmplayer$(EXESUF)
	-rm -f $(BINDIR)/mencoder$(EXESUF)
	-rm -f $(MANDIR)/man1/mencoder.1 $(MANDIR)/man1/mplayer.1
	-rm -f $(prefix)/share/pixmaps/mplayer.xpm
	-rm -f $(prefix)/share/applications/mplayer.desktop
	for l in $(MAN_LANG); do \
	  if test "$$l" != "en"; then \
	    rm -f $(MANDIR)/$$l/man1/mplayer.1    \
	          $(MANDIR)/$$l/man1/mencoder.1   \
	          $(MANDIR)/$$l/man1/gmplayer.1 ; \
	  fi ; \
	done
	$(MAKE) -C libdha uninstall
	$(MAKE) -C vidix uninstall
	@echo "Uninstall completed"

dirclean:
	-rm -f *.o *.a *~

clean: dirclean
	@for a in $(PARTS); do $(MAKE) -C $$a clean; done

distclean: dirclean doxygen_clean
	@for a in $(PARTS); do $(MAKE) -C $$a distclean; done
	-rm -f *~ mplayer$(EXESUF) mencoder$(EXESUF) \
	  codec-cfg$(EXESUF) codecs2html$(EXESUF) codecs.conf.h \
          .depend configure.log config.mak config.h help_mp.h version.h

strip:
	strip -s $(ALL_PRG)

dep depend: help_mp.h version.h
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
	iconv -f UTF-8 -t $(CHARSET) "$(HELP_FILE)" >> help_mp.h
endif

ifneq ($(HELP_FILE),help/help_mp-en.h)
	@echo "Adding untranslated messages to help_mp.h"
	@echo '// untranslated messages from the English master file:' >> help_mp.h
	@help/help_diff.sh $(HELP_FILE) < help/help_mp-en.h >> help_mp.h
endif

# explicit dependencies to force version.h to be built even if .depend is missing
mplayer.o mencoder.o vobsub.o: version.h

# temporary measure to make sure help_mp.h is built. we desperately need correct deps!
$(MPLAYER_DEPS) $(MENCODER_DEPS): help_mp.h

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
dvdread/libdvdread.a: .norecurse $(wildcard dvdread/*.[ch])
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
