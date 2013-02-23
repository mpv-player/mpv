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

SOURCES_AUDIO_INPUT-$(ALSA)     += stream/ai_alsa1x.c
SOURCES_AUDIO_INPUT-$(OSS)      += stream/ai_oss.c
SOURCES-$(AUDIO_INPUT)          += $(SOURCES_AUDIO_INPUT-yes)
SOURCES-$(CDDA)                 += stream/stream_cdda.c \
                                   stream/cdinfo.c
SOURCES-$(CDDB)                 += stream/stream_cddb.c
SOURCES-$(DVBIN)                += stream/dvb_tune.c \
                                   stream/stream_dvb.c
SOURCES-$(DVDREAD)              += stream/stream_dvd.c \
                                   stream/stream_dvd_common.c

SOURCES-$(FTP)                  += stream/stream_ftp.c
SOURCES-$(HAVE_SYS_MMAN_H)      += audio/filter/af_export.c osdep/mmap_anon.c
SOURCES-$(LADSPA)               += audio/filter/af_ladspa.c
SOURCES-$(LIBASS)               += sub/ass_mp.c sub/sd_ass.c

SOURCES-$(LIBBLURAY)            += stream/stream_bluray.c
SOURCES-$(LIBBS2B)              += audio/filter/af_bs2b.c

SOURCES-$(LIBPOSTPROC)          += video/filter/vf_pp.c
SOURCES-$(LIBSMBCLIENT)         += stream/stream_smb.c

SOURCES-$(MACOSX_BUNDLE)        += osdep/macosx_bundle.m
SOURCES-$(COCOA)                += video/out/osx_common.m \
                                   video/out/cocoa_common.m \
                                   osdep/macosx_application.m
SOURCES-$(MNG)                  += demux/demux_mng.c
SOURCES-$(MPG123)               += audio/decode/ad_mpg123.c

SOURCES-$(NEED_GETTIMEOFDAY)    += osdep/gettimeofday.c
SOURCES-$(NEED_GLOB)            += osdep/glob-win.c
SOURCES-$(NEED_SHMEM)           += osdep/shmem.c
SOURCES-$(NETWORKING)           += stream/asf_mmst_streaming.c \
                                   stream/asf_streaming.c \
                                   stream/cookies.c \
                                   stream/http.c \
                                   stream/network.c \
                                   stream/udp.c \
                                   stream/tcp.c \
                                   stream/stream_udp.c \

SOURCES-$(PRIORITY)             += osdep/priority.c
SOURCES-$(PVR)                  += stream/stream_pvr.c
SOURCES-$(RADIO)                += stream/stream_radio.c
SOURCES-$(RADIO_CAPTURE)        += stream/audio_in.c
SOURCES-$(STREAM_CACHE)         += stream/cache2.c

SOURCES-$(TV)                   += stream/stream_tv.c stream/tv.c \
                                   stream/frequencies.c stream/tvi_dummy.c

SOURCES-$(TV_V4L2)              += stream/tvi_v4l2.c stream/audio_in.c
SOURCES-$(VCD)                  += stream/stream_vcd.c
SOURCES-$(VSTREAM)              += stream/stream_vstream.c
SOURCES-$(DUMMY_OSD)            += sub/osd_dummy.c
SOURCES-$(LIBASS_OSD)           += sub/osd_libass.c

SOURCES-$(ALSA)                 += audio/out/ao_alsa.c
SOURCES-$(CACA)                 += video/out/vo_caca.c
SOURCES-$(SDL)                  += audio/out/ao_sdl.c
SOURCES-$(SDL2)                 += video/out/vo_sdl.c
SOURCES-$(COREAUDIO)            += audio/out/ao_coreaudio.c
SOURCES-$(COREVIDEO)            += video/out/vo_corevideo.m
SOURCES-$(DIRECT3D)             += video/out/vo_direct3d.c \
                                   video/out/w32_common.c
SOURCES-$(DSOUND)               += audio/out/ao_dsound.c
SOURCES-$(GL)                   += video/out/gl_common.c video/out/gl_osd.c \
                                   video/out/vo_opengl.c video/out/gl_lcms.c \
                                   video/out/gl_video.c \
                                   video/out/vo_opengl_old.c \
                                   video/out/pnm_loader.c

SOURCES-$(ENCODING)             += video/out/vo_lavc.c audio/out/ao_lavc.c \
                                   core/encode_lavc.c

