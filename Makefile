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
SOURCES-$(DVBIN)                += stream/dvb_tune.c \
                                   stream/stream_dvb.c
SOURCES-$(DVDREAD)              += stream/stream_dvd.c \
                                   stream/stream_dvd_common.c

SOURCES-$(HAVE_SYS_MMAN_H)      += audio/filter/af_export.c
SOURCES-$(LADSPA)               += audio/filter/af_ladspa.c
SOURCES-$(LIBASS)               += sub/ass_mp.c sub/sd_ass.c \
                                   demux/demux_libass.c

SOURCES-$(LIBBLURAY)            += stream/stream_bluray.c
SOURCES-$(LIBBS2B)              += audio/filter/af_bs2b.c

SOURCES-$(LIBPOSTPROC)          += video/filter/vf_pp.c
SOURCES-$(LIBSMBCLIENT)         += stream/stream_smb.c

SOURCES-$(COCOA)                += video/out/cocoa_common.m \
                                   osdep/path-macosx.m \
                                   osdep/macosx_application.m \
                                   osdep/macosx_events.m \
                                   osdep/ar/HIDRemote.m
SOURCES-$(MNG)                  += demux/demux_mng.c
SOURCES-$(MPG123)               += audio/decode/ad_mpg123.c

SOURCES-$(NEED_GETTIMEOFDAY)    += osdep/gettimeofday.c
SOURCES-$(NEED_GLOB)            += osdep/glob-win.c
SOURCES-$(WIN32)                += osdep/path-win.c

SOURCES-$(PRIORITY)             += osdep/priority.c
SOURCES-$(PVR)                  += stream/stream_pvr.c
SOURCES-$(RADIO)                += stream/stream_radio.c
SOURCES-$(RADIO_CAPTURE)        += stream/audio_in.c
SOURCES-$(STREAM_CACHE)         += stream/cache.c

SOURCES-$(TV)                   += stream/stream_tv.c stream/tv.c \
                                   stream/frequencies.c stream/tvi_dummy.c

SOURCES-$(TV_V4L2)              += stream/tvi_v4l2.c stream/audio_in.c
SOURCES-$(VCD)                  += stream/stream_vcd.c
SOURCES-$(DUMMY_OSD)            += sub/osd_dummy.c
SOURCES-$(LIBASS_OSD)           += sub/osd_libass.c

SOURCES-$(ALSA)                 += audio/out/ao_alsa.c
SOURCES-$(CACA)                 += video/out/vo_caca.c
SOURCES-$(SDL)                  += audio/out/ao_sdl.c
SOURCES-$(SDL2)                 += video/out/vo_sdl.c
SOURCES-$(COREAUDIO)            += audio/out/ao_coreaudio.c \
                                   audio/out/ao_coreaudio_utils.c \
                                   audio/out/ao_coreaudio_properties.c
SOURCES-$(COREVIDEO)            += video/out/vo_corevideo.c
SOURCES-$(DIRECT3D)             += video/out/vo_direct3d.c \
                                   video/out/w32_common.c
SOURCES-$(DSOUND)               += audio/out/ao_dsound.c
SOURCES-$(WASAPI)               += audio/out/ao_wasapi.c
SOURCES-$(GL)                   += video/out/gl_common.c video/out/gl_osd.c \
                                   video/out/vo_opengl.c video/out/gl_lcms.c \
                                   video/out/gl_video.c video/out/dither.c \
                                   video/out/vo_opengl_old.c \
                                   video/out/pnm_loader.c

SOURCES-$(ENCODING)             += video/out/vo_lavc.c audio/out/ao_lavc.c \
                                   mpvcore/encode_lavc.c

SOURCES-$(GL_WIN32)             += video/out/w32_common.c video/out/gl_w32.c
SOURCES-$(GL_X11)               += video/out/x11_common.c video/out/gl_x11.c
SOURCES-$(GL_COCOA)             += video/out/gl_cocoa.c
SOURCES-$(GL_WAYLAND)           += video/out/wayland_common.c \
                                   video/out/gl_wayland.c

