#include <stdio.h>
#include <stdlib.h>

#include "../config.h"

#include "audio_out.h"

#ifdef	USE_OSS_AUDIO
#include <sys/soundcard.h> /* AFMT_* */
#endif


#ifndef	AFMT_U8
#       define AFMT_MU_LAW              0x00000001
#       define AFMT_A_LAW               0x00000002
#       define AFMT_IMA_ADPCM           0x00000004
#       define AFMT_U8                  0x00000008
#       define AFMT_S16_LE              0x00000010      /* Little endian signed
16*/
#       define AFMT_S16_BE              0x00000020      /* Big endian signed 16
*/
#       define AFMT_S8                  0x00000040
#       define AFMT_U16_LE              0x00000080      /* Little endian U16 */
#       define AFMT_U16_BE              0x00000100      /* Big endian U16 */
#       define AFMT_MPEG                0x00000200      /* MPEG (2) audio */

/* 32 bit formats (MSB aligned) formats */
#       define AFMT_S32_LE              0x00001000
#       define AFMT_S32_BE              0x00002000
#endif


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
/*
#ifdef HAVE_ALSA9
 extern ao_functions_t audio_out_alsa9;
#endif
#ifdef HAVE_ESD
 extern ao_functions_t audio_out_esd;
#endif
*/
#ifdef HAVE_SDL
extern ao_functions_t audio_out_sdl;
#endif
#ifdef USE_SUN_AUDIO
extern ao_functions_t audio_out_sun;
#endif

ao_functions_t* audio_out_drivers[] =
{
#ifdef USE_OSS_AUDIO
        &audio_out_oss,
#endif
        &audio_out_null,
#ifdef HAVE_ALSA5
	&audio_out_alsa5,
#endif
/*
#ifdef HAVE_ALSA9
	&audio_out_alsa9,
#endif
#ifdef HAVE_ESD
	&audio_out_esd,
#endif
*/
#ifdef HAVE_SDL
        &audio_out_sdl,
#endif
#ifdef USE_SUN_AUDIO
        &audio_out_sun,
#endif
	NULL
};

char *audio_out_format_name(int format)
{
    switch (format)
    {
	case AFMT_S8:
	    return("signed 8-bit");
	case AFMT_U8:
	    return("unsigned 8-bit");
	case AFMT_U16_LE:
	    return("unsigned 16-bit (little-endian)");
	case AFMT_U16_BE: 
	    return("unsigned 16-bit (big-endian)");
	case AFMT_S16_LE:
	    return("signed 16-bit (little-endian)");
	case AFMT_S16_BE:
	    return("unsigned 16-bit (big-endian)");
    }
    return("unknown");
}
