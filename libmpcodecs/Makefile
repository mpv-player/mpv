
include ../config.mak

LIBNAME = libmpcodecs.a
LIBNAME2 = libmpencoders.a

AUDIO_SRCS_LIB=ad_liba52.c ad_hwac3.c ad_mp3lib.c
AUDIO_SRCS_NAT=ad_alaw.c ad_dk3adpcm.c ad_pcm.c ad_dvdpcm.c ad_imaadpcm.c ad_msadpcm.c ad_msgsm.c ad_roqaudio.c ad_ra1428.c
AUDIO_SRCS_OPT=ad_acm.c ad_dshow.c ad_dmo.c ad_qtaudio.c ad_ffmpeg.c ad_faad.c ad_libvorbis.c ad_libmad.c ad_realaud.c ad_libdv.c
AUDIO_SRCS=dec_audio.c ad.c $(AUDIO_SRCS_LIB) $(AUDIO_SRCS_NAT) $(AUDIO_SRCS_OPT)

VIDEO_SRCS_LIB=vd_libmpeg2.c vd_nuv.c vd_lzo.c
VIDEO_SRCS_NAT=vd_null.c vd_cinepak.c vd_raw.c vd_hmblck.c vd_roqvideo.c vd_cyuv.c vd_mpegpes.c vd_mtga.c vd_sgi.c
VIDEO_SRCS_OPT=vd_realvid.c vd_ffmpeg.c vd_dshow.c vd_dmo.c vd_vfw.c vd_vfwex.c vd_odivx.c vd_divx4.c vd_zrmjpeg.c vd_xanim.c vd_xvid.c vd_xvid4.c vd_libdv.c vd_qtvideo.c vd_theora.c
VIDEO_SRCS=dec_video.c vd.c $(VIDEO_SRCS_NAT) $(VIDEO_SRCS_LIB) $(VIDEO_SRCS_OPT)

VFILTER_SRCS=vf.c vf_vo.c vf_crop.c vf_expand.c vf_scale.c vf_format.c vf_noformat.c vf_yuy2.c vf_flip.c vf_rgb2bgr.c vf_rotate.c vf_mirror.c vf_palette.c vf_lavc.c vf_dvbscale.c vf_cropdetect.c vf_test.c vf_noise.c vf_yvu9.c vf_rectangle.c vf_lavcdeint.c vf_eq.c vf_eq2.c vf_halfpack.c vf_dint.c vf_1bpp.c vf_bmovl.c vf_2xsai.c vf_unsharp.c vf_swapuv.c vf_il.c vf_boxblur.c vf_sab.c vf_smartblur.c vf_perspective.c vf_down3dright.c vf_field.c vf_denoise3d.c vf_hqdn3d.c vf_detc.c vf_telecine.c vf_tfields.c vf_ivtc.c vf_ilpack.c vf_dsize.c vf_decimate.c vf_softpulldown.c vf_tinterlace.c vf_pullup.c pullup.c vf_framestep.c vf_tile.c vf_delogo.c vf_fil.c vf_hue.c vf_spp.c vf_yuvcsp.c vf_filmdint.c vf_kerndeint.c vf_rgbtest.c vf_qp.c vf_phase.c vf_divtc.c vf_harddup.c vf_softskip.c
ifeq ($(HAVE_FFPOSTPROCESS),yes)
VFILTER_SRCS += vf_pp.c
endif

ENCODER_SRCS=ve.c ve_divx4.c ve_lavc.c ve_vfw.c ve_raw.c ve_libdv.c ve_xvid.c ve_xvid4.c ve_qtvideo.c ve_nuv.c

NATIVE_SRCS=native/RTjpegN.c native/cinepak.c native/minilzo.c native/nuppelvideo.c native/qtrle.c native/roqav.c native/xa_gsm.c native/decode144.c native/decode288.c

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

SRCS=$(AUDIO_SRCS) $(VIDEO_SRCS) $(VFILTER_SRCS) $(NATIVE_SRCS) img_format.c
OBJS=$(SRCS:.c=.o)

SRCS2=$(ENCODER_SRCS)
OBJS2=$(SRCS2:.c=.o)

CFLAGS  = $(OPTFLAGS) -I. -Inative -I.. -I../libmpdemux -I../loader $(EXTRA_INC) -D_GNU_SOURCE

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

