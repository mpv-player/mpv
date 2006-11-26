
include ../config.mak

LIBNAME = libmpcodecs.a
LIBNAME2 = libmpencoders.a

LIBS =$(LIBNAME)
ifeq ($(MENCODER),yes)
LIBS+=$(LIBNAME2)
endif

SRCS=ad_hwac3.c \
               ad_hwmpa.c \

ifeq ($(LIBA52),yes)
SRCS+=ad_liba52.c
endif
ifeq ($(MP3LIB),yes)
SRCS+=ad_mp3lib.c
endif

SRCS=dec_audio.c \
           ad.c \
           ad_alaw.c \
               ad_dk3adpcm.c \
               ad_dvdpcm.c \
               ad_imaadpcm.c \
               ad_msadpcm.c \
               ad_msgsm.c \
               ad_pcm.c \

ifeq ($(CONFIG_LIBAVCODEC),yes)
SRCS+=ad_ffmpeg.c
endif
ifeq ($(CONFIG_LIBAVCODEC_SO),yes)
SRCS+=ad_ffmpeg.c
endif
ifeq ($(WIN32DLL),yes)
SRCS+=ad_acm.c ad_dmo.c ad_dshow.c ad_twin.c
endif
ifeq ($(QTX_CODECS),yes)
SRCS+=ad_qtaudio.c
endif
ifeq ($(REAL_CODECS),yes)
SRCS+=ad_realaud.c
endif
ifeq ($(FAAD),yes)
SRCS += ad_faad.c
endif
ifeq ($(LIBDV),yes)
SRCS+=ad_libdv.c
endif
ifeq ($(LIBMAD),yes)
SRCS+=ad_libmad.c
endif
ifeq ($(LIBVORBIS),yes)
SRCS+=ad_libvorbis.c
endif
ifeq ($(MUSEPACK),yes)
SRCS += ad_mpc.c
endif
ifeq ($(SPEEX),yes)
SRCS += ad_speex.c
endif


SRCS=vd_lzo.c \
               vd_nuv.c \

ifeq ($(LIBMPEG2),yes)
SRCS+=vd_libmpeg2.c
endif

SRCS=dec_video.c \
           vd.c \
           vd_hmblck.c \
               vd_mpegpes.c \
               vd_mtga.c \
               vd_null.c \
               vd_raw.c \
               vd_sgi.c \

ifeq ($(CONFIG_LIBAVCODEC),yes)
SRCS+=vd_ffmpeg.c
endif
ifeq ($(CONFIG_LIBAVCODEC_SO),yes)
SRCS+=vd_ffmpeg.c
endif
ifeq ($(WIN32DLL),yes)
SRCS+=vd_dmo.c vd_dshow.c vd_vfw.c vd_vfwex.c
endif
ifeq ($(QTX_CODECS),yes)
SRCS+=vd_qtvideo.c
endif
ifeq ($(REAL_CODECS),yes)
SRCS+=vd_realvid.c
endif
ifeq ($(XANIM_CODECS),yes)
SRCS+=vd_xanim.c
endif
ifeq ($(LIBDV),yes)
SRCS+=vd_libdv.c
endif
ifeq ($(LIBTHEORA),yes)
SRCS+=vd_theora.c
endif
ifeq ($(XVID4),yes)
SRCS+=vd_xvid4.c
endif
ifeq ($(ZORAN),yes)
SRCS+=vd_zrmjpeg.c
endif
ifeq ($(PNG),yes)
SRCS += vd_mpng.c
endif
ifeq ($(JPEG),yes)
SRCS += vd_ijpg.c
endif


SRCS=vf.c \
             pullup.c \
             vf_1bpp.c \
             vf_2xsai.c \
             vf_blackframe.c \
             vf_bmovl.c \
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
             vf_yuvcsp.c \
             vf_yuy2.c \
             vf_yvu9.c \
             vf_yadif.c \

LAVC_SRCS += vf_lavc.c \
                     vf_lavcdeint.c \

LAVC_DSPUTIL_SRCS += vf_uspp.c \
                             vf_fspp.c \
                             vf_qp.c \
                             vf_geq.c \
                             vf_spp.c \
                             vf_mcdeint.c \

ifeq ($(CONFIG_LIBPOSTPROC),yes)
SRCS += vf_pp.c
endif
ifeq ($(CONFIG_LIBPOSTPROC_SO),yes)
SRCS += vf_pp.c
endif
ifeq ($(CONFIG_LIBAVCODEC),yes)
SRCS += $(LAVC_SRCS)
SRCS += $(LAVC_DSPUTIL_SRCS)
endif
ifeq ($(CONFIG_LIBAVCODEC_SO),yes)
SRCS += $(LAVC_SRCS)
endif
ifeq ($(PNG),yes)
SRCS += vf_screenshot.c
endif
ifeq ($(ZORAN),yes)
SRCS += vf_zrmjpeg.c
endif
ifeq ($(CONFIG_ASS),yes)
SRCS += vf_ass.c
endif


ENCODER_SRCS=ae.c \
             ae_pcm.c \
             ve.c \
             ve_nuv.c \
             ve_raw.c \

ifeq ($(CONFIG_LIBAVCODEC),yes)
ENCODER_SRCS+=ve_lavc.c
endif
ifeq ($(CONFIG_LIBAVCODEC_SO),yes)
ENCODER_SRCS+=ve_lavc.c
endif
ifeq ($(WIN32DLL),yes)
ENCODER_SRCS+=ve_vfw.c
endif
ifeq ($(QTX_CODECS),yes)
ENCODER_SRCS+=ve_qtvideo.c
endif
ifeq ($(LIBDV),yes)
ENCODER_SRCS+=ve_libdv.c
endif
ifeq ($(XVID4),yes)
ENCODER_SRCS+=ve_xvid4.c
endif
ifeq ($(X264),yes)
ENCODER_SRCS+=ve_x264.c
endif
ifeq ($(TOOLAME),yes)
ENCODER_SRCS += ae_toolame.c
endif
ifeq ($(TWOLAME),yes)
ENCODER_SRCS += ae_twolame.c
endif
ifeq ($(MP3LAME),yes)
ENCODER_SRCS += ae_lame.c
endif
ifeq ($(FAAC),yes)
ENCODER_SRCS += ae_faac.c
endif
ifeq ($(CONFIG_LIBAVCODEC_SO),yes)
ENCODER_SRCS += ae_lavc.c
endif
ifeq ($(CONFIG_LIBAVCODEC),yes)
ENCODER_SRCS += ae_lavc.c
endif

ifeq ($(CONFIG_LIBAVUTIL),yes)
LIBAV_INC += -I../libavutil
endif
ifeq ($(CONFIG_LIBAVCODEC),yes)
LIBAV_INC += -I../libavcodec
endif


SRCS+=native/minilzo.c \
            native/nuppelvideo.c \
            native/RTjpegN.c \
            native/xa_gsm.c \
     img_format.c \

OBJS=$(SRCS:.c=.o)

SRCS2=$(ENCODER_SRCS)
OBJS2=$(SRCS2:.c=.o)

CFLAGS  = -I.. \
          -Inative \
          -I../libmpdemux \
          -I../stream \
          -I../loader \
          $(LIBAV_INC) \
          -D_GNU_SOURCE \

include ../mpcommon.mak

all:    $(LIBS)

$(LIBNAME2): $(OBJS2)
	$(AR) r $@ $^
	$(RANLIB) $@

clean::
	rm -f native/*.o native/*.a native/*~

