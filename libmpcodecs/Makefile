
include ../config.mak

LIBNAME = libmpcodecs.a
LIBNAME2 = libmpencoders.a

AUDIO_SRCS=dec_audio.c ad.c ad_a52.c ad_acm.c ad_alaw.c ad_dk3adpcm.c ad_dk4adpcm.c ad_dshow.c ad_dvdpcm.c ad_ffmpeg.c ad_hwac3.c ad_imaadpcm.c ad_mp3.c ad_msadpcm.c ad_pcm.c ad_roqaudio.c ad_msgsm.c ad_faad.c ad_vorbis.c ad_libmad.c
VIDEO_SRCS=dec_video.c vd.c vd_null.c vd_cinepak.c vd_qtrpza.c vd_ffmpeg.c vd_dshow.c vd_vfw.c vd_odivx.c vd_divx4.c vd_raw.c vd_xanim.c vd_msvidc.c vd_fli.c vd_qtrle.c vd_qtsmc.c vd_roqvideo.c vd_cyuv.c vd_nuv.c vd_libmpeg2.c vd_msrle.c vd_huffyuv.c vd_zlib.c vd_mpegpes.c
VFILTER_SRCS=vf.c vf_vo.c vf_crop.c vf_expand.c vf_pp.c vf_scale.c vf_format.c vf_yuy2.c vf_flip.c vf_rgb2bgr.c vf_rotate.c vf_mirror.c vf_palette.c vf_lavc.c vf_dvbscale.c vf_cropdetect.c
ENCODER_SRCS=ve.c ve_divx4.c ve_lavc.c ve_vfw.c ve_rawrgb.c ve_libdv.c
NATIVE_SRCS=native/RTjpegN.c native/cinepak.c native/cyuv.c native/fli.c native/minilzo.c native/msvidc.c native/nuppelvideo.c native/qtrle.c native/qtrpza.c native/qtsmc.c native/roqav.c native/xa_gsm.c

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

CFLAGS  = -Wall $(OPTFLAGS) -I. -Inative -I.. -I../libmpdemux -I../loader $(EXTRA_INC)

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