SOURCES-$(JACK)                 += audio/out/ao_jack.c
SOURCES-$(JOYSTICK)             += mpvcore/input/joystick.c
SOURCES-$(LIBQUVI)              += mpvcore/resolve_quvi.c
SOURCES-$(LIBQUVI9)             += mpvcore/resolve_quvi9.c
SOURCES-$(LIRC)                 += mpvcore/input/lirc.c
SOURCES-$(OPENAL)               += audio/out/ao_openal.c
SOURCES-$(OSS)                  += audio/out/ao_oss.c
SOURCES-$(PULSE)                += audio/out/ao_pulse.c
SOURCES-$(PORTAUDIO)            += audio/out/ao_portaudio.c
SOURCES-$(RSOUND)               += audio/out/ao_rsound.c
SOURCES-$(VDPAU)                += video/vdpau.c video/out/vo_vdpau.c
SOURCES-$(VDA)                  += video/decode/vda.c
SOURCES-$(VDPAU_DEC)            += video/decode/vdpau.c
SOURCES-$(VDPAU_DEC_OLD)        += video/decode/vdpau_old.c
SOURCES-$(VAAPI)                += video/out/vo_vaapi.c \
                                   video/decode/vaapi.c

SOURCES-$(X11)                  += video/out/vo_x11.c video/out/x11_common.c
SOURCES-$(XV)                   += video/out/vo_xv.c
SOURCES-$(WAYLAND)              += video/out/vo_wayland.c video/out/wayland_common.c

SOURCES-$(VF_LAVFI)             += video/filter/vf_lavfi.c
SOURCES-$(AF_LAVFI)             += audio/filter/af_lavfi.c

ifeq ($(HAVE_AVUTIL_REFCOUNTING),no)
    SOURCES-yes                 += video/decode/lavc_dr1.c
endif

SOURCES-$(DLOPEN)               += video/filter/vf_dlopen.c

SOURCES = talloc.c \
          audio/audio.c \
          audio/chmap.c \
          audio/chmap_sel.c \
          audio/fmt-conversion.c \
          audio/format.c \
          audio/mixer.c \
          audio/reorder_ch.c \
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
          audio/filter/af_force.c \
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
          demux/codec_tags.c \
          demux/demux.c \
          demux/demux_edl.c \
          demux/demux_cue.c \
          demux/demux_lavf.c \
          demux/demux_mf.c \
          demux/demux_mkv.c \
          demux/demux_playlist.c \
          demux/demux_raw.c \
          demux/demux_subreader.c \
          demux/ebml.c \
          demux/mf.c \
          mpvcore/asxparser.c \
          mpvcore/av_common.c \
          mpvcore/av_log.c \
          mpvcore/av_opts.c \
          mpvcore/bstr.c \
          mpvcore/charset_conv.c \
          mpvcore/codecs.c \
          mpvcore/command.c \
          mpvcore/cpudetect.c \
          mpvcore/m_config.c \
          mpvcore/m_option.c \
          mpvcore/m_property.c \
          mpvcore/mp_common.c \
          mpvcore/mp_msg.c \
          mpvcore/mp_ring.c \
          mpvcore/mplayer.c \
          mpvcore/options.c \
          mpvcore/parser-cfg.c \
          mpvcore/parser-mpcmd.c \
          mpvcore/path.c \
          mpvcore/playlist.c \
          mpvcore/playlist_parser.c \
          mpvcore/screenshot.c \
          mpvcore/version.c \
          mpvcore/input/input.c \
          mpvcore/timeline/tl_edl.c \
          mpvcore/timeline/tl_matroska.c \
          mpvcore/timeline/tl_cue.c \
          osdep/io.c \
          osdep/numcores.c \
          osdep/timer.c \
          stream/cookies.c \
          stream/rar.c \
          stream/stream.c \
          stream/stream_avdevice.c \
          stream/stream_file.c \
          stream/stream_lavf.c \
          stream/stream_memory.c \
          stream/stream_mf.c \
          stream/stream_null.c \
          stream/stream_rar.c \
          sub/dec_sub.c \
          sub/draw_bmp.c \
          sub/find_subfiles.c \
          sub/img_convert.c \
          sub/sd_lavc.c \
          sub/sd_lavc_conv.c \
          sub/sd_lavf_srt.c \
          sub/sd_microdvd.c \
          sub/sd_movtext.c \
          sub/sd_spu.c \
          sub/sd_srt.c \
          sub/spudec.c \
          sub/sub.c \
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
INSTALL_PDF      =

ifeq ($(BUILD_MAN),yes)
    INSTALL_MAN += install-mpv-man
    ALL_TARGETS += DOCS/man/en/mpv.1
endif

ifeq ($(BUILD_PDF),yes)
    INSTALL_PDF += install-mpv-pdf
    ALL_TARGETS += DOCS/man/en/mpv.pdf
