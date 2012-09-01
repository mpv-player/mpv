# MPlayer Makefile
#
# copyright (c) 2008 Diego Biurrun
# Rewritten entirely from a set of Makefiles written by Arpi and many others.
#
# This file is part of MPlayer.
#
# MPlayer is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# MPlayer is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with MPlayer; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

include config.mak

###### variable declarations #######

SRCS_AUDIO_INPUT-$(ALSA)             += stream/ai_alsa1x.c
SRCS_AUDIO_INPUT-$(OSS)              += stream/ai_oss.c
SRCS_COMMON-$(AUDIO_INPUT)           += $(SRCS_AUDIO_INPUT-yes)
SRCS_COMMON-$(CDDA)                  += stream/stream_cdda.c \
                                        stream/cdinfo.c
SRCS_COMMON-$(CDDB)                  += stream/stream_cddb.c
SRCS_COMMON-$(DVBIN)                 += stream/dvb_tune.c \
                                        stream/stream_dvb.c
SRCS_COMMON-$(DVDREAD)               += stream/stream_dvd.c \
                                        stream/stream_dvd_common.c

# These filters use private headers and do not work with shared libavcodec.
SRCS_COMMON-$(FFMPEG_INTERNALS)      += libmpcodecs/vf_mcdeint.c \
                                        libmpcodecs/vf_spp.c \

SRCS_COMMON-$(FTP)                   += stream/stream_ftp.c
SRCS_COMMON-$(GIF)                   += libmpdemux/demux_gif.c
SRCS_COMMON-$(HAVE_POSIX_SELECT)     += libmpcodecs/vf_bmovl.c
SRCS_COMMON-$(HAVE_SYS_MMAN_H)       += libaf/af_export.c osdep/mmap_anon.c
SRCS_COMMON-$(LADSPA)                += libaf/af_ladspa.c
SRCS_COMMON-$(LIBASS)                += libmpcodecs/vf_ass.c \
                                        sub/ass_mp.c \
                                        sub/sd_ass.c \

SRCS_COMMON-$(LIBBLURAY)             += stream/stream_bluray.c
SRCS_COMMON-$(LIBBS2B)               += libaf/af_bs2b.c

SRCS_COMMON-$(LIBPOSTPROC)           += libmpcodecs/vf_pp.c
SRCS_COMMON-$(LIBSMBCLIENT)          += stream/stream_smb.c

SRCS_COMMON-$(MACOSX_FINDER)         += osdep/macosx_finder_args.m
SRCS_COMMON-$(COCOA)                 += libvo/osx_common.c \
                                        libvo/cocoa_common.m \
                                        osdep/cocoa_events.m
SRCS_COMMON-$(MNG)                   += libmpdemux/demux_mng.c
SRCS_COMMON-$(MPG123)                += libmpcodecs/ad_mpg123.c

SRCS_COMMON-$(NEED_GETTIMEOFDAY)     += osdep/gettimeofday.c
SRCS_COMMON-$(NEED_GLOB)             += osdep/glob-win.c
SRCS_COMMON-$(NEED_SETENV)           += osdep/setenv.c
SRCS_COMMON-$(NEED_SHMEM)            += osdep/shmem.c
SRCS_COMMON-$(NEED_STRSEP)           += osdep/strsep.c
SRCS_COMMON-$(NEED_VSSCANF)          += osdep/vsscanf.c
SRCS_COMMON-$(NETWORKING)            += stream/stream_netstream.c \
                                        stream/asf_mmst_streaming.c \
                                        stream/asf_streaming.c \
                                        stream/cookies.c \
                                        stream/http.c \
                                        stream/network.c \
                                        stream/udp.c \
                                        stream/tcp.c \
                                        stream/stream_udp.c \

SRCS_COMMON-$(PRIORITY)              += osdep/priority.c
SRCS_COMMON-$(PVR)                   += stream/stream_pvr.c
SRCS_COMMON-$(RADIO)                 += stream/stream_radio.c
SRCS_COMMON-$(RADIO_CAPTURE)         += stream/audio_in.c
SRCS_COMMON-$(STREAM_CACHE)          += stream/cache2.c

