# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected
#			* DVD support

include config.mak

CFLAGS = -I. -I./libavutil $(OPTFLAGS)

CFLAGS-$(CONFIG_LIBAVCODEC) += -I./libavcodec
CFLAGS                      += $(CFLAGS-yes)

COMMON_LDFLAGS += $(EXTRA_LIB)\
                  $(EXTRALIBS) \

LDFLAGS_MPLAYER = $(EXTRALIBS_MPLAYER) \
                  $(COMMON_LDFLAGS) \

LDFLAGS_MENCODER = $(EXTRALIBS_MENCODER) \
                   $(COMMON_LDFLAGS) \

SRCS_COMMON = asxparser.c \
              codec-cfg.c \
              cpudetect.c \
              edl.c \
              find_sub.c \
              m_config.c \
              m_option.c \
              m_struct.c \
              mpcommon.c \
              parser-cfg.c \
              playtree.c \
              playtreeparser.c \
              spudec.c \
              sub_cc.c \
              subreader.c \
              vobsub.c \

SRCS_COMMON-$(UNRARLIB) += unrarlib.c

SRCS_MPLAYER = mplayer.c \
               m_property.c \
               mp_fifo.c \
               mp_msg.c \
               mixer.c \
               parser-mpcmd.c \
               subopt-helper.c \
               command.c \

SRCS_MENCODER = mencoder.c \
                mp_msg-mencoder.c \
                parser-mecmd.c \
                xvid_vbr.c \

COMMON_LIBS = libmpcodecs/libmpcodecs.a \
              libaf/libaf.a \
              libmpdemux/libmpdemux.a \
              stream/stream.a \
              libswscale/libswscale.a \
              libvo/libosd.a \

COMMON_LIBS-$(CONFIG_LIBAVFORMAT) += libavformat/libavformat.a
COMMON_LIBS-$(CONFIG_LIBAVCODEC)  += libavcodec/libavcodec.a
COMMON_LIBS-$(CONFIG_LIBAVUTIL)   += libavutil/libavutil.a
COMMON_LIBS-$(CONFIG_LIBPOSTPROC) += libpostproc/libpostproc.a
COMMON_LIBS-$(WIN32DLL)           += loader/libloader.a
COMMON_LIBS-$(MP3LIB)             += mp3lib/libmp3.a
COMMON_LIBS-$(LIBA52)             += liba52/liba52.a
COMMON_LIBS-$(LIBMPEG2)           += libmpeg2/libmpeg2.a
COMMON_LIBS-$(FAAD_INTERNAL)      += libfaad2/libfaad2.a
COMMON_LIBS-$(TREMOR_INTERNAL)    += tremor/libvorbisidec.a
COMMON_LIBS-$(DVDREAD_INTERNAL)   += dvdread/libdvdread.a
COMMON_LIBS-$(DVDCSS_INTERNAL)    += libdvdcss/libdvdcss.a
COMMON_LIBS-$(CONFIG_ASS)         += libass/libass.a

LIBS_MPLAYER = libvo/libvo.a \
               libao2/libao2.a \
               input/libinput.a \

LIBS_MPLAYER-$(VIDIX)             += vidix/libvidix.a libdha/libdha.a
LIBS_MPLAYER-$(GUI)               += Gui/libgui.a
LIBS_MPLAYER-$(LIBMENU)           += libmenu/libmenu.a

LIBS_MENCODER = libmpcodecs/libmpencoders.a \
                libmpdemux/libmpmux.a \

# Having this in libosdep.a is not enough.
OBJS_MPLAYER-$(TARGET_WIN32) += osdep/mplayer-rc.o

ALL_PRG-$(MPLAYER)  += mplayer$(EXESUF)
ALL_PRG-$(MENCODER) += mencoder$(EXESUF)

OBJS_COMMON   = $(SRCS_COMMON:.c=.o)
OBJS_MPLAYER  = $(SRCS_MPLAYER:.c=.o)
OBJS_MENCODER = $(SRCS_MENCODER:.c=.o)

SRCS_COMMON  += $(SRCS_COMMON-yes)
COMMON_LIBS  += $(COMMON_LIBS-yes)
LIBS_MPLAYER += $(LIBS_MPLAYER-yes)
OBJS_MPLAYER += $(OBJS_MPLAYER-yes)
PARTS        += $(PARTS-yes)
ALL_PRG      += $(ALL_PRG-yes)

COMMON_LIBS += osdep/libosdep.a