endif

DIRS =  . \
        audio \
        audio/decode \
        audio/filter \
        audio/out \
        mpvcore \
        mpvcore/input \
        mpvcore/timeline \
        demux \
        osdep \
        osdep/ar \
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

%.tex: %.rst
	$(RST2LATEX) --config=DOCS/man/docutils.conf $< $@

%.pdf: %.tex
	pdflatex -interaction=batchmode -jobname=$(basename $@) $<; pdflatex -interaction=batchmode -jobname=$(basename $@) $<

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

mpvcore/input/input.c: mpvcore/input/input_conf.h
mpvcore/input/input_conf.h: TOOLS/file2string.pl etc/input.conf
	./$^ >$@

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

video/out/x11_common.c: video/out/x11_icon.inc
video/out/x11_icon.inc: TOOLS/file2string.pl video/out/x11_icon.bin
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

mpvcore/version.c osdep/mpv-rc.o: version.h

osdep/mpv-rc.o: osdep/mpv.exe.manifest etc/mpv-icon.ico

DOCS/man/en/mpv.1 DOCS/man/en/mpv.pdf: DOCS/man/en/af.rst \
                                       DOCS/man/en/ao.rst \
                                       DOCS/man/en/changes.rst \
                                       DOCS/man/en/encode.rst \
                                       DOCS/man/en/input.rst \
                                       DOCS/man/en/options.rst \
                                       DOCS/man/en/vf.rst \
                                       DOCS/man/en/vo.rst

###### installation / clean / generic rules #######

install:               $(INSTALL_BIN)       install-data $(INSTALL_MAN) $(INSTALL_PDF)
install-no-man:        $(INSTALL_BIN)       install-data
install-strip:         $(INSTALL_BIN_STRIP) install-data $(INSTALL_MAN) $(INSTALL_PDF)
install-strip-no-man:  $(INSTALL_BIN_STRIP) install-data

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

install-mpv-pdf:  install-mpv-pdf-en

install-mpv-pdf-en: DOCS/man/en/mpv.pdf
	if test ! -d $(DOCDIR)/mpv ; then $(INSTALL) -d $(DOCDIR)/mpv ; fi
	$(INSTALL) -m 644 DOCS/man/en/mpv.pdf $(DOCDIR)/mpv/

ICONSIZES = 16x16 32x32 64x64

define ICON_INSTALL_RULE
install-mpv-icon-$(size): etc/mpv-icon-8bit-$(size).png
	$(INSTALL) -d $(prefix)/share/icons/hicolor/$(size)/apps
	$(INSTALL) -m 644 etc/mpv-icon-8bit-$(size).png $(prefix)/share/icons/hicolor/$(size)/apps/mpv.png
endef

$(foreach size,$(ICONSIZES),$(eval $(ICON_INSTALL_RULE)))

install-mpv-icons: $(foreach size,$(ICONSIZES),install-mpv-icon-$(size))

install-mpv-desktop: etc/mpv.desktop
	$(INSTALL) -d $(prefix)/share/applications
	$(INSTALL) -m 644 etc/mpv.desktop $(prefix)/share/applications/

install-data: install-mpv-icons install-mpv-desktop

uninstall:
	$(RM) $(BINDIR)/mpv$(EXESUF)
	$(RM) $(MANDIR)/man1/mpv.1
	$(RM) $(MANDIR)/en/man1/mpv.1
	$(RM) $(DOCDIR)/mpv/mpv.pdf
	$(RM) $(prefix)/share/applications/mpv.desktop
	$(RM) $(foreach size,$(ICONSIZES),$(prefix)/share/icons/hicolor/$(size)/apps/mpv.png)

clean:
	-$(RM) $(call ADD_ALL_DIRS,/*.o /*.d /*.a /*.ho /*~)
	-$(RM) $(call ADD_ALL_DIRS,/*.o /*.a /*.ho /*~)
	-$(RM) $(call ADD_ALL_EXESUFS,mpv)
	-$(RM) $(call ADDSUFFIXES,.pdf .tex .log .aux .out .toc,DOCS/man/*/mpv)
	-$(RM) DOCS/man/*/mpv.1
	-$(RM) version.h
	-$(RM) mpvcore/input/input_conf.h
	-$(RM) video/out/vdpau_template.c
	-$(RM) demux/ebml_types.h demux/ebml_defs.c
	-$(RM) video/out/gl_video_shaders.h
	-$(RM) video/out/x11_icon.inc
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