SRCS_COMMON-$(TV)                    += stream/stream_tv.c stream/tv.c \
                                        stream/frequencies.c stream/tvi_dummy.c
SRCS_COMMON-$(TV_BSDBT848)           += stream/tvi_bsdbt848.c

SRCS_COMMON-$(TV_V4L2)               += stream/tvi_v4l2.c stream/audio_in.c
SRCS_COMMON-$(VCD)                   += stream/stream_vcd.c
SRCS_COMMON-$(VSTREAM)               += stream/stream_vstream.c

SRCS_COMMON-$(DUMMY_OSD)             += sub/osd_dummy.c
SRCS_COMMON-$(LIBASS_OSD)            += sub/osd_libass.c

SRCS_COMMON = asxparser.c \
              av_log.c \
              av_opts.c \
              bstr.c \
              codec-cfg.c \
              cpudetect.c \
              defaultopts.c \
              fmt-conversion.c \
              m_config.c \
              m_option.c \
              m_struct.c \
              mp_msg.c \
              mpcommon.c \
              version.c \
              parser-cfg.c \
              path.c \
              playlist.c \
              playlist_parser.c \
              subopt-helper.c \
              talloc.c \
              libaf/af.c \
              libaf/af_center.c \
              libaf/af_channels.c \
              libaf/af_delay.c \
              libaf/af_dummy.c \
              libaf/af_equalizer.c \
              libaf/af_extrastereo.c \
              libaf/af_format.c \
              libaf/af_hrtf.c \
              libaf/af_karaoke.c \
              libaf/af_lavcac3enc.c \
              libaf/af_lavcresample.c \
              libaf/af_pan.c \
              libaf/af_resample.c \
              libaf/af_scaletempo.c \
              libaf/af_sinesuppress.c \
              libaf/af_sub.c \
              libaf/af_surround.c \
              libaf/af_sweep.c \
              libaf/af_tools.c \
              libaf/af_volnorm.c \
              libaf/af_volume.c \
              libaf/filter.c \
              libaf/format.c \
              libaf/reorder_ch.c \
              libaf/window.c \
              libmpcodecs/ad.c \
              libmpcodecs/ad_ffmpeg.c \
              libmpcodecs/ad_spdif.c      \
              libmpcodecs/dec_audio.c \
              libmpcodecs/dec_video.c \
              libmpcodecs/img_format.c \
              libmpcodecs/mp_image.c \
              libmpcodecs/pullup.c \
              libmpcodecs/vd.c \
              libmpcodecs/vd_ffmpeg.c \
              libmpcodecs/vf.c \
              libmpcodecs/vf_1bpp.c \
              libmpcodecs/vf_2xsai.c \
              libmpcodecs/vf_blackframe.c \
              libmpcodecs/vf_boxblur.c \
              libmpcodecs/vf_crop.c \
              libmpcodecs/vf_cropdetect.c \
              libmpcodecs/vf_decimate.c \
              libmpcodecs/vf_delogo.c \
              libmpcodecs/vf_denoise3d.c \
              libmpcodecs/vf_detc.c \
              libmpcodecs/vf_dint.c \
              libmpcodecs/vf_divtc.c \
              libmpcodecs/vf_dlopen.c \
              libmpcodecs/vf_down3dright.c \
              libmpcodecs/vf_dsize.c \
              libmpcodecs/vf_dvbscale.c \
              libmpcodecs/vf_eq.c \
              libmpcodecs/vf_eq2.c \
              libmpcodecs/vf_expand.c \
              libmpcodecs/vf_field.c \
              libmpcodecs/vf_fil.c \
              libmpcodecs/vf_filmdint.c \
              libmpcodecs/vf_fixpts.c \
              libmpcodecs/vf_flip.c \
              libmpcodecs/vf_format.c \
              libmpcodecs/vf_framestep.c \
              libmpcodecs/vf_fspp.c \
              libmpcodecs/vf_geq.c \
              libmpcodecs/vf_gradfun.c \
              libmpcodecs/vf_halfpack.c \
              libmpcodecs/vf_harddup.c \
              libmpcodecs/vf_hqdn3d.c \
              libmpcodecs/vf_hue.c \
              libmpcodecs/vf_il.c \
              libmpcodecs/vf_ilpack.c \
              libmpcodecs/vf_ivtc.c \
              libmpcodecs/vf_kerndeint.c \
              libmpcodecs/vf_lavc.c \
              libmpcodecs/vf_lavcdeint.c \
              libmpcodecs/vf_mirror.c \
              libmpcodecs/vf_noformat.c \
              libmpcodecs/vf_noise.c \
              libmpcodecs/vf_ow.c \
              libmpcodecs/vf_palette.c \
              libmpcodecs/vf_perspective.c \
              libmpcodecs/vf_phase.c \
              libmpcodecs/vf_pp7.c \
              libmpcodecs/vf_pullup.c \
              libmpcodecs/vf_qp.c \
              libmpcodecs/vf_rectangle.c \
              libmpcodecs/vf_remove_logo.c \
              libmpcodecs/vf_rgbtest.c \
              libmpcodecs/vf_rotate.c \
              libmpcodecs/vf_sab.c \
              libmpcodecs/vf_scale.c \
              libmpcodecs/vf_screenshot.c \
              libmpcodecs/vf_smartblur.c \
              libmpcodecs/vf_softpulldown.c \
              libmpcodecs/vf_stereo3d.c \
              libmpcodecs/vf_softskip.c \
              libmpcodecs/vf_swapuv.c \
              libmpcodecs/vf_telecine.c \
              libmpcodecs/vf_test.c \
              libmpcodecs/vf_tfields.c \
              libmpcodecs/vf_tile.c \
              libmpcodecs/vf_tinterlace.c \
              libmpcodecs/vf_unsharp.c \
              libmpcodecs/vf_uspp.c \
              libmpcodecs/vf_vo.c \
              libmpcodecs/vf_yadif.c \
              libmpcodecs/vf_yuvcsp.c \
              libmpcodecs/vf_yvu9.c \
              libmpdemux/asfheader.c \
              libmpdemux/aviheader.c \
              libmpdemux/aviprint.c \
              libmpdemux/demuxer.c \
              libmpdemux/demux_asf.c \
              libmpdemux/demux_avi.c \
              libmpdemux/demux_edl.c \
              libmpdemux/demux_cue.c \
              libmpdemux/demux_lavf.c \
              libmpdemux/demux_mf.c \
              libmpdemux/demux_mkv.c \
              libmpdemux/demux_rawaudio.c \
              libmpdemux/demux_rawvideo.c \
              libmpdemux/ebml.c \
              libmpdemux/extension.c \
              libmpdemux/mf.c \
              libmpdemux/mp_taglists.c \
              libmpdemux/video.c \
              libvo/osd.c \
              libvo/eosd_packer.c \
              libvo/bitmap_packer.c \
              osdep/numcores.c \
              osdep/io.c \
              osdep/$(GETCH) \
              osdep/$(TIMER) \
              stream/open.c \
              stream/stream.c \
              stream/stream_ffmpeg.c \
              stream/stream_file.c \
              stream/stream_mf.c \
              stream/stream_null.c \
              stream/url.c \
              sub/dec_sub.c \
              sub/find_sub.c \
              sub/find_subfiles.c \
              sub/sd_lavc.c \
              sub/spudec.c \
              sub/sub.c \
              sub/subassconvert.c \
              sub/subreader.c \
              sub/vobsub.c \
              timeline/tl_edl.c \
              timeline/tl_matroska.c \
              timeline/tl_cue.c \
              $(SRCS_COMMON-yes)


