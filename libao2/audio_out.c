#include <stdio.h>
#include <stdlib.h>

#include "../config.h"

#include "audio_out.h"

#include <sys/soundcard.h> /* AFMT_* */

// there are some globals:
int ao_samplerate=0;
int ao_channels=0;
int ao_format=0;
int ao_bps=0;
int ao_outburst=OUTBURST; // config.h default
int ao_buffersize=-1;

extern ao_functions_t audio_out_oss;
//extern ao_functions_t audio_out_ossold;
extern ao_functions_t audio_out_null;
#ifdef HAVE_ALSA5
 extern ao_functions_t audio_out_alsa5;
#endif
//extern ao_functions_t audio_out_alsa9;
extern ao_functions_t audio_out_esd;
#ifdef HAVE_SDL
extern ao_functions_t audio_out_sdl;
#endif

ao_functions_t* audio_out_drivers[] =
{
        &audio_out_oss,
        &audio_out_null,
#ifdef HAVE_ALSA5
	&audio_out_alsa5,
#endif
//	&audio_out_alsa9,
//	&audio_out_esd,
#ifdef HAVE_SDL
        &audio_out_sdl,
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
