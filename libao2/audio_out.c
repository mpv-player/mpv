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
extern ao_functions_t audio_out_pcm;

ao_functions_t* audio_out_drivers[] =
{
#ifdef USE_OSS_AUDIO
        &audio_out_oss,
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
#ifdef USE_SUN_AUDIO
        &audio_out_sun,
#endif
	&audio_out_pcm,
	NULL
};

char *audio_out_format_name(int format)
{
    switch (format)
    {
/*
	case AFMT_MU_LAW:
	case AFMT_A_LAW:
	case AFMT_IMA_ADPCM:
*/
	case AFMT_S8:
	    return("Signed 8-bit");
	case AFMT_U8:
	    return("Unsigned 8-bit");
	case AFMT_U16_LE:
	    return("Unsigned 16-bit (Little-Endian)");
	case AFMT_U16_BE: 
	    return("Unsigned 16-bit (Big-Endian)");
	case AFMT_S16_LE:
	    return("Signed 16-bit (Little-Endian)");
	case AFMT_S16_BE:
	    return("Unsigned 16-bit (Big-Endian)");
	case AFMT_MPEG:
	    return("MPEG (2) audio");
	// the following two formats are not available with old linux kernel
	// headers (e.g. in 2.2.16)
#ifdef AFMT_S32_LE
	case AFMT_S32_LE:
	    return("Signed 32-bit (Little-Endian)");
#endif
#ifdef AFMT_S32_BE
	case AFMT_S32_BE:
	    return("Signed 32-bit (Big-Endian)");
#endif
    }
    return("Unknown");
}
