
include ../config.mak

LIBNAME = libmpcodecs.a
LIBNAME2 = libmpencoders.a

AUDIO_SRCS_LIB=ad_hwac3.c \
               ad_liba52.c \
               ad_mp3lib.c \

AUDIO_SRCS_NAT=ad_alaw.c \
               ad_dk3adpcm.c \
               ad_dvdpcm.c \
               ad_imaadpcm.c \
               ad_msadpcm.c \
               ad_msgsm.c \
               ad_pcm.c \
               ad_ra1428.c \

AUDIO_SRCS_OPT=ad_acm.c \
               ad_dmo.c \
               ad_dshow.c \
               ad_faad.c \
               ad_ffmpeg.c \
               ad_libdv.c \
               ad_libmad.c \
               ad_libvorbis.c \
               ad_qtaudio.c \
               ad_realaud.c \
               ad_twin.c \

AUDIO_SRCS=dec_audio.c ad.c $(AUDIO_SRCS_LIB) $(AUDIO_SRCS_NAT) $(AUDIO_SRCS_OPT)

VIDEO_SRCS_LIB=vd_libmpeg2.c \
               vd_lzo.c \
               vd_nuv.c \

VIDEO_SRCS_NAT=vd_hmblck.c \
               vd_mpegpes.c \
               vd_mtga.c \
               vd_null.c \
               vd_raw.c \
               vd_sgi.c \

VIDEO_SRCS_OPT=vd_divx4.c \
               vd_dmo.c \
               vd_dshow.c \
               vd_ffmpeg.c\
               vd_libdv.c \
               vd_odivx.c \
               vd_qtvideo.c \
               vd_realvid.c \
               vd_theora.c \
               vd_vfw.c \
               vd_vfwex.c \
               vd_xanim.c \
               vd_xvid4.c \
               vd_xvid.c \
               vd_zrmjpeg.c \

VIDEO_SRCS=dec_video.c vd.c $(VIDEO_SRCS_NAT) $(VIDEO_SRCS_LIB) $(VIDEO_SRCS_OPT)

VFILTER_SRCS=vf.c \
             pullup.c \
             vf_1bpp.c \
             vf_2xsai.c \
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
             vf_fspp.c \
             vf_halfpack.c \
             vf_harddup.c \
             vf_hqdn3d.c \
             vf_hue.c \
             vf_il.c \
             vf_ilpack.c \
             vf_ivtc.c \
             vf_kerndeint.c \
             vf_lavc.c \
             vf_lavcdeint.c \
             vf_mirror.c \
             vf_noformat.c \
             vf_noise.c \
             vf_palette.c \
             vf_perspective.c \
             vf_phase.c \
             vf_pp7.c \
             vf_pullup.c \
             vf_qp.c \
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
             vf_spp.c \
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

ifeq ($(HAVE_FFPOSTPROCESS),yes)
VFILTER_SRCS += vf_pp.c
endif

ENCODER_SRCS=ae.c \
             ae_pcm.c \
             ve.c \
             ve_divx4.c \
             ve_lavc.c \
             ve_libdv.c \
             ve_nuv.c \
             ve_qtvideo.c \
             ve_raw.c \
             ve_vfw.c \
             ve_x264.c \
             ve_xvid4.c \
             ve_xvid.c \

NATIVE_SRCS=native/decode144.c \
            native/decode288.c \
            native/minilzo.c \
            native/nuppelvideo.c \
            native/RTjpegN.c \
            native/xa_gsm.c \

ifeq ($(FAME),yes)
VFILTER_SRCS += vf_fame.c
endif

ifeq ($(ZORAN),yes)
VFILTER_SRCS += vf_zrmjpeg.c
endif

ifeq ($(PNG),yes)
VIDEO_SRCS += vd_mpng.c
endif

ifeq ($(JPEG),yes)
VIDEO_SRCS += vd_ijpg.c
endif

ifeq ($(TOOLAME),yes)
ENCODER_SRCS += ae_toolame.c
EXTRA_INC += $(TOOLAME_EXTRAFLAGS)
endif

ifeq ($(TWOLAME),yes)
ENCODER_SRCS += ae_twolame.c
endif

ifeq ($(CONFIG_MP3LAME),yes)
ENCODER_SRCS += ae_lame.c
endif

LIBAV_INC =
ifeq ($(CONFIG_LIBAVUTIL),yes)
LIBAV_INC += -I../libavutil
endif
ifeq ($(CONFIG_LIBAVCODEC),yes)
LIBAV_INC += -I../libavcodec
ENCODER_SRCS += ae_lavc.c
endif

ifeq ($(MUSEPACK),yes)
AUDIO_SRCS += ad_mpc.c
endif

ifeq ($(FAAC),yes)
ENCODER_SRCS += ae_faac.c
endif

SRCS=$(AUDIO_SRCS) $(VIDEO_SRCS) $(VFILTER_SRCS) $(NATIVE_SRCS) img_format.c
OBJS=$(SRCS:.c=.o)

SRCS2=$(ENCODER_SRCS)
OBJS2=$(SRCS2:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -Inative -I.. -I../libmpdemux -I../loader $(LIBAV_INC) $(EXTRA_INC) $(X264_INC) -D_GNU_SOURCE

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:    $(LIBNAME) $(LIBNAME2)

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)
	$(RANLIB) $(LIBNAME)

$(LIBNAME2):     $(OBJS2)
	$(AR) r $(LIBNAME2) $(OBJS2)
	$(RANLIB) $(LIBNAME2)

clean:
	rm -f *.o *.a *~
	rm -f native/*.o native/*.a native/*~

distclean:
	rm -f Makefile.bak $(OBJS) $(OBJS2) $(LIBNAME) $(LIBNAME2) *~ .depend

dep:    depend

depend:
	$(CC) -MM $(CFLAGS) $(SRCS) $(SRCS2) 1>.depend

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

