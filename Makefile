# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected
#			* DVD support

include config.mak

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
              get_path.c \
              m_config.c \
              m_option.c \
              m_struct.c \
              mpcommon.c \
              parser-cfg.c \
              playtree.c \
              playtreeparser.c \
              spudec.c \
              sub_cc.c \
              subopt-helper.c \
              subreader.c \
              vobsub.c \
              osdep/$(GETCH) \
              osdep/$(TIMER) \

SRCS_COMMON-$(HAVE_SYS_MMAN_H)       += osdep/mmap_anon.c
SRCS_COMMON-$(MACOSX_FINDER_SUPPORT) += osdep/macosx_finder_args.c
SRCS_COMMON-$(NEED_GETTIMEOFDAY)     += osdep/gettimeofday.c
SRCS_COMMON-$(NEED_GLOB)             += osdep/glob-win.c
SRCS_COMMON-$(NEED_SETENV)           += osdep/setenv.c
SRCS_COMMON-$(NEED_SHMEM)            += osdep/shmem.c
SRCS_COMMON-$(NEED_STRSEP)           += osdep/strsep.c
SRCS_COMMON-$(NEED_SWAB)             += osdep/swab.c
SRCS_COMMON-$(NEED_VSSCANF)          += osdep/vsscanf.c
SRCS_COMMON-$(UNRAR_EXEC) += unrar_exec.c

SRCS_MPLAYER = mplayer.c \
               m_property.c \
               mp_fifo.c \
               mp_msg.c \
               mixer.c \
               parser-mpcmd.c \
               command.c \
               input/input.c \
               libmenu/menu.c \
               libmenu/menu_chapsel.c \
               libmenu/menu_cmdlist.c  \
               libmenu/menu_console.c \
               libmenu/menu_filesel.c \
               libmenu/menu_list.c  \
               libmenu/menu_param.c \
               libmenu/menu_pt.c \
               libmenu/menu_txt.c \
               libmenu/vf_menu.c \

SRCS_MPLAYER-$(APPLE_REMOTE) += input/ar.c
SRCS_MPLAYER-$(DVBIN)        += libmenu/menu_dvbin.c
SRCS_MPLAYER-$(JOYSTICK)     += input/joystick.c
SRCS_MPLAYER-$(LIRC)         += input/lirc.c


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

COMMON_LIBS-$(LIBAVFORMAT_A)      += libavformat/libavformat.a
COMMON_LIBS-$(LIBAVCODEC_A)       += libavcodec/libavcodec.a
COMMON_LIBS-$(LIBAVUTIL_A)        += libavutil/libavutil.a
COMMON_LIBS-$(LIBPOSTPROC_A)      += libpostproc/libpostproc.a
COMMON_LIBS-$(WIN32DLL)           += loader/libloader.a
COMMON_LIBS-$(MP3LIB)             += mp3lib/libmp3.a
COMMON_LIBS-$(LIBA52)             += liba52/liba52.a
COMMON_LIBS-$(LIBMPEG2)           += libmpeg2/libmpeg2.a
COMMON_LIBS-$(FAAD_INTERNAL)      += libfaad2/libfaad2.a
COMMON_LIBS-$(TREMOR_INTERNAL)    += tremor/libvorbisidec.a
COMMON_LIBS-$(DVDREAD_INTERNAL)   += dvdread/libdvdread.a
COMMON_LIBS-$(DVDCSS_INTERNAL)    += libdvdcss/libdvdcss.a
COMMON_LIBS-$(ASS)                += libass/libass.a

LIBS_MPLAYER = libvo/libvo.a \
               libao2/libao2.a \

LIBS_MPLAYER-$(VIDIX)             += vidix/libvidix.a
LIBS_MPLAYER-$(GUI)               += gui/libgui.a

LIBS_MENCODER = libmpcodecs/libmpencoders.a \
                libmpdemux/libmpmux.a \

ALL_PRG-$(MPLAYER)  += mplayer$(EXESUF)
ALL_PRG-$(MENCODER) += mencoder$(EXESUF)

COMMON_LIBS  += $(COMMON_LIBS-yes)
LIBS_MPLAYER += $(LIBS_MPLAYER-yes)
OBJS_MPLAYER += $(OBJS_MPLAYER-yes)
ALL_PRG      += $(ALL_PRG-yes)

MPLAYER_DEPS  = $(OBJS_MPLAYER)  $(OBJS_COMMON) $(LIBS_MPLAYER)  $(COMMON_LIBS)
MENCODER_DEPS = $(OBJS_MENCODER) $(OBJS_COMMON) $(LIBS_MENCODER) $(COMMON_LIBS)

