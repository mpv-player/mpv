
include ../config.mak

LIBNAME = libmpcodecs.a
LIBNAME2 = libmpencoders.a

AUDIO_SRCS_LIB=ad_liba52.c ad_hwac3.c ad_mp3lib.c
AUDIO_SRCS_NAT=ad_alaw.c ad_dk3adpcm.c ad_pcm.c ad_dvdpcm.c ad_imaadpcm.c ad_msadpcm.c ad_msgsm.c ad_roqaudio.c
AUDIO_SRCS_OPT=ad_acm.c ad_dshow.c ad_qtaudio.c ad_ffmpeg.c ad_faad.c ad_libvorbis.c ad_libmad.c ad_realaud.c ad_libdv.c
AUDIO_SRCS=dec_audio.c ad.c $(AUDIO_SRCS_LIB) $(AUDIO_SRCS_NAT) $(AUDIO_SRCS_OPT)

VIDEO_SRCS_LIB=vd_libmpeg2.c vd_nuv.c vd_lzo.c
VIDEO_SRCS_NAT=vd_null.c vd_cinepak.c vd_qtrpza.c vd_raw.c vd_msvidc.c vd_fli.c vd_qtrle.c vd_qtsmc.c vd_roqvideo.c vd_cyuv.c vd_msrle.c vd_huffyuv.c vd_mpegpes.c vd_svq1.c vd_lcl.c vd_mtga.c
VIDEO_SRCS_OPT=vd_realvid.c vd_ffmpeg.c vd_dshow.c vd_vfw.c vd_vfwex.c vd_odivx.c vd_divx4.c vd_xanim.c vd_xvid.c vd_libdv.c
VIDEO_SRCS=dec_video.c vd.c $(VIDEO_SRCS_NAT) $(VIDEO_SRCS_LIB) $(VIDEO_SRCS_OPT)

VFILTER_SRCS=vf.c vf_vo.c vf_crop.c vf_expand.c vf_pp.c vf_scale.c vf_format.c vf_yuy2.c vf_flip.c vf_rgb2bgr.c vf_rotate.c vf_mirror.c vf_palette.c vf_lavc.c vf_dvbscale.c vf_cropdetect.c vf_test.c vf_noise.c vf_yvu9.c vf_rectangle.c vf_lavcdeint.c vf_eq.c vf_eq2.c vf_halfpack.c vf_dint.c vf_1bpp.c vf_bmovl.c vf_2xsai.c vf_unsharp.c vf_swapuv.c vf_il.c vf_boxblur.c vf_sab.c
ENCODER_SRCS=ve.c ve_divx4.c ve_lavc.c ve_vfw.c ve_rawrgb.c ve_libdv.c ve_xvid.c

NATIVE_SRCS=native/RTjpegN.c native/cinepak.c native/cyuv.c native/fli.c native/minilzo.c native/msvidc.c native/nuppelvideo.c native/qtrle.c native/qtrpza.c native/qtsmc.c native/roqav.c native/xa_gsm.c native/svq1.c

ifeq ($(FAME),yes)
VFILTER_SRCS += vf_fame.c
endif

ifeq ($(PNG),yes)
VIDEO_SRCS += vd_mpng.c
endif

ifeq ($(JPEG),yes)
VIDEO_SRCS += vd_ijpg.c
endif

SRCS=$(AUDIO_SRCS) $(VIDEO_SRCS) $(VFILTER_SRCS) $(NATIVE_SRCS) img_format.c
OBJS=$(SRCS:.c=.o)

SRCS2=$(ENCODER_SRCS)
OBJS2=$(SRCS2:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -Inative -I.. -I../libmpdemux -I../loader $(EXTRA_INC)

.SUFFIXES: .c .o

# .PHONY: all clean

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all:    $(LIBNAME) $(LIBNAME2)

$(LIBNAME):     $(OBJS)
	$(AR) r $(LIBNAME) $(OBJS)

$(LIBNAME2):     $(OBJS2)
	$(AR) r $(LIBNAME2) $(OBJS2)

clean:
	rm -f *.o *.a *~

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

