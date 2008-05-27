# LINUX Makefile made by A'rpi / Astral
# Some cleanup by LGB: 	* 'make -C dir' instead of 'cd dir;make;cd..'
#			* for loops instead of linear sequence of make directories
#			* some minor problems with make clean and distclean were corrected
#			* DVD support

include config.mak


###### variable declarations #######

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
              libaf/af.c \
              libaf/af_center.c \
              libaf/af_channels.c \
              libaf/af_comp.c \
              libaf/af_delay.c \
              libaf/af_dummy.c \
              libaf/af_equalizer.c \
              libaf/af_extrastereo.c \
              libaf/af_format.c \
              libaf/af_gate.c \
              libaf/af_hrtf.c \
              libaf/af_karaoke.c \
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
              libmpcodecs/ad_alaw.c \
              libmpcodecs/ad_dk3adpcm.c \
              libmpcodecs/ad_dvdpcm.c \
              libmpcodecs/ad_hwac3.c \
              libmpcodecs/ad_hwmpa.c \
              libmpcodecs/ad_imaadpcm.c \
              libmpcodecs/ad_msadpcm.c \
              libmpcodecs/ad_msgsm.c \
              libmpcodecs/ad_pcm.c \
              libmpcodecs/dec_audio.c \
              libmpcodecs/dec_video.c \
              libmpcodecs/img_format.c \
              libmpcodecs/mp_image.c \
              libmpcodecs/native/nuppelvideo.c \
              libmpcodecs/native/rtjpegn.c \
              libmpcodecs/native/xa_gsm.c \
              libmpcodecs/pullup.c \
              libmpcodecs/vd.c \
              libmpcodecs/vd_hmblck.c \
              libmpcodecs/vd_lzo.c \
              libmpcodecs/vd_mpegpes.c \
              libmpcodecs/vd_mtga.c \
              libmpcodecs/vd_null.c \
              libmpcodecs/vd_nuv.c \
              libmpcodecs/vd_raw.c \
              libmpcodecs/vd_sgi.c \
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
              libmpcodecs/vf_down3dright.c \
              libmpcodecs/vf_dsize.c \
              libmpcodecs/vf_dvbscale.c \
              libmpcodecs/vf_eq.c \
              libmpcodecs/vf_eq2.c \
              libmpcodecs/vf_expand.c \
              libmpcodecs/vf_field.c \
              libmpcodecs/vf_fil.c \
              libmpcodecs/vf_filmdint.c \
              libmpcodecs/vf_flip.c \
              libmpcodecs/vf_format.c \
              libmpcodecs/vf_framestep.c \
              libmpcodecs/vf_halfpack.c \
              libmpcodecs/vf_harddup.c \
              libmpcodecs/vf_hqdn3d.c \
              libmpcodecs/vf_hue.c \
              libmpcodecs/vf_il.c \
              libmpcodecs/vf_ilpack.c \
              libmpcodecs/vf_ivtc.c \
              libmpcodecs/vf_kerndeint.c \
              libmpcodecs/vf_mirror.c \
              libmpcodecs/vf_noformat.c \
              libmpcodecs/vf_noise.c \
              libmpcodecs/vf_ow.c \
              libmpcodecs/vf_palette.c \
              libmpcodecs/vf_perspective.c \
              libmpcodecs/vf_phase.c \
              libmpcodecs/vf_pp7.c \
              libmpcodecs/vf_pullup.c \
              libmpcodecs/vf_rectangle.c \
              libmpcodecs/vf_remove_logo.c \
              libmpcodecs/vf_rgb2bgr.c \
              libmpcodecs/vf_rgbtest.c \
              libmpcodecs/vf_rotate.c \
              libmpcodecs/vf_sab.c \
              libmpcodecs/vf_scale.c \
              libmpcodecs/vf_smartblur.c \
              libmpcodecs/vf_softpulldown.c \
              libmpcodecs/vf_softskip.c \
              libmpcodecs/vf_swapuv.c \
              libmpcodecs/vf_telecine.c \
              libmpcodecs/vf_test.c \
              libmpcodecs/vf_tfields.c \
              libmpcodecs/vf_tile.c \
              libmpcodecs/vf_tinterlace.c \
              libmpcodecs/vf_unsharp.c \
              libmpcodecs/vf_vo.c \
              libmpcodecs/vf_yadif.c \
              libmpcodecs/vf_yuvcsp.c \
              libmpcodecs/vf_yuy2.c \
              libmpcodecs/vf_yvu9.c \
              libmpdemux/aac_hdr.c \
              libmpdemux/asfheader.c \
              libmpdemux/aviheader.c \
              libmpdemux/aviprint.c \
              libmpdemux/demuxer.c \
              libmpdemux/demux_aac.c \
              libmpdemux/demux_asf.c \
              libmpdemux/demux_audio.c \
              libmpdemux/demux_avi.c \
              libmpdemux/demux_demuxers.c \
              libmpdemux/demux_film.c \
              libmpdemux/demux_fli.c \
              libmpdemux/demux_lmlm4.c \
              libmpdemux/demux_mf.c \
              libmpdemux/demux_mkv.c \
              libmpdemux/demux_mov.c \
              libmpdemux/demux_mpg.c \
              libmpdemux/demux_nsv.c \
              libmpdemux/demux_nuv.c \
              libmpdemux/demux_pva.c \
              libmpdemux/demux_rawaudio.c \
              libmpdemux/demux_rawvideo.c \
              libmpdemux/demux_realaud.c \
              libmpdemux/demux_real.c \
              libmpdemux/demux_roq.c \
              libmpdemux/demux_smjpeg.c \
              libmpdemux/demux_ts.c \
              libmpdemux/demux_ty.c \
              libmpdemux/demux_ty_osd.c \
              libmpdemux/demux_viv.c \
              libmpdemux/demux_vqf.c \
              libmpdemux/demux_y4m.c \
              libmpdemux/ebml.c \
              libmpdemux/extension.c \
              libmpdemux/mf.c \
              libmpdemux/mp3_hdr.c \
              libmpdemux/mp_taglists.c \
              libmpdemux/mpeg_hdr.c \
              libmpdemux/mpeg_packetizer.c \
              libmpdemux/parse_es.c \
              libmpdemux/parse_mp4.c \
              libmpdemux/video.c \
              libmpdemux/yuv4mpeg.c \
              libmpdemux/yuv4mpeg_ratio.c \
              libvo/aclib.c \
              libvo/osd.c \
              libvo/sub.c \
              osdep/$(GETCH) \
              osdep/$(TIMER) \
              stream/open.c \
              stream/stream.c \
              stream/stream_cue.c \
              stream/stream_file.c \
              stream/stream_mf.c \
              stream/stream_null.c \
              stream/url.c \

