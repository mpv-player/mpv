/*
   ad.c - audio decoder interface
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "ad.h"

/* Missed vorbis, mad, dshow */

//extern ad_functions_t mpcodecs_ad_null;
extern ad_functions_t mpcodecs_ad_mp3lib;
extern ad_functions_t mpcodecs_ad_ffmpeg;
extern ad_functions_t mpcodecs_ad_liba52;
extern ad_functions_t mpcodecs_ad_hwac3;
extern ad_functions_t mpcodecs_ad_hwmpa;
extern ad_functions_t mpcodecs_ad_pcm;
extern ad_functions_t mpcodecs_ad_dvdpcm;
extern ad_functions_t mpcodecs_ad_alaw;
extern ad_functions_t mpcodecs_ad_imaadpcm;
extern ad_functions_t mpcodecs_ad_msadpcm;
extern ad_functions_t mpcodecs_ad_dk3adpcm;
extern ad_functions_t mpcodecs_ad_dk4adpcm;
extern ad_functions_t mpcodecs_ad_dshow;
extern ad_functions_t mpcodecs_ad_dmo;
extern ad_functions_t mpcodecs_ad_acm;
extern ad_functions_t mpcodecs_ad_msgsm;
extern ad_functions_t mpcodecs_ad_faad;
extern ad_functions_t mpcodecs_ad_libvorbis;
extern ad_functions_t mpcodecs_ad_speex;
extern ad_functions_t mpcodecs_ad_libmad;
extern ad_functions_t mpcodecs_ad_realaud;
extern ad_functions_t mpcodecs_ad_libdv;
extern ad_functions_t mpcodecs_ad_qtaudio;
extern ad_functions_t mpcodecs_ad_twin;
extern ad_functions_t mpcodecs_ad_libmusepack;
extern ad_functions_t mpcodecs_ad_libdca;

ad_functions_t* mpcodecs_ad_drivers[] =
{
//  &mpcodecs_ad_null,
#ifdef CONFIG_MP3LIB
  &mpcodecs_ad_mp3lib,
#endif
#ifdef CONFIG_LIBA52
  &mpcodecs_ad_liba52,
  &mpcodecs_ad_hwac3,
#endif
  &mpcodecs_ad_hwmpa,
#ifdef CONFIG_LIBAVCODEC
  &mpcodecs_ad_ffmpeg,
#endif
  &mpcodecs_ad_pcm,
  &mpcodecs_ad_dvdpcm,
  &mpcodecs_ad_alaw,
  &mpcodecs_ad_imaadpcm,
  &mpcodecs_ad_msadpcm,
  &mpcodecs_ad_dk3adpcm,
  &mpcodecs_ad_msgsm,
#ifdef CONFIG_WIN32DLL
  &mpcodecs_ad_dshow,
  &mpcodecs_ad_dmo,
  &mpcodecs_ad_acm,
  &mpcodecs_ad_twin,
#endif
#ifdef CONFIG_QTX_CODECS
  &mpcodecs_ad_qtaudio,
#endif
#ifdef CONFIG_FAAD
  &mpcodecs_ad_faad,
#endif
#ifdef CONFIG_OGGVORBIS
  &mpcodecs_ad_libvorbis,
#endif
#ifdef CONFIG_SPEEX
  &mpcodecs_ad_speex,
#endif
#ifdef CONFIG_LIBMAD
  &mpcodecs_ad_libmad,
#endif
#ifdef CONFIG_REALCODECS
  &mpcodecs_ad_realaud,
#endif
#ifdef CONFIG_LIBDV095
  &mpcodecs_ad_libdv,
#endif
#ifdef CONFIG_MUSEPACK
  &mpcodecs_ad_libmusepack,
#endif
#ifdef CONFIG_LIBDCA
  &mpcodecs_ad_libdca,
#endif
  NULL
};