INSTALL_TARGETS-$(MPLAYER)  += install-mplayer  install-mplayer-man
INSTALL_TARGETS-$(MENCODER) += install-mencoder install-mplayer-man
INSTALL_TARGETS-$(GUI)      += install-gui
INSTALL_TARGETS             += $(INSTALL_TARGETS-yes)

PARTS = dvdread \
        gui \
        liba52 \
        libaf \
        libao2 \
        libass \
        libavcodec \
        libavformat \
        libavutil \
        libdvdcss \
        libfaad2 \
        libmpcodecs \
        libmpdemux \
        libmpeg2 \
        libpostproc \
        libswscale \
        libvo \
        loader \
        mp3lib \
        stream \
        tremor \
        vidix \

DIRS =  input \
        libmenu \
        osdep \

all:	$(ALL_PRG)

dep depend:: help_mp.h version.h codecs.conf.h
	for a in $(PARTS); do $(MAKE) -C $$a dep; done

include mpcommon.mak

CFLAGS := $(subst -I..,-I.,$(CFLAGS))

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
	$(MAKE) -C libavutil

libavcodec/libavcodec.a:
	$(MAKE) -C libavcodec

libpostproc/libpostproc.a:
	$(MAKE) -C libpostproc

libavformat/libavformat.a:
	$(MAKE) -C libavformat

libswscale/libswscale.a:
	$(MAKE) -C libswscale

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

vidix/libvidix.a:
	$(MAKE) -C vidix

gui/libgui.a:
	$(MAKE) -C gui

mplayer$(EXESUF): $(MPLAYER_DEPS)
	$(CC) -o $@ $^ $(LDFLAGS_MPLAYER)

mencoder$(EXESUF): $(MENCODER_DEPS)
	$(CC) -o $@ $^ $(LDFLAGS_MENCODER)

codec-cfg$(EXESUF): codec-cfg.c codec-cfg.h help_mp.h
	$(HOST_CC) -O -I. -DCODECS2HTML $< -o $@

codecs.conf.h: codec-cfg$(EXESUF) etc/codecs.conf
	./codec-cfg$(EXESUF) ./etc/codecs.conf > $@

codec-cfg.o: codecs.conf.h

codecs2html$(EXESUF): mp_msg.o
	$(CC) -DCODECS2HTML codec-cfg.c $^ -o $@

codec-cfg-test$(EXESUF): codecs.conf.h codec-cfg.h mp_msg.o osdep/getch2.o
	$(CC) -I. -DTESTING codec-cfg.c mp_msg.o osdep/getch2.o -ltermcap -o $@

osdep/mplayer-rc.o: osdep/mplayer.rc version.h
	$(WINDRES) -o $@ $<

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

clean::
	-rm -f mplayer$(EXESUF) mencoder$(EXESUF) codec-cfg$(EXESUF) \
	  codecs2html$(EXESUF) codec-cfg-test$(EXESUF) cpuinfo$(EXESUF) \
	  codecs.conf.h help_mp.h version.h TAGS tags
	for a in $(PARTS); do $(MAKE) -C $$a clean; done
	for dir in $(DIRS); do rm -f $$dir/*.o $$dir/*.a $$dir/*.ho $$dir/*~ ; done

distclean:: doxygen_clean
	for a in $(PARTS); do $(MAKE) -C $$a distclean; done
	$(MAKE) -C TOOLS distclean
	-rm -f configure.log config.mak config.h

strip:
	strip -s $(ALL_PRG)

TAGS:
	rm -f $@; ( find -name '*.[chS]' -print ) | xargs etags -a

tags:
	rm -f $@; ( find -name '*.[chS]' -print ) | xargs ctags -a

# ./configure must be rerun if it changed
config.mak: configure
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
	@echo '#ifndef MPLAYER_HELP_MP_H' >> help_mp.h
	@echo '#define MPLAYER_HELP_MP_H' >> help_mp.h
ifeq ($(CHARSET),)
	@echo '#include "$(HELP_FILE)"' >> help_mp.h
else
	iconv -f UTF-8 -t $(CHARSET) "$(HELP_FILE)" >> help_mp.h
endif
	@echo '#endif /* MPLAYER_HELP_MP_H */' >> help_mp.h

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
vidix/libvidix.a: .norecurse $(wildcard vidix/*.[ch])
gui/libgui.a: .norecurse $(wildcard gui/*.[ch] gui/*/*.[ch] gui/*/*/*.[ch])

libass/libass.a: .norecurse $(wildcard libass/*.[ch])

.PHONY: all install* uninstall strip doxygen doxygen_clean TAGS tags