MPLAYER_DEPS  = $(OBJS_MPLAYER)  $(OBJS_COMMON) $(LIBS_MPLAYER)  $(COMMON_LIBS)
MENCODER_DEPS = $(OBJS_MENCODER) $(OBJS_COMMON) $(LIBS_MENCODER) $(COMMON_LIBS)

INSTALL_TARGETS-$(MPLAYER)  += install-mplayer  install-mplayer-man
INSTALL_TARGETS-$(MENCODER) += install-mencoder install-mplayer-man
INSTALL_TARGETS-$(GUI)      += install-gui
INSTALL_TARGETS             += $(INSTALL_TARGETS-yes)

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
        liba52 \
        libavcodec \
        libavformat \
        libpostproc \
        loader \
        mp3lib \
        libmpeg2 \
        libfaad2 \
        tremor \
        libdha \
        vidix \
        dvdread \
        libdvdcss \
        libass \
        Gui \
        libmenu \


all:	$(ALL_PRG)

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
	$(MAKE) -C libmpdemux libmpdemux.a

libmpdemux/libmpmux.a:
	$(MAKE) -C libmpdemux libmpmux.a

stream/stream.a:
	$(MAKE) -C stream

libmpcodecs/libmpcodecs.a:
	$(MAKE) -C libmpcodecs

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
	$(MAKE) -C libvo libvo.a

libvo/libosd.a:
	$(MAKE) -C libvo libosd.a

libao2/libao2.a:
	$(MAKE) -C libao2

liba52/liba52.a:
	$(MAKE) -C liba52

libfaad2/libfaad2.a:
	$(MAKE) -C libfaad2

mp3lib/libmp3.a:
	$(MAKE) -C mp3lib

tremor/libvorbisidec.a:
	$(MAKE) -C tremor

libdha/libdha.a:
	$(MAKE) -C libdha

vidix/libvidix.a: libdha/libdha.a
	$(MAKE) -C vidix

Gui/libgui.a:
	$(MAKE) -C Gui

osdep/libosdep.a:
	$(MAKE) -C osdep

osdep/mplayer-rc.o:
	$(MAKE) -C osdep mplayer-rc.o

input/libinput.a:
	$(MAKE) -C input

libmenu/libmenu.a:
	$(MAKE) -C libmenu

mplayer$(EXESUF): $(MPLAYER_DEPS)
	$(CC) -o $@ $^ $(LDFLAGS_MPLAYER)

mencoder$(EXESUF): $(MENCODER_DEPS)
	$(CC) -o $@ $^ $(LDFLAGS_MENCODER)

codec-cfg$(EXESUF): codec-cfg.c codec-cfg.h help_mp.h
	$(HOST_CC) -I. -DCODECS2HTML $< -o $@

codecs.conf.h: codec-cfg$(EXESUF) etc/codecs.conf
	./codec-cfg$(EXESUF) ./etc/codecs.conf > $@

codec-cfg.o: codecs.conf.h

codecs2html$(EXESUF): mp_msg.o
	$(CC) -DCODECS2HTML codec-cfg.c $^ -o $@

codec-cfg-test$(EXESUF): codecs.conf.h codec-cfg.h mp_msg.o osdep/getch2.o
	$(CC) -I. -DTESTING codec-cfg.c mp_msg.o osdep/getch2.o -ltermcap -o $@

install: install-dirs $(INSTALL_TARGETS)

install-dirs:
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -d $(DATADIR)
	$(INSTALL) -d $(MANDIR)/man1
	$(INSTALL) -d $(CONFDIR)
	if test -f $(CONFDIR)/codecs.conf ; then mv -f $(CONFDIR)/codecs.conf $(CONFDIR)/codecs.conf.old ; fi

install-mplayer: mplayer$(EXESUF)
	$(INSTALL) -m 755 $(INSTALLSTRIP) mplayer$(EXESUF) $(BINDIR)

install-mplayer-man:
	for i in $(MAN_LANG); do \
		if test "$$i" = en ; then \
			$(INSTALL) -c -m 644 DOCS/man/en/mplayer.1 $(MANDIR)/man1/ ; \
		else \
			$(INSTALL) -d $(MANDIR)/$$i/man1 ; \
			$(INSTALL) -c -m 644 DOCS/man/$$i/mplayer.1 $(MANDIR)/$$i/man1/ ; \
		fi ; \
	done