SRCS_COMMON-$(AUDIO_INPUT)-$(ALSA1X) += stream/ai_alsa1x.c
SRCS_COMMON-$(AUDIO_INPUT)-$(ALSA9)  += stream/ai_alsa.c
SRCS_COMMON-$(AUDIO_INPUT)-$(OSS)    += stream/ai_oss.c
SRCS_COMMON-$(BITMAP_FONT)           += libvo/font_load.c
SRCS_COMMON-$(CDDA)                  += stream/stream_cdda.c \
                                        stream/cdinfo.c
SRCS_COMMON-$(CDDB)                  += stream/stream_cddb.c
SRCS_COMMON-$(DVBIN)                 += stream/dvb_tune.c \
                                        stream/stream_dvb.c
SRCS_COMMON-$(DVDNAV)                += stream/stream_dvdnav.c
SRCS_COMMON-$(DVDREAD)               += stream/stream_dvd.c \
                                        stream/stream_dvd_common.c
SRCS_COMMON-$(DVDREAD_INTERNAL)      += dvdread/cmd_print.c \
                                        dvdread/dvd_input.c \
                                        dvdread/dvd_reader.c \
                                        dvdread/dvd_udf.c \
                                        dvdread/ifo_print.c \
                                        dvdread/ifo_read.c \
                                        dvdread/md5.c \
                                        dvdread/nav_print.c \
                                        dvdread/nav_read.c \

SRCS_COMMON-$(FAAD)                  += libmpcodecs/ad_faad.c
SRCS_COMMON-$(FREETYPE)              += libvo/font_load_ft.c
SRCS_COMMON-$(FTP)                   += stream/stream_ftp.c
SRCS_COMMON-$(GIF)                   += libmpdemux/demux_gif.c
SRCS_COMMON-$(HAVE_POSIX_SELECT)     += libmpcodecs/vf_bmovl.c
SRCS_COMMON-$(HAVE_SYS_MMAN_H)       += osdep/mmap_anon.c
SRCS_COMMON-$(HAVE_SYS_MMAN_H)       += libaf/af_export.c
SRCS_COMMON-$(JPEG)                  += libmpcodecs/vd_ijpg.c
SRCS_COMMON-$(LADSPA)                += libaf/af_ladspa.c
SRCS_COMMON-$(LIBA52)                += libmpcodecs/ad_liba52.c \
                                        liba52/crc.c \
                                        liba52/resample.c \
                                        liba52/bit_allocate.c \
                                        liba52/bitstream.c \
                                        liba52/downmix.c \
                                        liba52/imdct.c \
                                        liba52/parse.c \

SRCS_COMMON-$(LIBASS)                += libass/ass.c \
                                        libass/ass_bitmap.c \
                                        libass/ass_cache.c \
                                        libass/ass_font.c \
                                        libass/ass_fontconfig.c \
                                        libass/ass_library.c \
                                        libass/ass_mp.c \
                                        libass/ass_render.c \
                                        libass/ass_utils.c \
                                        libmpcodecs/vf_ass.c \

SRCS_COMMON-$(LIBAVCODEC)            += av_opts.c \
                                        libaf/af_lavcresample.c \
                                        libmpcodecs/ad_ffmpeg.c \
                                        libmpcodecs/vd_ffmpeg.c \
                                        libmpcodecs/vf_lavc.c \
                                        libmpcodecs/vf_lavcdeint.c \
                                        libmpcodecs/vf_screenshot.c \

# These filters use private headers and do not work with shared libavcodec.
SRCS_COMMON-$(LIBAVCODEC_A)          += libaf/af_lavcac3enc.c \
                                        libmpcodecs/vf_fspp.c \
                                        libmpcodecs/vf_geq.c \
                                        libmpcodecs/vf_mcdeint.c \
                                        libmpcodecs/vf_qp.c \
                                        libmpcodecs/vf_spp.c \
                                        libmpcodecs/vf_uspp.c \

SRCS_COMMON-$(LIBAVFORMAT)           += libmpdemux/demux_lavf.c
SRCS_COMMON-$(LIBDCA)                += libmpcodecs/ad_libdca.c
SRCS_COMMON-$(LIBDV)                 += libmpcodecs/ad_libdv.c \
                                        libmpcodecs/vd_libdv.c \
                                        libmpdemux/demux_rawdv.c
SRCS_COMMON-$(LIBDVDCSS_INTERNAL)    += libdvdcss/css.c \
                                        libdvdcss/device.c \
                                        libdvdcss/error.c \
                                        libdvdcss/ioctl.c \
                                        libdvdcss/libdvdcss.c \

