
#include <stdio.h>
#include <stdlib.h>

#include "../config.h"

#include "audio_out.h"

// there are some globals:
int ao_samplerate=0;
int ao_channels=0;
int ao_format=0;
int ao_bps=0;
int ao_outburst=OUTBURST; // config.h default
int ao_buffersize=-1;

extern ao_functions_t audio_out_oss;
//extern ao_functions_t audio_out_ossold;
//extern ao_functions_t audio_out_alsa;
//extern ao_functions_t audio_out_esd;
extern ao_functions_t audio_out_null;
#ifdef HAVE_SDL
extern ao_functions_t audio_out_sdl;
#endif

ao_functions_t* audio_out_drivers[] =
{
        &audio_out_oss,
        &audio_out_null,
#ifdef HAVE_SDL
        &audio_out_sdl,
#endif
	NULL
};

