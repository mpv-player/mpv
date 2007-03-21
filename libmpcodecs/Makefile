include ../config.mak

LIBNAME_COMMON   = libmpcodecs.a
LIBNAME_MENCODER = libmpencoders.a

SRCS_COMMON = native/nuppelvideo.c \
              native/RTjpegN.c \
              native/xa_gsm.c \
              img_format.c \
              dec_audio.c \
              ad.c \
              ad_alaw.c \
              ad_dk3adpcm.c \
              ad_dvdpcm.c \
              ad_hwac3.c \
              ad_hwmpa.c \
              ad_imaadpcm.c \
              ad_msadpcm.c \
              ad_msgsm.c \
              ad_pcm.c \
              dec_video.c \
              vd.c \
              vd_hmblck.c \
              vd_lzo.c \
              vd_mpegpes.c \
              vd_mtga.c \
              vd_null.c \
              vd_nuv.c \
              vd_raw.c \
              vd_sgi.c \
              vf.c \
              pullup.c \
              vf_1bpp.c \
              vf_2xsai.c \
              vf_blackframe.c \
              vf_boxblur.c \
              vf_crop.c \
              vf_cropdetect.c \
              vf_decimate.c \
              vf_delogo.c \
              vf_denoise3d.c \
              vf_detc.c \
              vf_dint.c \
              vf_divtc.c \
              vf_down3dright.c \
              vf_dsize.c \
              vf_dvbscale.c \
              vf_eq2.c \
              vf_eq.c \
              vf_expand.c \
              vf_field.c \
              vf_fil.c \
              vf_filmdint.c \
              vf_flip.c \
              vf_format.c \
              vf_framestep.c \
              vf_halfpack.c \
              vf_harddup.c \
              vf_hqdn3d.c \
              vf_hue.c \
              vf_il.c \
              vf_ilpack.c \
              vf_ivtc.c \
              vf_kerndeint.c \
              vf_mirror.c \
              vf_noformat.c \
              vf_noise.c \
              vf_palette.c \
              vf_perspective.c \
              vf_phase.c \
              vf_pp7.c \
              vf_pullup.c \
              vf_rectangle.c \
              vf_remove_logo.c \
              vf_rgb2bgr.c \
              vf_rgbtest.c \
              vf_rotate.c \
              vf_sab.c \
              vf_scale.c \
              vf_smartblur.c \
              vf_softpulldown.c \
              vf_softskip.c \
              vf_swapuv.c \
              vf_telecine.c \
              vf_test.c \
              vf_tfields.c \
              vf_tile.c \
              vf_tinterlace.c \
              vf_unsharp.c \
              vf_vo.c \
              vf_yadif.c \
              vf_yuvcsp.c \
              vf_yuy2.c \
              vf_yvu9.c \

SRCS_COMMON-$(CONFIG_ASS)            += vf_ass.c
# These filters use private headers and do not work with shared libavcodec.
SRCS_COMMON-$(CONFIG_LIBAVCODEC)     += vf_fspp.c \
                                        vf_geq.c \
                                        vf_mcdeint.c \
                                        vf_qp.c \
                                        vf_spp.c \
                                        vf_uspp.c \

SRCS_COMMON-$(CONFIG_LIBAVCODEC)     += ad_ffmpeg.c vd_ffmpeg.c vf_lavc.c vf_lavcdeint.c
SRCS_COMMON-$(CONFIG_LIBAVCODEC_SO)  += ad_ffmpeg.c vd_ffmpeg.c vf_lavc.c vf_lavcdeint.c
SRCS_COMMON-$(CONFIG_LIBPOSTPROC)    += vf_pp.c
SRCS_COMMON-$(CONFIG_LIBPOSTPROC_SO) += vf_pp.c
SRCS_COMMON-$(FAAD)                  += ad_faad.c
SRCS_COMMON-$(HAVE_POSIX_SELECT)     += vf_bmovl.c
SRCS_COMMON-$(JPEG)                  += vd_ijpg.c
SRCS_COMMON-$(LIBA52)                += ad_liba52.c
SRCS_COMMON-$(LIBDV)                 += ad_libdv.c vd_libdv.c
SRCS_COMMON-$(LIBMAD)                += ad_libmad.c
SRCS_COMMON-$(LIBMPEG2)              += vd_libmpeg2.c
SRCS_COMMON-$(LIBTHEORA)             += vd_theora.c
SRCS_COMMON-$(LIBVORBIS)             += ad_libvorbis.c
SRCS_COMMON-$(MP3LIB)                += ad_mp3lib.c
SRCS_COMMON-$(MUSEPACK)              += ad_mpc.c
SRCS_COMMON-$(PNG)                   += vd_mpng.c vf_screenshot.c
SRCS_COMMON-$(QTX_CODECS)            += ad_qtaudio.c vd_qtvideo.c
SRCS_COMMON-$(REAL_CODECS)           += ad_realaud.c vd_realvid.c
SRCS_COMMON-$(SPEEX)                 += ad_speex.c
SRCS_COMMON-$(WIN32DLL)              += ad_acm.c ad_dmo.c ad_dshow.c ad_twin.c \
                                        vd_dmo.c vd_dshow.c vd_vfw.c vd_vfwex.c
SRCS_COMMON-$(XANIM_CODECS)          += vd_xanim.c
SRCS_COMMON-$(XVID4)                 += vd_xvid4.c
SRCS_COMMON-$(ZORAN)                 += vd_zrmjpeg.c vf_zrmjpeg.c


SRCS_MENCODER = ae.c \
                ae_pcm.c \
                ve.c \
                ve_raw.c \

SRCS_MENCODER-$(CONFIG_LIBAVCODEC)    += ae_lavc.c ve_lavc.c
SRCS_MENCODER-$(CONFIG_LIBAVCODEC_SO) += ae_lavc.c ve_lavc.c
SRCS_MENCODER-$(LIBDV)                += ve_libdv.c
SRCS_MENCODER-$(LIBLZO)               += ve_nuv.c
SRCS_MENCODER-$(QTX_CODECS)           += ve_qtvideo.c
SRCS_MENCODER-$(WIN32DLL)             += ve_vfw.c
SRCS_MENCODER-$(X264)                 += ve_x264.c
SRCS_MENCODER-$(XVID4)                += ve_xvid4.c
SRCS_MENCODER-$(FAAC)                 += ae_faac.c
SRCS_MENCODER-$(TOOLAME)              += ae_toolame.c
SRCS_MENCODER-$(TWOLAME)              += ae_twolame.c
SRCS_MENCODER-$(MP3LAME)              += ae_lame.c


include ../mpcommon.mak

clean::
	rm -f native/*.o native/*.a native/*~