SRCS_COMMON-$(FAAD_INTERNAL)         += libfaad2/bits.c \
                                        libfaad2/cfft.c \
                                        libfaad2/common.c \
                                        libfaad2/decoder.c \
                                        libfaad2/drc.c \
                                        libfaad2/drm_dec.c \
                                        libfaad2/error.c \
                                        libfaad2/filtbank.c \
                                        libfaad2/hcr.c \
                                        libfaad2/huffman.c \
                                        libfaad2/ic_predict.c \
                                        libfaad2/is.c \
                                        libfaad2/lt_predict.c \
                                        libfaad2/mdct.c \
                                        libfaad2/mp4.c \
                                        libfaad2/ms.c \
                                        libfaad2/output.c \
                                        libfaad2/pns.c \
                                        libfaad2/ps_dec.c \
                                        libfaad2/ps_syntax.c  \
                                        libfaad2/pulse.c \
                                        libfaad2/rvlc.c \
                                        libfaad2/sbr_dct.c \
                                        libfaad2/sbr_dec.c \
                                        libfaad2/sbr_e_nf.c \
                                        libfaad2/sbr_fbt.c \
                                        libfaad2/sbr_hfadj.c \
                                        libfaad2/sbr_hfgen.c \
                                        libfaad2/sbr_huff.c \
                                        libfaad2/sbr_qmf.c \
                                        libfaad2/sbr_syntax.c \
                                        libfaad2/sbr_tf_grid.c \
                                        libfaad2/specrec.c \
                                        libfaad2/ssr.c \
                                        libfaad2/ssr_fb.c \
                                        libfaad2/ssr_ipqf.c \
                                        libfaad2/syntax.c \
                                        libfaad2/tns.c \

SRCS_COMMON-$(LIBMAD)                += libmpcodecs/ad_libmad.c
SRCS_COMMON-$(LIBMPEG2)              += libmpcodecs/vd_libmpeg2.c \
                                        libmpeg2/alloc.c \
                                        libmpeg2/cpu_accel.c\
                                        libmpeg2/cpu_state.c \
                                        libmpeg2/decode.c \
                                        libmpeg2/header.c \
                                        libmpeg2/idct.c \
                                        libmpeg2/motion_comp.c \
                                        libmpeg2/slice.c
SRCS_COMMON-$(LIBMPEG2)-$(ARCH_ALPHA)   += libmpeg2/idct_alpha.c \
                                           libmpeg2/motion_comp_alpha.c
SRCS_COMMON-$(LIBMPEG2)-$(ARCH_ARMV4L)  += libmpeg2/motion_comp_arm.c \
                                           libmpeg2/motion_comp_arm_s.S \
                                           libmpeg2/motion_comp_iwmmxt.c
SRCS_COMMON-$(LIBMPEG2)-$(HAVE_ALTIVEC) += libmpeg2/idct_altivec.c \
                                           libmpeg2/motion_comp_altivec.c
SRCS_COMMON-$(LIBMPEG2)-$(HAVE_MMX)     += libmpeg2/idct_mmx.c \
                                           libmpeg2/motion_comp_mmx.c
SRCS_COMMON-$(LIBMPEG2)-$(HAVE_VIS)     += libmpeg2/motion_comp_vis.c
SRCS_COMMON-$(LIBNEMESI)             += libmpdemux/demux_nemesi.c \
                                        stream/stream_nemesi.c
SRCS_COMMON-$(LIBNUT)                += libmpdemux/demux_nut.c
SRCS_COMMON-$(LIBPOSTPROC)           += libmpcodecs/vf_pp.c
SRCS_COMMON-$(LIBSMBCLIENT)          += stream/stream_smb.c
SRCS_COMMON-$(LIBTHEORA)             += libmpcodecs/vd_theora.c
SRCS_COMMON-$(LIBVORBIS)             += libmpcodecs/ad_libvorbis.c \
                                        libmpdemux/demux_ogg.c
SRCS_COMMON-$(MACOSX_FINDER_SUPPORT) += osdep/macosx_finder_args.c
SRCS_COMMON-$(MP3LIB)                += libmpcodecs/ad_mp3lib.c mp3lib/sr1.c
SRCS_COMMON-$(MP3LIB)-$(ARCH_X86_32) += mp3lib/decode_i586.c
SRCS_COMMON-$(MP3LIB)-$(ARCH_X86_32)-$(HAVE_3DNOW)    += mp3lib/dct36_3dnow.c \
                                                         mp3lib/dct64_3dnow.c
SRCS_COMMON-$(MP3LIB)-$(ARCH_X86_32)-$(HAVE_3DNOWEX)  += mp3lib/dct36_k7.c \
                                                         mp3lib/dct64_k7.c
SRCS_COMMON-$(MP3LIB)-$(ARCH_X86_32)-$(HAVE_MMX)      += mp3lib/dct64_mmx.c
SRCS_COMMON-$(MP3LIB)-$(HAVE_ALTIVEC) += mp3lib/dct64_altivec.c
SRCS_COMMON-$(MP3LIB)-$(HAVE_MMX)    += mp3lib/decode_mmx.c
SRCS_COMMON-$(MP3LIB)-$(HAVE_SSE)    += mp3lib/dct64_sse.c
SRCS_COMMON-$(MPLAYER_NETWORK)       += stream/stream_netstream.c \
                                        stream/asf_mmst_streaming.c \
                                        stream/asf_streaming.c \
                                        stream/cookies.c \
                                        stream/http.c \
                                        stream/network.c \
                                        stream/pnm.c \
                                        stream/rtp.c \
                                        stream/udp.c \
                                        stream/tcp.c \
                                        stream/stream_rtp.c \
                                        stream/stream_udp.c \
                                        stream/librtsp/rtsp.c \
                                        stream/realrtsp/asmrp.c \
                                        stream/realrtsp/real.c \
                                        stream/realrtsp/rmff.c \
                                        stream/realrtsp/sdpplin.c \
                                        stream/realrtsp/xbuffer.c \

SRCS_COMMON-$(MUSEPACK)              += libmpcodecs/ad_mpc.c \
                                        libmpdemux/demux_mpc.c
SRCS_COMMON-$(NATIVE_RTSP)           += stream/stream_rtsp.c \
                                        stream/freesdp/common.c \
                                        stream/freesdp/errorlist.c \
                                        stream/freesdp/parser.c \
                                        stream/librtsp/rtsp.c \
                                        stream/librtsp/rtsp_rtp.c \
                                        stream/librtsp/rtsp_session.c \