SRCS_MPLAYER-$(ALSA)         += libao2/ao_alsa.c
SRCS_MPLAYER-$(APPLE_IR)     += input/appleir.c
SRCS_MPLAYER-$(APPLE_REMOTE) += input/ar.c
SRCS_MPLAYER-$(CACA)         += libvo/vo_caca.c
SRCS_MPLAYER-$(COREAUDIO)    += libao2/ao_coreaudio.c
SRCS_MPLAYER-$(COREVIDEO)    += libvo/vo_corevideo.m
SRCS_MPLAYER-$(DIRECT3D)     += libvo/vo_direct3d.c libvo/w32_common.c
SRCS_MPLAYER-$(GL)           += libvo/gl_common.c libvo/vo_gl.c libvo/vo_gl3.c \
                                pnm_loader.c
SRCS_MPLAYER-$(GL_WIN32)     += libvo/w32_common.c
SRCS_MPLAYER-$(GL_X11)       += libvo/x11_common.c

SRCS_MPLAYER-$(JACK)         += libao2/ao_jack.c
SRCS_MPLAYER-$(JOYSTICK)     += input/joystick.c
SRCS_MPLAYER-$(LIRC)          += input/lirc.c
SRCS_MPLAYER-$(OPENAL)        += libao2/ao_openal.c
SRCS_MPLAYER-$(OSS)           += libao2/ao_oss.c
SRCS_MPLAYER-$(PULSE)         += libao2/ao_pulse.c
SRCS_MPLAYER-$(PORTAUDIO)     += libao2/ao_portaudio.c
SRCS_MPLAYER-$(RSOUND)        += libao2/ao_rsound.c
SRCS_MPLAYER-$(VDPAU)         += libvo/vo_vdpau.c