install-mencoder: mencoder$(EXESUF)
	$(INSTALL) -m 755 $(INSTALLSTRIP) mencoder$(EXESUF) $(BINDIR)
	for i in $(MAN_LANG); do \
		if test "$$i" = en ; then \
			cd $(MANDIR)/man1 && ln -sf mplayer.1 mencoder.1 ; \
		else \
			cd $(MANDIR)/$$i/man1 && ln -sf mplayer.1 mencoder.1 ; \
		fi ; \
	done

install-gui:
	-ln -sf mplayer$(EXESUF) $(BINDIR)/gmplayer$(EXESUF)
	$(INSTALL) -d $(DATADIR)/skins
	@echo "*** Download skin(s) at http://www.mplayerhq.hu/design7/dload.html"
	@echo "*** for GUI, and extract to $(DATADIR)/skins/"
	$(INSTALL) -d $(prefix)/share/pixmaps
	$(INSTALL) -m 644 etc/mplayer.xpm $(prefix)/share/pixmaps/
	$(INSTALL) -d $(prefix)/share/applications
	$(INSTALL) -m 644 etc/mplayer.desktop $(prefix)/share/applications/

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

dep depend: help_mp.h version.h codecs.conf.h
	$(CC) -MM $(CFLAGS) $(SRCS_MPLAYER) $(SRCS_MENCODER) $(SRCS_COMMON) 1>.depend
	@for a in $(PARTS); do $(MAKE) -C $$a dep; done

clean:
	-rm -f *.o *.a *~
	-rm -f mplayer$(EXESUF) mencoder$(EXESUF) codec-cfg$(EXESUF) \
	  codecs2html$(EXESUF) codec-cfg-test$(EXESUF) \
	  codecs.conf.h help_mp.h version.h
	@for a in $(PARTS); do $(MAKE) -C $$a clean; done

distclean: clean doxygen_clean
	@for a in $(PARTS); do $(MAKE) -C $$a distclean; done
	-rm -f .depend configure.log config.mak config.h

strip:
	strip -s $(ALL_PRG)

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
libvo/libosd.a: .norecurse $(wildcard libvo/*.[ch])
libao2/libao2.a: .norecurse $(wildcard libao2/*.[ch])
osdep/libosdep.a: .norecurse $(wildcard osdep/*.[ch])
input/libinput.a: .norecurse $(wildcard input/*.[ch])

libmenu/libmenu.a: .norecurse $(wildcard libmenu/*.[ch])
libaf/libaf.a: .norecurse $(wildcard libaf/*.[ch])
dvdread/libdvdread.a: .norecurse $(wildcard dvdread/*.[ch])
libdvdcss/libdvdcss.a: .norecurse $(wildcard libdvdcss/*.[ch])

libmpdemux/libmpdemux.a: .norecurse $(wildcard libmpdemux/*.[ch])
libmpdemux/libmpmux.a: .norecurse $(wildcard libmpdemux/*.[ch])
stream/stream.a: .norecurse $(wildcard stream/*.[ch] stream/*/*.[ch])
libmpcodecs/libmpcodecs.a: .norecurse $(wildcard libmpcodecs/*.[ch]) $(wildcard libmpcodecs/native/*.[ch])
libmpcodecs/libmpencoders.a: .norecurse $(wildcard libmpcodecs/*.[ch])

libavutil/libavutil.a: .norecurse $(wildcard libavutil/*.[ch])
libavcodec/libavcodec.a: .norecurse $(wildcard libavcodec/*.[ch] libavcodec/*/*.[chS])
libavformat/libavformat.a: .norecurse $(wildcard libavformat/*.[ch])
libswscale/libswscale.a: .norecurse $(wildcard libswscale/*.[ch])

libmpeg2/libmpeg2.a: .norecurse $(wildcard libmpeg2/*.[ch])
liba52/liba52.a: .norecurse $(wildcard liba52/*.[ch])
mp3lib/libmp3.a: .norecurse $(wildcard mp3lib/*.[ch])
libfaad2/libfaad2.a: .norecurse $(wildcard libfaad2/*.[ch] libfaad2/*/*.[ch])

loader/libloader.a: .norecurse $(wildcard loader/*.[chSs])
libdha/libdha.a: .norecurse $(wildcard libdha/*.[ch])
vidix/libvidix.a: .norecurse $(wildcard vidix/*.[ch])
Gui/libgui.a: .norecurse $(wildcard Gui/*.[ch] Gui/*/*.[ch] Gui/*/*/*.[ch])

libass/libass.a: .norecurse $(wildcard libass/*.[ch])

-include .depend

.PHONY: all install* uninstall clean distclean strip dep depend
.PHONY: doxygen doxygen_clean
