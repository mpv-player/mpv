#include <stdio.h>
#include <stdlib.h>

#include "../config.h"
#include "audio_out.h"
#include "afmt.h"

// there are some globals:
int ao_samplerate=0;
int ao_channels=0;
int ao_format=0;
int ao_bps=0;
int ao_outburst=OUTBURST; // config.h default
int ao_buffersize=-1;
int ao_pts=0;
char *ao_subdevice = NULL;

#ifdef USE_OSS_AUDIO
extern ao_functions_t audio_out_oss;
#endif
//extern ao_functions_t audio_out_ossold;
extern ao_functions_t audio_out_null;
#ifdef HAVE_ALSA5
 extern ao_functions_t audio_out_alsa5;
#endif
#ifdef HAVE_ALSA9
 extern ao_functions_t audio_out_alsa9;
#endif
#ifdef HAVE_ESD
 extern ao_functions_t audio_out_esd;
#endif
#ifdef HAVE_SDL
extern ao_functions_t audio_out_sdl;
#endif
#ifdef USE_SUN_AUDIO
extern ao_functions_t audio_out_sun;
#endif
#ifdef USE_SGI_AUDIO
extern ao_functions_t audio_out_sgi;
#endif
#ifdef HAVE_DXR3
extern ao_functions_t audio_out_dxr3;
#endif
extern ao_functions_t audio_out_pcm;
#ifndef USE_LIBVO2
extern ao_functions_t audio_out_mpegpes;
#endif
extern ao_functions_t audio_out_pss;

ao_functions_t* audio_out_drivers[] =
{
#ifdef USE_OSS_AUDIO
        &audio_out_oss,
#endif
#ifdef USE_SUN_AUDIO
        &audio_out_sun,
#endif
#ifdef USE_SGI_AUDIO
        &audio_out_sgi,
#endif
        &audio_out_null,
#ifdef HAVE_ALSA5
	&audio_out_alsa5,
#endif
#ifdef HAVE_ALSA9
	&audio_out_alsa9,
#endif
#ifdef HAVE_ESD
	&audio_out_esd,
#endif
#ifdef HAVE_SDL
        &audio_out_sdl,
#endif
#ifdef HAVE_DXR3
	&audio_out_dxr3,
#endif
	&audio_out_pcm,
#ifndef USE_LIBVO2
	&audio_out_mpegpes,
#endif
//	&audio_out_pss,
	NULL
};