SRCS_MPLAYER-$(X11)           += libvo/vo_x11.c libvo/x11_common.c
SRCS_MPLAYER-$(XV)            += libvo/vo_xv.c

SRCS_MPLAYER = command.c \
               m_property.c \
               mixer.c \
               mp_fifo.c \
               mplayer.c \
               parser-mpcmd.c \
               screenshot.c \
               image_writer.c \
               input/input.c \
               libao2/ao_null.c \
               libao2/ao_pcm.c \
               libao2/audio_out.c \
               libvo/aspect.c \
               libvo/csputils.c \
               libvo/filter_kernels.c \
               libvo/geometry.c \
               libvo/video_out.c \
               libvo/vo_null.c \
               libvo/vo_image.c \
               $(SRCS_MPLAYER-yes)

COMMON_LIBS += $(COMMON_LIBS-yes)

OBJS_COMMON    += $(addsuffix .o, $(basename $(SRCS_COMMON)))
OBJS_MPLAYER   += $(addsuffix .o, $(basename $(SRCS_MPLAYER)))
OBJS_MPLAYER-$(PE_EXECUTABLE) += osdep/mplayer-rc.o
OBJS_MPLAYER   += $(OBJS_MPLAYER-yes)

MPLAYER_DEPS  = $(OBJS_MPLAYER)  $(OBJS_COMMON) $(COMMON_LIBS)
DEP_FILES = $(patsubst %.S,%.d,$(patsubst %.cpp,%.d,$(patsubst %.c,%.d,$(SRCS_COMMON:.m=.d) $(SRCS_MPLAYER:.m=.d))))

ALL_PRG-$(MPLAYER)  += mplayer$(EXESUF)

INSTALL_TARGETS-$(MPLAYER)  += check_rst2man       \
                               install-mplayer     \
                               install-mplayer-man \
                               install-mplayer-msg

INSTALL_NO_MAN_TARGETS-$(MPLAYER) += install-mplayer  \
                                     install-mplayer-msg

DIRS =  . \
        input \
        libaf \
        libao2 \
        libmpcodecs \
        libmpdemux \
        libvo \
        osdep \
        stream \
        sub \
        timeline \

MOFILES := $(MSG_LANGS:%=locale/%/LC_MESSAGES/mplayer.mo)

ALLHEADERS = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.h))

ADDSUFFIXES     = $(foreach suf,$(1),$(addsuffix $(suf),$(2)))
ADD_ALL_DIRS    = $(call ADDSUFFIXES,$(1),$(DIRS))
ADD_ALL_EXESUFS = $(1) $(call ADDSUFFIXES,$(EXESUFS_ALL),$(1))