SOURCES-$(GL_WIN32)             += video/out/w32_common.c video/out/gl_w32.c
SOURCES-$(GL_X11)               += video/out/x11_common.c video/out/gl_x11.c
SOURCES-$(GL_COCOA)             += video/out/gl_cocoa.c
SOURCES-$(GL_WAYLAND)           += video/out/wayland_common.c \
                                   video/out/gl_wayland.c

SOURCES-$(JACK)                 += audio/out/ao_jack.c
SOURCES-$(JOYSTICK)             += core/input/joystick.c
SOURCES-$(LIBQUVI)              += core/quvi.c
SOURCES-$(LIRC)                 += core/input/lirc.c
SOURCES-$(OPENAL)               += audio/out/ao_openal.c
SOURCES-$(OSS)                  += audio/out/ao_oss.c
SOURCES-$(PULSE)                += audio/out/ao_pulse.c
SOURCES-$(PORTAUDIO)            += audio/out/ao_portaudio.c
SOURCES-$(RSOUND)               += audio/out/ao_rsound.c
SOURCES-$(VDPAU)                += video/out/vo_vdpau.c

SOURCES-$(X11)                  += video/out/vo_x11.c video/out/x11_common.c
SOURCES-$(XV)                   += video/out/vo_xv.c

SOURCES-$(VF_LAVFI)             += video/filter/vf_lavfi.c

ifeq ($(HAVE_AVUTIL_REFCOUNTING),no)
    SOURCES-yes                 += video/decode/lavc_dr1.c
endif

SOURCES = talloc.c \
          audio/format.c \
          audio/mixer.c \
          audio/reorder_ch.c \
          audio/decode/ad.c \
          audio/decode/ad_lavc.c \
          audio/decode/ad_spdif.c      \
          audio/decode/dec_audio.c \
          audio/filter/af.c \
          audio/filter/af_center.c \
          audio/filter/af_channels.c \
          audio/filter/af_delay.c \
          audio/filter/af_dummy.c \
          audio/filter/af_equalizer.c \
          audio/filter/af_extrastereo.c \
          audio/filter/af_format.c \
          audio/filter/af_hrtf.c \
          audio/filter/af_karaoke.c \
          audio/filter/af_lavcac3enc.c \
          audio/filter/af_lavrresample.c \
          audio/filter/af_pan.c \
          audio/filter/af_scaletempo.c \
          audio/filter/af_sinesuppress.c \
          audio/filter/af_sub.c \
          audio/filter/af_surround.c \
          audio/filter/af_sweep.c \
          audio/filter/af_tools.c \
          audio/filter/af_drc.c \
          audio/filter/af_volume.c \
          audio/filter/filter.c \
          audio/filter/window.c \
          audio/out/ao.c \
          audio/out/ao_null.c \
          audio/out/ao_pcm.c \
          core/asxparser.c \
          core/av_common.c \
          core/av_log.c \
          core/av_opts.c \
          core/bstr.c \
          core/codecs.c \
          core/command.c \
          core/cpudetect.c \
          core/defaultopts.c \
          core/m_config.c \
          core/m_option.c \
          core/m_property.c \
          core/m_struct.c \
          core/mp_common.c \
          core/mp_fifo.c \
          core/mp_msg.c \
          core/mplayer.c \
          core/parser-cfg.c \
          core/parser-mpcmd.c \
          core/path.c \
          core/playlist.c \
          core/playlist_parser.c \
          core/screenshot.c \
          core/subopt-helper.c \
          core/version.c \
          core/input/input.c \
          core/timeline/tl_edl.c \
          core/timeline/tl_matroska.c \
          core/timeline/tl_cue.c \
          demux/asfheader.c \
          demux/aviheader.c \
          demux/aviprint.c \
          demux/codec_tags.c \
          demux/demux.c \
          demux/demux_asf.c \
          demux/demux_avi.c \
          demux/demux_edl.c \
          demux/demux_cue.c \
          demux/demux_lavf.c \
          demux/demux_mf.c \
          demux/demux_mkv.c \
          demux/demux_mpg.c \
          demux/demux_ts.c \
          demux/mp3_hdr.c \
          demux/parse_es.c \
          demux/mpeg_hdr.c \
          demux/demux_rawaudio.c \
          demux/demux_rawvideo.c \
          demux/ebml.c \
          demux/extension.c \
          demux/mf.c \
          demux/video.c \
          osdep/numcores.c \
          osdep/io.c \
          stream/stream.c \
          stream/stream_avdevice.c \
          stream/stream_file.c \
          stream/stream_lavf.c \
          stream/stream_mf.c \
          stream/stream_null.c \
          stream/url.c \
          sub/dec_sub.c \
          sub/draw_bmp.c \
          sub/find_sub.c \
          sub/find_subfiles.c \
          sub/img_convert.c \
          sub/sd_lavc.c \
          sub/spudec.c \
          sub/sub.c \
          sub/subassconvert.c \
          sub/subreader.c \
          video/csputils.c \
          video/fmt-conversion.c \
          video/image_writer.c \
          video/img_format.c \
          video/mp_image.c \
          video/mp_image_pool.c \
          video/sws_utils.c \
          video/decode/dec_video.c \
          video/decode/vd.c \
          video/decode/vd_lavc.c \
          video/filter/vf.c \
          video/filter/pullup.c \
          video/filter/vf_crop.c \
          video/filter/vf_delogo.c \
          video/filter/vf_divtc.c \
          video/filter/vf_dlopen.c \
          video/filter/vf_down3dright.c \
          video/filter/vf_dsize.c \
          video/filter/vf_eq.c \
          video/filter/vf_expand.c \
          video/filter/vf_flip.c \
          video/filter/vf_format.c \
          video/filter/vf_gradfun.c \
          video/filter/vf_hqdn3d.c \
          video/filter/vf_ilpack.c \
          video/filter/vf_mirror.c \
          video/filter/vf_noformat.c \
          video/filter/vf_noise.c \
          video/filter/vf_phase.c \
          video/filter/vf_pullup.c \
          video/filter/vf_rotate.c \
          video/filter/vf_scale.c \
          video/filter/vf_screenshot.c \
          video/filter/vf_softpulldown.c \
          video/filter/vf_stereo3d.c \
          video/filter/vf_sub.c \
          video/filter/vf_swapuv.c \
          video/filter/vf_unsharp.c \
          video/filter/vf_vo.c \
          video/filter/vf_yadif.c \
          video/out/bitmap_packer.c \
          video/out/aspect.c \
          video/out/filter_kernels.c \
          video/out/vo.c \
          video/out/vo_null.c \
          video/out/vo_image.c \
          osdep/$(GETCH) \
          osdep/$(TIMER) \
          $(SOURCES-yes)