SRCS_COMMON-$(NEED_GETTIMEOFDAY)     += osdep/gettimeofday.c
SRCS_COMMON-$(NEED_GLOB)             += osdep/glob-win.c
SRCS_COMMON-$(NEED_MMAP)             += osdep/mmap-os2.c
SRCS_COMMON-$(NEED_SETENV)           += osdep/setenv.c
SRCS_COMMON-$(NEED_SHMEM)            += osdep/shmem.c
SRCS_COMMON-$(NEED_STRSEP)           += osdep/strsep.c
SRCS_COMMON-$(NEED_SWAB)             += osdep/swab.c
SRCS_COMMON-$(NEED_VSSCANF)          += osdep/vsscanf.c
SRCS_COMMON-$(PNG)                   += libmpcodecs/vd_mpng.c
SRCS_COMMON-$(PVR)                   += stream/stream_pvr.c
SRCS_COMMON-$(QTX_CODECS)            += libmpcodecs/ad_qtaudio.c \
                                        libmpcodecs/vd_qtvideo.c
SRCS_COMMON-$(QTX_EMULATION)         += loader/wrapper.S
SRCS_COMMON-$(RADIO)                 += stream/stream_radio.c
SRCS_COMMON-$(RADIO_CAPTURE)         += stream/audio_in.c
SRCS_COMMON-$(REAL_CODECS)           += libmpcodecs/ad_realaud.c \
                                        libmpcodecs/vd_realvid.c
SRCS_COMMON-$(SPEEX)                 += libmpcodecs/ad_speex.c
SRCS_COMMON-$(STREAM_CACHE)          += stream/cache2.c
SRCS_COMMON-$(STREAMING_LIVE555)     += libmpdemux/demux_rtp.cpp \
                                        libmpdemux/demux_rtp_codec.cpp \
                                        stream/stream_livedotcom.c \

SRCS_COMMON-$(TREMOR_INTERNAL)       += tremor/bitwise.c \
                                        tremor/block.c \
                                        tremor/codebook.c \
                                        tremor/floor0.c \
                                        tremor/floor1.c \
                                        tremor/framing.c \
                                        tremor/info.c \
                                        tremor/mapping0.c \
                                        tremor/mdct.c \
                                        tremor/registry.c \
                                        tremor/res012.c \
                                        tremor/sharedbook.c \
                                        tremor/synthesis.c \
                                        tremor/window.c \

SRCS_COMMON-$(TV)                    += stream/stream_tv.c stream/tv.c \
                                        stream/frequencies.c stream/tvi_dummy.c
SRCS_COMMON-$(TV_BSDBT848)           += stream/tvi_bsdbt848.c
SRCS_COMMON-$(TV_DSHOW)              += stream/tvi_dshow.c
SRCS_COMMON-$(TV_TELETEXT)           += stream/tvi_vbi.c
SRCS_COMMON-$(TV_V4L1)               += stream/tvi_v4l.c  stream/audio_in.c
SRCS_COMMON-$(TV_V4L2)               += stream/tvi_v4l2.c stream/audio_in.c
SRCS_COMMON-$(UNRAR_EXEC)            += unrar_exec.c
SRCS_COMMON-$(VCD)                   += stream/stream_vcd.c
SRCS_COMMON-$(VSTREAM)               += stream/stream_vstream.c
SRCS_COMMON-$(WIN32_EMULATION)       += loader/elfdll.c \
                                        loader/ext.c \
                                        loader/ldt_keeper.c \
                                        loader/module.c \
                                        loader/pe_image.c \
                                        loader/pe_resource.c \
                                        loader/registry.c \
                                        loader/resource.c \
                                        loader/win32.c \

SRCS_COMMON-$(WIN32DLL)              += libmpcodecs/ad_acm.c \
                                        libmpcodecs/ad_dmo.c \
                                        libmpcodecs/ad_dshow.c \
                                        libmpcodecs/ad_twin.c \
                                        libmpcodecs/vd_dmo.c \
                                        libmpcodecs/vd_dshow.c \
                                        libmpcodecs/vd_vfw.c \
                                        libmpcodecs/vd_vfwex.c \
                                        libmpdemux/demux_avs.c \
                                        loader/afl.c \
                                        loader/driver.c \
                                        loader/vfl.c \
                                        loader/dshow/DS_AudioDecoder.c \
                                        loader/dshow/DS_Filter.c \
                                        loader/dshow/DS_VideoDecoder.c \
                                        loader/dshow/allocator.c \
                                        loader/dshow/cmediasample.c \
                                        loader/dshow/guids.c \
                                        loader/dshow/inputpin.c \
                                        loader/dshow/mediatype.c \
                                        loader/dshow/outputpin.c \
                                        loader/dmo/DMO_AudioDecoder.c \
                                        loader/dmo/DMO_VideoDecoder.c   \
                                        loader/dmo/buffer.c   \
                                        loader/dmo/dmo.c  \
                                        loader/dmo/dmo_guids.c \

SRCS_COMMON-$(XANIM_CODECS)          += libmpcodecs/vd_xanim.c
SRCS_COMMON-$(XMMS_PLUGINS)          += libmpdemux/demux_xmms.c
SRCS_COMMON-$(XVID4)                 += libmpcodecs/vd_xvid4.c
SRCS_COMMON-$(ZORAN)                 += libmpcodecs/vd_zrmjpeg.c \
                                        libmpcodecs/vf_zrmjpeg.c

SRCS_MPLAYER = mplayer.c \
               m_property.c \
               mp_fifo.c \
               mp_msg.c \
               mixer.c \
               parser-mpcmd.c \
               command.c \
               input/input.c \
               libao2/audio_out.c \
               libao2/ao_mpegpes.c \
               libao2/ao_null.c \
               libao2/ao_pcm.c \
               $(addprefix libao2/,$(AO_SRCS)) \
               libvo/aspect.c \
               libvo/geometry.c \
               libvo/spuenc.c \
               libvo/video_out.c \
               libvo/vo_mpegpes.c \
               libvo/vo_null.c \
               libvo/vo_yuv4mpeg.c \
               $(addprefix libvo/,$(VO_SRCS)) \

