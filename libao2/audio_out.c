#include <stdio.h>
#include <stdlib.h>

#include "../config.h"
#include "audio_out.h"
#include "afmt.h"

#include "../mp_msg.h"
#include "../help_mp.h"

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
// vo-related:   will fail unless you also do -vo mpegpes/dxr2
	&audio_out_mpegpes,
#ifdef HAVE_DXR2
        &audio_out_dxr2,
#endif
// native:
#ifdef USE_OSS_AUDIO
        &audio_out_oss,
#endif
#ifdef HAVE_ALSA9
	&audio_out_alsa9,
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
#ifdef HAVE_NAS
	&audio_out_nas,
#endif
#ifdef HAVE_SDL
        &audio_out_sdl,
#endif
        &audio_out_null,
// should not be auto-selected:
	&audio_out_pcm,
	&audio_out_plugin,
	NULL
};

void list_audio_out(){
      int i=0;
      mp_msg(MSGT_AO, MSGL_INFO, MSGTR_AvailableAudioOutputDrivers);
      while (audio_out_drivers[i]) {
        const ao_info_t *info = audio_out_drivers[i++]->info;
	printf("\t%s\t%s\n", info->short_name, info->name);
      }
      printf("\n");
}

ao_functions_t* init_best_audio_out(char** ao_list,int use_plugin,int rate,int channels,int format,int flags){
    int i;
    // first try the preferred drivers, with their optional subdevice param:
    if(ao_list && ao_list[0])
      while(ao_list[0][0]){
        char* ao=strdup(ao_list[0]);
	ao_subdevice=strchr(ao,':');
	if(ao_subdevice){
	    ao_subdevice[0]=0;
	    ++ao_subdevice;
	}
	for(i=0;audio_out_drivers[i];i++){
	    ao_functions_t* audio_out=audio_out_drivers[i];
	    if(!strcmp(audio_out->info->short_name,ao)){
		// name matches, try it
		if(use_plugin){
		    audio_out_plugin.control(AOCONTROL_SET_PLUGIN_DRIVER,(int)audio_out);
		    audio_out=&audio_out_plugin;
		}
		if(audio_out->init(rate,channels,format,flags))
		    return audio_out; // success!
	    }
	}
        // continue...
	++ao_list;
	if(!(ao_list[0])) return NULL; // do NOT fallback to others
      }
    // now try the rest...
    ao_subdevice=NULL;
    for(i=0;audio_out_drivers[i];i++){
	ao_functions_t* audio_out=audio_out_drivers[i];
	if(use_plugin){
	    audio_out_plugin.control(AOCONTROL_SET_PLUGIN_DRIVER,(int)audio_out);
	    audio_out=&audio_out_plugin;
	}
	if(audio_out->init(rate,channels,format,flags))
	    return audio_out; // success!
    }
    return NULL;
}