OBJECTS         += $(addsuffix .o, $(basename $(SOURCES)))
OBJECTS-$(PE_EXECUTABLE) += osdep/mpv-rc.o
OBJECTS         += $(OBJECTS-yes)

DEP_FILES = $(patsubst %.S,%.d,$(patsubst %.cpp,%.d,$(patsubst %.c,%.d,$(SOURCES:.m=.d) $(SOURCES:.m=.d))))

ALL_TARGETS     += mpv$(EXESUF)

INSTALL_BIN     += install-mpv
INSTALL_BIN_STRIP += install-mpv-strip
INSTALL_MAN      =

ifeq ($(BUILD_MAN),yes)
    INSTALL_MAN += install-mpv-man
    ALL_TARGETS += DOCS/man/en/mpv.1
endif

DIRS =  . \
        audio \
        audio/decode \
        audio/filter \
        audio/out \
        core \
        core/input \
        core/timeline \
        demux \
        osdep \
        stream \
        sub \
        video \
        video/decode \
        video/filter \
        video/out


ADDSUFFIXES     = $(foreach suf,$(1),$(addsuffix $(suf),$(2)))
ADD_ALL_DIRS    = $(call ADDSUFFIXES,$(1),$(DIRS))
ADD_ALL_EXESUFS = $(1) $(call ADDSUFFIXES,$(EXESUFS_ALL),$(1))

###### brief build output #######

ifndef V
$(eval override CC = @printf "CC\t$$@\n"; $(CC))
$(eval override RM = @$(RM))
endif

###### generic rules #######

all: $(ALL_TARGETS)

%.1: %.rst
	$(RST2MAN) $< $@

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

mpv$(EXESUF): $(OBJECTS)
mpv$(EXESUF):
	$(CC) -o $@ $^ $(EXTRALIBS)

core/input/input.c: core/input/input_conf.h
core/input/input_conf.h: TOOLS/file2string.pl etc/input.conf
	./$^ >$@

video/out/vo_vdpau.c: video/out/vdpau_template.c
video/out/vdpau_template.c: TOOLS/vdpau_functions.pl
	./$< > $@