SRCS_MPLAYER-$(APPLE_IR)     += input/appleir.c
SRCS_MPLAYER-$(APPLE_REMOTE) += input/ar.c
SRCS_MPLAYER-$(GUI)          += gui/bitmap.c
SRCS_MPLAYER-$(GUI_GTK)      += gui/app.c \
                                gui/cfg.c \
                                gui/interface.c \
                                gui/mplayer/gui_common.c \
                                gui/mplayer/menu.c \
                                gui/mplayer/mw.c \
                                gui/mplayer/pb.c \
                                gui/mplayer/play.c \
                                gui/mplayer/sw.c \
                                gui/mplayer/widgets.c \
                                gui/mplayer/gtk/about.c \
                                gui/mplayer/gtk/eq.c \
                                gui/mplayer/gtk/fs.c \
                                gui/mplayer/gtk/gtk_common.c \
                                gui/mplayer/gtk/gtk_url.c \
                                gui/mplayer/gtk/mb.c \
                                gui/mplayer/gtk/menu.c \
                                gui/mplayer/gtk/opts.c \
                                gui/mplayer/gtk/pl.c \
                                gui/mplayer/gtk/sb.c \
                                gui/skin/cut.c \
                                gui/skin/font.c \
                                gui/skin/skin.c \
                                gui/wm/ws.c \
                                gui/wm/wsxdnd.c \

SRCS_MPLAYER-$(GUI_WIN32)    += gui/win32/dialogs.c \
                                gui/win32/gui.c \
                                gui/win32/interface.c \
                                gui/win32/playlist.c \
                                gui/win32/preferences.c \
                                gui/win32/skinload.c \
                                gui/win32/widgetrender.c \
                                gui/win32/wincfg.c \

SRCS_MPLAYER-$(JOYSTICK)     += input/joystick.c
SRCS_MPLAYER-$(LIBMENU)      += libmenu/menu.c \
                                libmenu/menu_chapsel.c \
                                libmenu/menu_cmdlist.c  \
                                libmenu/menu_console.c \
                                libmenu/menu_filesel.c \
                                libmenu/menu_list.c  \
                                libmenu/menu_param.c \
                                libmenu/menu_pt.c \
                                libmenu/menu_txt.c \
                                libmenu/vf_menu.c \

SRCS_MPLAYER-$(LIBMENU_DVBIN) += libmenu/menu_dvbin.c
SRCS_MPLAYER-$(LIRC)          += input/lirc.c

SRCS_MPLAYER-$(VIDIX)         += libvo/vosub_vidix.c \
                                 vidix/vidixlib.c \
                                 vidix/drivers.c \
                                 vidix/dha.c \
                                 vidix/mtrr.c \
                                 vidix/pci.c \
                                 vidix/pci_names.c \
                                 vidix/pci_dev_ids.c\

SRCS_MPLAYER-$(VIDIX_CYBERBLADE)    += vidix/cyberblade_vid.c
SRCS_MPLAYER-$(VIDIX_IVTV)          += vidix/ivtv_vid.c
SRCS_MPLAYER-$(VIDIX_MACH64)        += vidix/mach64_vid.c
SRCS_MPLAYER-$(VIDIX_MGA)           += vidix/mga_vid.c
SRCS_MPLAYER-$(VIDIX_MGA_CRTC2)     += vidix/mga_crtc2_vid.c
SRCS_MPLAYER-$(VIDIX_NVIDIA)        += vidix/nvidia_vid.c
SRCS_MPLAYER-$(VIDIX_PM2)           += vidix/pm2_vid.c
SRCS_MPLAYER-$(VIDIX_PM3)           += vidix/pm3_vid.c
SRCS_MPLAYER-$(VIDIX_RADEON)        += vidix/radeon_vid.c
SRCS_MPLAYER-$(VIDIX_RAGE128)       += vidix/rage128_vid.c
SRCS_MPLAYER-$(VIDIX_S3)            += vidix/s3_vid.c
SRCS_MPLAYER-$(VIDIX_SIS)           += vidix/sis_vid.c vidix/sis_bridge.c
SRCS_MPLAYER-$(VIDIX_UNICHROME)     += vidix/unichrome_vid.c

SRCS_MENCODER = mencoder.c \
                mp_msg-mencoder.c \
                parser-mecmd.c \
                xvid_vbr.c \
                libmpcodecs/ae.c \
                libmpcodecs/ae_pcm.c \
                libmpcodecs/ve.c \
                libmpcodecs/ve_raw.c \
                libmpdemux/muxer.c \
                libmpdemux/muxer_avi.c \
                libmpdemux/muxer_mpeg.c \
                libmpdemux/muxer_rawaudio.c \
                libmpdemux/muxer_rawvideo.c \

SRCS_MENCODER-$(FAAC)             += libmpcodecs/ae_faac.c
SRCS_MENCODER-$(LIBAVCODEC)       += libmpcodecs/ae_lavc.c libmpcodecs/ve_lavc.c
SRCS_MENCODER-$(LIBAVFORMAT)      += libmpdemux/muxer_lavf.c
SRCS_MENCODER-$(LIBDV)            += libmpcodecs/ve_libdv.c
SRCS_MENCODER-$(LIBLZO)           += libmpcodecs/ve_nuv.c
SRCS_MENCODER-$(MP3LAME)          += libmpcodecs/ae_lame.c
SRCS_MENCODER-$(QTX_CODECS_WIN32) += libmpcodecs/ve_qtvideo.c
SRCS_MENCODER-$(TOOLAME)          += libmpcodecs/ae_toolame.c
SRCS_MENCODER-$(TWOLAME)          += libmpcodecs/ae_twolame.c
SRCS_MENCODER-$(WIN32DLL)         += libmpcodecs/ve_vfw.c
SRCS_MENCODER-$(X264)             += libmpcodecs/ve_x264.c
SRCS_MENCODER-$(XVID4)            += libmpcodecs/ve_xvid4.c

SRCS_COMMON   += $(SRCS_COMMON-yes) $(SRCS_COMMON-yes-yes) $(SRCS_COMMON-yes-yes-yes)
SRCS_MENCODER += $(SRCS_MENCODER-yes)
SRCS_MPLAYER  += $(SRCS_MPLAYER-yes)

COMMON_LIBS-$(LIBAVFORMAT_A)      += libavformat/libavformat.a
COMMON_LIBS-$(LIBAVCODEC_A)       += libavcodec/libavcodec.a
COMMON_LIBS-$(LIBAVUTIL_A)        += libavutil/libavutil.a
COMMON_LIBS-$(LIBPOSTPROC_A)      += libpostproc/libpostproc.a
COMMON_LIBS  = libswscale/libswscale.a
COMMON_LIBS += $(COMMON_LIBS-yes)

OBJS_COMMON    += $(addsuffix .o, $(basename $(SRCS_COMMON)))
OBJS_MENCODER  += $(addsuffix .o, $(basename $(SRCS_MENCODER)))
OBJS_MPLAYER   += $(addsuffix .o, $(basename $(SRCS_MPLAYER)))
OBJS_MPLAYER-$(PE_EXECUTABLE) += osdep/mplayer-rc.o
OBJS_MPLAYER   += $(OBJS_MPLAYER-yes)

MPLAYER_DEPS  = $(OBJS_MPLAYER)  $(OBJS_COMMON) $(COMMON_LIBS)
MENCODER_DEPS = $(OBJS_MENCODER) $(OBJS_COMMON) $(COMMON_LIBS)

ALL_PRG-$(MPLAYER)  += mplayer$(EXESUF)
ALL_PRG-$(MENCODER) += mencoder$(EXESUF)

INSTALL_TARGETS-$(MPLAYER)  += install-mplayer  install-mplayer-man
INSTALL_TARGETS-$(MENCODER) += install-mencoder install-mencoder-man
INSTALL_TARGETS-$(GUI)      += install-gui

DIRS =  . \
        dvdread \
        gui \
        gui/mplayer \
        gui/mplayer/gtk \
        gui/skin \
        gui/wm \
        gui/win32 \
        input \
        liba52 \
        libaf \
        libao2 \
        libass \
        libavcodec \
        libavcodec/alpha \
        libavcodec/armv4l \
        libavcodec/bfin \
        libavcodec/i386 \
        libavcodec/mlib \
        libavcodec/ppc \
        libavcodec/sh4 \
        libavcodec/sparc \
        libavformat \
        libavutil \
        libdvdcss \
        libfaad2 \
        libmenu \
        libmpcodecs \
        libmpcodecs/native \
        libmpdemux \
        libmpeg2 \
        libpostproc \
        libswscale \
        libvo \
        loader \
        loader/dshow \
        loader/dmo \
        mp3lib \
        osdep \
        stream \
        stream/freesdp \
        stream/librtsp \
        stream/realrtsp \
        tremor \
        TOOLS \
        vidix \

PARTS = libavcodec \
        libavformat \
        libavutil \
        libpostproc \
        libswscale \



###### generic rules #######

all: $(ALL_PRG-yes)

%.d: %.c
	$(MPDEPEND_CMD) > $@

%.d: %.cpp
	$(MPDEPEND_CMD_CXX) > $@

%.d: %.m
	$(MPDEPEND_CMD) > $@

%.o: %.m
	$(CC) $(CFLAGS) -c -o $@ $<

%.ho: %.h
	$(CC) $(CFLAGS) -Wno-unused -c -o $@ -x c $<

ALLHEADERS = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.h))
checkheaders: $(ALLHEADERS:.h=.ho)

dep depend: $(DEPS)
	for part in $(PARTS); do $(MAKE) -C $$part depend; done

ALLPARTLIBS = $(foreach part, $(PARTS), $(part)/$(part).a)

$(ALLPARTLIBS): recurse
	$(MAKE) -C $(@D)

mplayer$(EXESUF): $(MPLAYER_DEPS)
	$(CC) -o $@ $^ $(LDFLAGS_MPLAYER)

mencoder$(EXESUF): $(MENCODER_DEPS)
	$(CC) -o $@ $^ $(LDFLAGS_MENCODER)

codec-cfg$(EXESUF): codec-cfg.c codec-cfg.h help_mp.h
	$(HOST_CC) -O -I. -DCODECS2HTML -o $@ $<

codecs.conf.h: codec-cfg$(EXESUF) etc/codecs.conf
	./codec-cfg$(EXESUF) ./etc/codecs.conf > $@

# ./configure must be rerun if it changed
config.mak: configure
	@echo "############################################################"
	@echo "####### Please run ./configure again - it's changed! #######"
	@echo "############################################################"

help_mp.h: help/help_mp-en.h $(HELP_FILE)
	help/help_create.sh $(HELP_FILE) $(CHARSET)

# rebuild version.h each time the working copy is updated
ifeq ($(wildcard .svn/entries),.svn/entries)
version.h: .svn/entries
endif
version.h:
	./version.sh `$(CC) -dumpversion`

osdep/mplayer-rc.o: osdep/mplayer.rc version.h
	$(WINDRES) -I. -o $@ $<



###### dependency declarations / specific CFLAGS ######

codec-cfg.d: codecs.conf.h
mencoder.d mplayer.d vobsub.d gui/win32/gui.d libmpdemux/muxer_avi.d stream/network.d stream/stream_cddb.d: version.h
DEPS = $(filter-out %.S,$(patsubst %.cpp,%.d,$(patsubst %.c,%.d,$(SRCS_COMMON) $(SRCS_MPLAYER:.m=.d) $(SRCS_MENCODER))))
$(DEPS): help_mp.h