###### brief build output #######

ifndef V
$(eval override CC = @printf "CC\t$$@\n"; $(CC))
$(eval override RM = @$(RM))
endif

###### generic rules #######

all: $(ALL_PRG-yes) locales

%.1: %.rst
	rst2man $< $@

%.o: %.S
	$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CC) $(DEPFLAGS) $(CXXFLAGS) -c -o $@ $<

%.o: %.m
	$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<

%-rc.o: %.rc
	$(WINDRES) -I. $< $@

mplayer$(EXESUF): $(MPLAYER_DEPS)
mplayer$(EXESUF): EXTRALIBS += $(EXTRALIBS_MPLAYER)
mplayer$(EXESUF):
	$(CC) -o $@ $^ $(EXTRALIBS)

codec-cfg.c: codecs.conf.h
codecs.conf.h: TOOLS/file2string.py etc/codecs.conf
	./$^ >$@

input/input.c: input/input_conf.h
input/input_conf.h: TOOLS/file2string.py etc/input.conf
	./$^ >$@

libvo/vo_vdpau.c: libvo/vdpau_template.c
libvo/vdpau_template.c: TOOLS/vdpau_functions.py
	./$< > $@

libmpdemux/ebml.c libmpdemux/demux_mkv.c: libmpdemux/ebml_types.h
libmpdemux/ebml_types.h: TOOLS/matroska.py
	./$< --generate-header > $@

libmpdemux/ebml.c: libmpdemux/ebml_defs.c
libmpdemux/ebml_defs.c: TOOLS/matroska.py
	./$< --generate-definitions > $@

libvo/vo_gl3.c: libvo/vo_gl3_shaders.h
libvo/vo_gl3_shaders.h: TOOLS/file2string.py libvo/vo_gl3_shaders.glsl
	./$^ >$@

sub/osd_libass.c: sub/osd_font.h
sub/osd_font.h: TOOLS/file2string.py sub/osd_font.pfb
	./$^ >$@

# ./configure must be rerun if it changed
config.mak: configure
	@echo "############################################################"
	@echo "####### Please run ./configure again - it's changed! #######"
	@echo "############################################################"

version.h .version: version.sh
	./$<

# Force version.sh to run to potentially regenerate version.h
-include .version

%$(EXESUF): %.c
	$(CC) $(CFLAGS) -o $@ $^

locales: $(MOFILES)

locale/%/LC_MESSAGES/mplayer.mo: po/%.po
	mkdir -p $(dir $@)
	msgfmt -c -o $@ $<

%.ho: %.h
	$(CC) $(CFLAGS) -Wno-unused -c -o $@ -x c $<

checkheaders: $(ALLHEADERS:.h=.ho)



###### dependency declarations / specific CFLAGS ######

version.c osdep/mplayer-rc.o: version.h

# Files that depend on libavcodec internals
libmpcodecs/vf_fspp.o libmpcodecs/vf_mcdeint.o libmpcodecs/vf_spp.o: CFLAGS := -I$(FFMPEG_SOURCE_PATH) $(CFLAGS)

osdep/mplayer-rc.o: osdep/mplayer.exe.manifest


###### installation / clean / generic rules #######

check_rst2man:
	@which rst2man > /dev/null 2>&1 || (printf "\n\trst2man not found. You need the docutils (>= 0.7) to generate the manpages. Alternatively you can use 'install-no-man' rule.\n\n" && exit 1)

install: $(INSTALL_TARGETS-yes)

install-no-man: $(INSTALL_NO_MAN_TARGETS-yes)

install-dirs:
	if test ! -d $(BINDIR) ; then $(INSTALL) -d $(BINDIR) ; fi
	if test ! -d $(CONFDIR) ; then $(INSTALL) -d $(CONFDIR) ; fi
	if test ! -d $(LIBDIR) ; then $(INSTALL) -d $(LIBDIR) ; fi