MKVLIB_DEPS = TOOLS/lib/Parse/Matroska.pm \
              TOOLS/lib/Parse/Matroska/Definitions.pm \
              TOOLS/lib/Parse/Matroska/Element.pm \
              TOOLS/lib/Parse/Matroska/Reader.pm \
              TOOLS/lib/Parse/Matroska/Utils.pm \

demux/ebml.c demux/demux_mkv.c: demux/ebml_types.h
demux/ebml_types.h: TOOLS/matroska.pl $(MKVLIB_DEPS)
	./$< --generate-header > $@

demux/ebml.c: demux/ebml_defs.c
demux/ebml_defs.c: TOOLS/matroska.pl $(MKVLIB_DEPS)
	./$< --generate-definitions > $@

video/out/gl_video.c: video/out/gl_video_shaders.h
video/out/gl_video_shaders.h: TOOLS/file2string.pl video/out/gl_video_shaders.glsl
	./$^ >$@

sub/osd_libass.c: sub/osd_font.h
sub/osd_font.h: TOOLS/file2string.pl sub/osd_font.otf
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


###### dependency declarations / specific CFLAGS ######

core/version.c osdep/mpv-rc.o: version.h

osdep/mpv-rc.o: osdep/mpv.exe.manifest

DOCS/man/en/mpv.1: DOCS/man/en/af.rst \
                   DOCS/man/en/ao.rst \
                   DOCS/man/en/changes.rst \
                   DOCS/man/en/encode.rst \
                   DOCS/man/en/input.rst \
                   DOCS/man/en/options.rst \
                   DOCS/man/en/vf.rst \
                   DOCS/man/en/vo.rst


###### installation / clean / generic rules #######

install:               $(INSTALL_BIN)       $(INSTALL_MAN)
install-no-man:        $(INSTALL_BIN)
install-strip:         $(INSTALL_BIN_STRIP) $(INSTALL_MAN)
install-strip-no-man:  $(INSTALL_BIN_STRIP)

install-dirs:
	if test ! -d $(BINDIR) ; then $(INSTALL) -d $(BINDIR) ; fi

install-%: %$(EXESUF) install-dirs
	$(INSTALL) -m 755 $< $(BINDIR)

install-%-strip: %$(EXESUF) install-dirs
	$(INSTALL) -m 755 -s $< $(BINDIR)

install-mpv-man:  install-mpv-man-en

install-mpv-man-en: DOCS/man/en/mpv.1
	if test ! -d $(MANDIR)/man1 ; then $(INSTALL) -d $(MANDIR)/man1 ; fi
	$(INSTALL) -m 644 DOCS/man/en/mpv.1 $(MANDIR)/man1/

uninstall:
	$(RM) $(BINDIR)/mpv$(EXESUF)
	$(RM) $(MANDIR)/man1/mpv.1
	$(RM) $(MANDIR)/en/man1/mpv.1

clean:
	-$(RM) $(call ADD_ALL_DIRS,/*.o /*.d /*.a /*.ho /*~)
	-$(RM) $(call ADD_ALL_DIRS,/*.o /*.a /*.ho /*~)
	-$(RM) $(call ADD_ALL_EXESUFS,mpv)
	-$(RM) DOCS/man/en/mpv.1
	-$(RM) version.h
	-$(RM) core/input/input_conf.h
	-$(RM) video/out/vdpau_template.c
	-$(RM) demux/ebml_types.h demux/ebml_defs.c
	-$(RM) video/out/gl_video_shaders.h
	-$(RM) sub/osd_font.h

distclean: clean
	-$(RM) config.log config.mak config.h TAGS tags

TAGS:
	$(RM) $@; find . -name '*.[chS]' -o -name '*.asm' | xargs etags -a

tags:
	$(RM) $@; find . -name '*.[chS]' -o -name '*.asm' | xargs ctags -a

osxbundle:
	@TOOLS/osxbundle.py mpv

osxbundle-skip-deps:
	@TOOLS/osxbundle.py --skip-deps mpv

-include $(DEP_FILES)

.PHONY: all *install* *clean .version

# Disable suffix rules.  Most of the builtin rules are suffix rules,
# so this saves some time on slow systems.
.SUFFIXES:

# If a command returns failure but changed its target file, delete the
# (presumably malformed) file. Otherwise the file would be considered to
# be up to date if make is restarted.

.DELETE_ON_ERROR:
