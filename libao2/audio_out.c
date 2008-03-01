#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "audio_out.h"

#include "mp_msg.h"
#include "help_mp.h"
#include "mplayer.h" /* for exit_player() */

// there are some globals:
ao_data_t ao_data={0,0,0,0,OUTBURST,-1,0};
char *ao_subdevice = NULL;

extern ao_functions_t audio_out_oss;
extern ao_functions_t audio_out_macosx;
extern ao_functions_t audio_out_arts;
extern ao_functions_t audio_out_esd;
extern ao_functions_t audio_out_pulse;
extern ao_functions_t audio_out_jack;
extern ao_functions_t audio_out_openal;
extern ao_functions_t audio_out_null;
extern ao_functions_t audio_out_alsa5;
extern ao_functions_t audio_out_alsa;
extern ao_functions_t audio_out_nas;
extern ao_functions_t audio_out_sdl;
extern ao_functions_t audio_out_sun;
extern ao_functions_t audio_out_sgi;
extern ao_functions_t audio_out_win32;
extern ao_functions_t audio_out_dsound;
extern ao_functions_t audio_out_dxr2;
extern ao_functions_t audio_out_ivtv;
extern ao_functions_t audio_out_v4l2;
extern ao_functions_t audio_out_mpegpes;
extern ao_functions_t audio_out_pcm;
extern ao_functions_t audio_out_pss;

const ao_functions_t* const audio_out_drivers[] =
{
// native:
#ifdef HAVE_DIRECTX
        &audio_out_dsound,
#endif
#ifdef HAVE_WIN32WAVEOUT
        &audio_out_win32,
#endif
#ifdef MACOSX
        &audio_out_macosx,
#endif
#ifdef USE_OSS_AUDIO
        &audio_out_oss,
#endif
#ifdef HAVE_ALSA1X
        &audio_out_alsa,
#endif
#ifdef HAVE_ALSA9
        &audio_out_alsa,
#endif
#ifdef HAVE_ALSA5
        &audio_out_alsa5,
#endif
#ifdef USE_SGI_AUDIO
        &audio_out_sgi,
#endif
#ifdef USE_SUN_AUDIO
        &audio_out_sun,
#endif
// wrappers:
#ifdef USE_ARTS
        &audio_out_arts,
#endif
#ifdef USE_ESD
        &audio_out_esd,
#endif
#ifdef USE_PULSE
        &audio_out_pulse,
#endif
#ifdef USE_JACK
        &audio_out_jack,
#endif
#ifdef HAVE_NAS
        &audio_out_nas,
#endif
#ifdef HAVE_SDL
        &audio_out_sdl,
#endif
#ifdef USE_OPENAL
        &audio_out_openal,
#endif
        &audio_out_mpegpes,
#ifdef HAVE_DXR2
        &audio_out_dxr2,
#endif
#ifdef HAVE_IVTV
        &audio_out_ivtv,
#endif
#ifdef HAVE_V4L2_DECODER
        &audio_out_v4l2,
#endif
        &audio_out_null,
// should not be auto-selected:
        &audio_out_pcm,
        NULL
};

void list_audio_out(void){
    int i=0;
    mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AvailableAudioOutputDrivers);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_OUTPUTS\n");
    while (audio_out_drivers[i]) {
        const ao_info_t *info = audio_out_drivers[i++]->info;
        mp_msg(MSGT_GLOBAL, MSGL_INFO,"\t%s\t%s\n", info->short_name, info->name);
    }
    mp_msg(MSGT_GLOBAL, MSGL_INFO,"\n");
}

const ao_functions_t* init_best_audio_out(char** ao_list,int use_plugin,int rate,int channels,int format,int flags){
    int i;
    // first try the preferred drivers, with their optional subdevice param:
    if(ao_list && ao_list[0])
      while(ao_list[0][0]){
        char* ao=ao_list[0];
        int ao_len;
        if (strncmp(ao, "alsa9", 5) == 0 || strncmp(ao, "alsa1x", 6) == 0) {
            mp_msg(MSGT_AO, MSGL_FATAL, MSGTR_AO_ALSA9_1x_Removed);
            exit_player(NULL);
        }
        if (ao_subdevice) {
            free(ao_subdevice);
            ao_subdevice = NULL;
        }
        ao_subdevice=strchr(ao,':');
        if(ao_subdevice){
            ao_len = ao_subdevice - ao;
            ao_subdevice = strdup(&ao[ao_len + 1]);
        }
        else
            ao_len = strlen(ao);
        for(i=0;audio_out_drivers[i];i++){
            const ao_functions_t* audio_out=audio_out_drivers[i];
            if(!strncmp(audio_out->info->short_name,ao,ao_len)){
                // name matches, try it
                if(audio_out->init(rate,channels,format,flags))
                    return audio_out; // success!
            }
        }
        // continue...
        ++ao_list;
        if(!(ao_list[0])) return NULL; // do NOT fallback to others
      }
    if (ao_subdevice) {
        free(ao_subdevice);
        ao_subdevice = NULL;
    }
    // now try the rest...
    for(i=0;audio_out_drivers[i];i++){
        const ao_functions_t* audio_out=audio_out_drivers[i];
//        if(audio_out->control(AOCONTROL_QUERY_FORMAT, (int)format) == CONTROL_TRUE)
        if(audio_out->init(rate,channels,format,flags))
            return audio_out; // success!
    }
    return NULL;
}