dvdread/%.o dvdread/%.d: CFLAGS += -D__USE_UNIX98 -D_GNU_SOURCE $(LIBDVDCSS_DVDREAD_FLAGS)
libdvdcss/%.o libdvdcss/%.d: CFLAGS += -D__USE_UNIX98 -D_GNU_SOURCE -DVERSION=\"1.2.9\"
libfaad2/%.o libfaad2/%.d: CFLAGS += -Ilibfaad2 -D_GNU_SOURCE

loader/% loader/%: CFLAGS += -Iloader -fno-omit-frame-pointer $(CFLAG_NO_OMIT_LEAF_FRAME_POINTER)
#loader/%.o loader/%.d: CFLAGS += -Ddbg_printf=__vprintf -DTRACE=__vprintf -DDETAILED_OUT
loader/win32.o loader/win32.d: CFLAGS += $(CFLAG_STACKREALIGN)

mp3lib/decode_i586.o: CFLAGS += -fomit-frame-pointer

VIDIX_PCI_FILES = vidix/pci_dev_ids.c vidix/pci_ids.h vidix/pci_names.c \
                  vidix/pci_names.h vidix/pci_vendors.h

$(VIDIX_PCI_FILES): vidix/pci.db
	LC_ALL=C awk -f vidix/pci_db2c.awk $< $(VIDIX_PCIDB)

VIDIX_DEPS = $(filter vidix/%,$(SRCS_MPLAYER:.c=.d))
VIDIX_OBJS = $(filter vidix/%,$(SRCS_MPLAYER:.c=.o))

$(VIDIX_DEPS) $(VIDIX_OBJS): $(VIDIX_PCI_FILES)



###### installation / clean / generic rules #######

install: $(INSTALL_TARGETS-yes)

install-dirs:
	$(INSTALL) -d $(BINDIR) $(CONFDIR)

install-%: %$(EXESUF) install-dirs
	$(INSTALL) -m 755 $(INSTALLSTRIP) $< $(BINDIR)

install-mplayer-man: $(foreach lang,$(MAN_LANG_ALL),install-mplayer-man-$(lang))
install-mencoder-man: $(foreach lang,$(MAN_LANG_ALL),install-mencoder-man-$(lang))

install-mplayer-man-en:
	$(INSTALL) -d $(MANDIR)/man1
	$(INSTALL) -c -m 644 DOCS/man/en/mplayer.1 $(MANDIR)/man1/

install-mencoder-man-en: install-mplayer-man-en
	cd $(MANDIR)/man1 && ln -sf mplayer.1 mencoder.1

define MPLAYER_MAN_RULE
install-mplayer-man-$(lang):
	$(INSTALL) -d $(MANDIR)/$(lang)/man1
	$(INSTALL) -c -m 644 DOCS/man/$(lang)/mplayer.1 $(MANDIR)/$(lang)/man1/
endef

define MENCODER_MAN_RULE
install-mencoder-man-$(lang): install-mplayer-man-$(lang)
	cd $(MANDIR)/$(lang)/man1 && ln -sf mplayer.1 mencoder.1
endef

$(foreach lang,$(MAN_LANG),$(eval $(MPLAYER_MAN_RULE)))
$(foreach lang,$(MAN_LANG),$(eval $(MENCODER_MAN_RULE)))

install-gui: install-mplayer
	-ln -sf mplayer$(EXESUF) $(BINDIR)/gmplayer$(EXESUF)
	$(INSTALL) -d $(DATADIR)/skins $(prefix)/share/pixmaps $(prefix)/share/applications
	$(INSTALL) -m 644 etc/mplayer.xpm $(prefix)/share/pixmaps/
	$(INSTALL) -m 644 etc/mplayer.desktop $(prefix)/share/applications/

uninstall:
	rm -f $(BINDIR)/mplayer$(EXESUF) $(BINDIR)/gmplayer$(EXESUF)
	rm -f $(BINDIR)/mencoder$(EXESUF)
	rm -f $(MANDIR)/man1/mencoder.1 $(MANDIR)/man1/mplayer.1
	rm -f $(prefix)/share/pixmaps/mplayer.xpm
	rm -f $(prefix)/share/applications/mplayer.desktop
	rm -f $(MANDIR)/man1/mplayer.1 $(MANDIR)/man1/mencoder.1
	rm -f $(foreach lang,$(MAN_LANG),$(foreach man,mplayer.1 mencoder.1,$(MANDIR)/$(lang)/man1/$(man)))

clean:
	rm -f $(foreach dir,$(DIRS),$(foreach suffix,/*.o /*.a /*.ho /*~, $(addsuffix $(suffix),$(dir))))
	rm -f mplayer$(EXESUF) mencoder$(EXESUF)

distclean: clean testsclean toolsclean
	rm -rf DOCS/tech/doxygen
	rm -f $(foreach dir,$(DIRS),$(foreach suffix,/*.d, $(addsuffix $(suffix),$(dir))))
	rm -f configure.log config.mak config.h	codecs.conf.h help_mp.h \
           version.h $(VIDIX_PCI_FILES) \
           codec-cfg$(EXESUF) cpuinfo$(EXESUF) TAGS tags

doxygen:
	doxygen DOCS/tech/Doxyfile

TAGS:
	rm -f $@; ( find -name '*.[chS]' -print ) | xargs etags -a

tags:
	rm -f $@; ( find -name '*.[chS]' -print ) | xargs ctags -a



###### tests / tools #######

TEST_OBJS = mp_msg-mencoder.o mp_fifo.o osdep/$(GETCH) osdep/$(TIMER) -ltermcap -lm

codecs2html$(EXESUF): codec-cfg.c $(TEST_OBJS)
	$(CC) -I. -DCODECS2HTML -o $@ $^

codec-cfg-test$(EXESUF): codec-cfg.c codecs.conf.h codec-cfg.h $(TEST_OBJS)
	$(CC) -I. -DTESTING -o $@ $^

liba52/test$(EXESUF): liba52/test.c cpudetect.o $(filter liba52/%,$(SRCS_COMMON:.c=.o))

libvo/aspecttest$(EXESUF): libvo/aspecttest.c libvo/aspect.o libvo/geometry.o $(TEST_OBJS)

LOADER_TEST_OBJS = $(filter loader/%,$(SRCS_COMMON:.c=.o)) libmpdemux/aviprint.o osdep/mmap_anon.o cpudetect.o $(TEST_OBJS)

loader/qtx/list$(EXESUF) loader/qtx/qtxload$(EXESUF): CFLAGS += -g
loader/qtx/list$(EXESUF): loader/qtx/list.c $(LOADER_TEST_OBJS)
loader/qtx/qtxload$(EXESUF): loader/qtx/qtxload.c $(LOADER_TEST_OBJS)

mp3lib/test$(EXESUF):  mp3lib/test.c  $(filter mp3lib/%,$(SRCS_COMMON:.c=.o)) libvo/aclib.o cpudetect.o $(TEST_OBJS)
mp3lib/test2$(EXESUF): mp3lib/test2.c $(filter mp3lib/%,$(SRCS_COMMON:.c=.o)) libvo/aclib.o cpudetect.o $(TEST_OBJS)

TESTS = codecs2html$(EXESUF) codec-cfg-test$(EXESUF) \
        liba52/test$(EXESUF) libvo/aspecttest$(EXESUF) \
        loader/qtx/list$(EXESUF) loader/qtx/qtxload$(EXESUF) \
        mp3lib/test$(EXESUF) mp3lib/test2$(EXESUF)

tests: $(TESTS)

testsclean:
	rm -f $(TESTS)

TOOLS = TOOLS/alaw-gen$(EXESUF) \
        TOOLS/asfinfo$(EXESUF) \
        TOOLS/avi-fix$(EXESUF) \
        TOOLS/avisubdump$(EXESUF) \
        TOOLS/compare$(EXESUF) \
        TOOLS/dump_mp4$(EXESUF) \
        TOOLS/movinfo$(EXESUF) \
        TOOLS/subrip$(EXESUF) \

ifdef ARCH_X86
TOOLS += TOOLS/modify_reg$(EXESUF)
endif

ALLTOOLS = $(TOOLS) \
           TOOLS/bmovl-test$(EXESUF) \
           TOOLS/vfw2menc$(EXESUF) \
           TOOLS/vivodump$(EXESUF) \
           TOOLS/netstream$(EXESUF) \

tools: $(TOOLS)
alltools: $(ALLTOOLS)

toolsclean:
	rm -f $(ALLTOOLS) TOOLS/fastmem*-* TOOLS/realcodecs/*.so.6.0

TOOLS/bmovl-test$(EXESUF): TOOLS/bmovl-test.c -lSDL_image

TOOLS/subrip$(EXESUF): TOOLS/subrip.c vobsub.o spudec.o unrar_exec.o \
  libvo/aclib.o libswscale/libswscale.a libavutil/libavutil.a \
  $(TEST_OBJS)

TOOLS/vfw2menc$(EXESUF): TOOLS/vfw2menc.c -lwinmm -lole32

#FIXME: Linking is broken, help welcome.
TOOLS/vivodump$(EXESUF): TOOLS/vivodump.c $(TEST_OBJS)

fastmemcpybench: TOOLS/fastmemcpybench.c
	$(CC) $(CFLAGS) $< -o TOOLS/fastmem-mmx$(EXESUF)  -DNAME=\"mmx\"      -DHAVE_MMX
	$(CC) $(CFLAGS) $< -o TOOLS/fastmem-k6$(EXESUF)   -DNAME=\"k6\ \"     -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) $< -o TOOLS/fastmem-k7$(EXESUF)   -DNAME=\"k7\ \"     -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) $< -o TOOLS/fastmem-sse$(EXESUF)  -DNAME=\"sse\"      -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2
	$(CC) $(CFLAGS) $< -o TOOLS/fastmem2-mmx$(EXESUF) -DNAME=\"mga-mmx\"  -DHAVE_MGA -DHAVE_MMX
	$(CC) $(CFLAGS) $< -o TOOLS/fastmem2-k6$(EXESUF)  -DNAME=\"mga-k6\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW
	$(CC) $(CFLAGS) $< -o TOOLS/fastmem2-k7$(EXESUF)  -DNAME=\"mga-k7\ \" -DHAVE_MGA -DHAVE_MMX -DHAVE_3DNOW -DHAVE_MMX2
	$(CC) $(CFLAGS) $< -o TOOLS/fastmem2-sse$(EXESUF) -DNAME=\"mga-sse\"  -DHAVE_MGA -DHAVE_MMX -DHAVE_SSE   -DHAVE_MMX2

REAL_SRCS    = $(wildcard TOOLS/realcodecs/*.c)
REAL_TARGETS = $(REAL_SRCS:.c=.so.6.0)

realcodecs: $(REAL_TARGETS)

fastmemcpybench realcodecs: CFLAGS += -g

%.so.6.0: %.o
	ld -shared -o $@ $< -ldl -lc

# FIXME: netstream linking is a mess that should be fixed properly some day.
# It does not work with either GUI, LIVE555, libavformat, cdparanoia enabled.
NETSTREAM_DEPS = libavutil/libavutil.a \
                 m_option.o \
                 m_struct.o \
                 $(TEST_OBJS)

TOOLS/netstream$(EXESUF): TOOLS/netstream.o $(NETSTREAM_DEPS)
	$(CC) $(CFLAGS) -o $@ $^

-include $(DEPS)

.PHONY: all doxygen *install* recurse *tools
.PHONY: checkheaders *clean dep depend tests