install-%: %$(EXESUF) install-dirs
	$(INSTALL) -m 755 $(INSTALLSTRIP) $< $(BINDIR)

install-mplayer-man:  $(foreach lang,$(MAN_LANGS),install-mplayer-man-$(lang))
install-mplayer-msg:  $(foreach lang,$(MSG_LANGS),install-mplayer-msg-$(lang))

install-mplayer-man-en: DOCS/man/en/mplayer.1
	if test ! -d $(MANDIR)/man1 ; then $(INSTALL) -d $(MANDIR)/man1 ; fi
	$(INSTALL) -m 644 DOCS/man/en/mplayer.1 $(MANDIR)/man1/

define MPLAYER_MAN_RULE
install-mplayer-man-$(lang): DOCS/man/$(lang)/mplayer.1
	if test ! -d $(MANDIR)/$(lang)/man1 ; then $(INSTALL) -d $(MANDIR)/$(lang)/man1 ; fi
	$(INSTALL) -m 644 DOCS/man/$(lang)/mplayer.1 $(MANDIR)/$(lang)/man1/
endef

$(foreach lang,$(filter-out en,$(MAN_LANG_ALL)),$(eval $(MPLAYER_MAN_RULE)))

define MPLAYER_MSG_RULE
install-mplayer-msg-$(lang):
	if test ! -d $(LOCALEDIR)/$(lang)/LC_MESSAGES ; then $(INSTALL) -d $(LOCALEDIR)/$(lang)/LC_MESSAGES ; fi
	$(INSTALL) -m 644 locale/$(lang)/LC_MESSAGES/mplayer.mo $(LOCALEDIR)/$(lang)/LC_MESSAGES/
endef

$(foreach lang,$(MSG_LANG_ALL),$(eval $(MPLAYER_MSG_RULE)))

uninstall:
	$(RM) $(BINDIR)/mplayer$(EXESUF) $(BINDIR)/gmplayer$(EXESUF)
	$(RM) $(prefix)/share/pixmaps/mplayer.xpm
	$(RM) $(prefix)/share/applications/mplayer.desktop
	$(RM) $(MANDIR)/man1/mplayer.1
	$(RM) $(foreach lang,$(MAN_LANGS),$(foreach man,mplayer.1,$(MANDIR)/$(lang)/man1/$(man)))
	$(RM) $(foreach lang,$(MSG_LANGS),$(LOCALEDIR)/$(lang)/LC_MESSAGES/mplayer.1)

clean:
	-$(RM) $(call ADD_ALL_DIRS,/*.o /*.d /*.a /*.ho /*~)
	-$(RM) $(foreach lang,$(MAN_LANGS),$(foreach man,mplayer.1,DOCS/man/$(lang)/$(man)))
	-$(RM) $(call ADD_ALL_DIRS,/*.o /*.a /*.ho /*~)
	-$(RM) $(call ADD_ALL_EXESUFS,mplayer)
	-$(RM) $(MOFILES)
	-$(RM) version.h
	-$(RM) codecs.conf.h
	-$(RM) input/input_conf.h
	-$(RM) libvo/vdpau_template.c
	-$(RM) libmpdemux/ebml_types.h libmpdemux/ebml_defs.c
	-$(RM) libvo/vo_gl3_shaders.h
	-$(RM) sub/osd_font.h

distclean: clean
	-$(RM) -r locale
	-$(RM) config.log config.mak config.h TAGS tags

TAGS:
	$(RM) $@; find . -name '*.[chS]' -o -name '*.asm' | xargs etags -a

tags:
	$(RM) $@; find . -name '*.[chS]' -o -name '*.asm' | xargs ctags -a

-include $(DEP_FILES)

.PHONY: all locales *install*
.PHONY: checkheaders *clean .version

# Disable suffix rules.  Most of the builtin rules are suffix rules,
# so this saves some time on slow systems.
.SUFFIXES:

# If a command returns failure but changed its target file, delete the
# (presumably malformed) file. Otherwise the file would be considered to
# be up to date if make is restarted.

.DELETE_ON_ERROR:
