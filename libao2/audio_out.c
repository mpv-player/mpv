#include <stdio.h>
#include <stdlib.h>

#include "../config.h"
#include "audio_out.h"
#include "afmt.h"

// there are some globals:
ao_data_t ao_data={0,0,0,0,OUTBURST,-1,0};
char *ao_subdevice = NULL;

#ifdef USE_OSS_AUDIO
extern ao_functions_t audio_out_oss;
#endif
#ifdef USE_ARTS
extern ao_functions_t audio_out_arts;
#endif
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
#ifdef HAVE_NAS
extern ao_functions_t audio_out_nas;
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
#ifdef HAVE_DXR2
extern ao_functions_t audio_out_dxr2;
#endif
extern ao_functions_t audio_out_mpegpes;
extern ao_functions_t audio_out_pcm;
extern ao_functions_t audio_out_pss;
extern ao_functions_t audio_out_plugin;

ao_functions_t* audio_out_drivers[] =
{
#ifdef USE_OSS_AUDIO
        &audio_out_oss,
#endif
#ifdef USE_ARTS
        &audio_out_arts,
#endif
#ifdef USE_SUN_AUDIO
        &audio_out_sun,
#endif
#ifdef USE_SGI_AUDIO
        &audio_out_sgi,
#endif
#ifdef HAVE_DXR2
        &audio_out_dxr2,
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
#ifdef HAVE_NAS
	&audio_out_nas,
#endif
#ifdef HAVE_SDL
        &audio_out_sdl,
#endif
	&audio_out_mpegpes,
	&audio_out_pcm,
	&audio_out_plugin,
//	&audio_out_pss,
	NULL
};



