/*
  ao_sgi - sgi/irix output plugin for MPlayer

  22oct2001 oliver.schoenbrunner@jku.at
  
*/

#include <stdio.h>
#include <stdlib.h>
#include <dmedia/audio.h>

#include "audio_out.h"
#include "audio_out_internal.h"

static ao_info_t info = 
{
	"sgi audio output",
	"sgi",
	"kopflos",
	""
};

LIBAO_EXTERN(sgi)


static ALconfig	ao_config;
static ALport	ao_port;

// to set/get/query special features/parameters
static int control(int cmd, void *arg){
  
  printf("ao_sgi, control\n");
  
  return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate, int channels, int format, int flags) {
  
  printf("ao_sgi, init: Samplerate: %iHz Channels: %s Format %s\n", rate, (channels > 1) ? "Stereo" : "Mono", audio_out_format_name(format));
  
  { /* from /usr/share/src/dmedia/audio/setrate.c */
  
    int fd;
    int rv;
    double frate;
    ALpv x[2];

    rv = alGetResourceByName(AL_SYSTEM, "out.analog", AL_DEVICE_TYPE);
    if (!rv) {
      printf("ao_sgi, play: invalid device\n");
      return 0;
    }
    
    frate = rate;

    x[0].param = AL_RATE;
    x[0].value.ll = alDoubleToFixed(rate);
    x[1].param = AL_MASTER_CLOCK;
    x[1].value.i = AL_CRYSTAL_MCLK_TYPE;

    if (alSetParams(rv,x, 2)<0) {
      printf("ao_sgi, init: setparams failed: %s\n", alGetErrorString(oserror()));
      printf("ao_sgi, init: could not set desired samplerate\n"); 
    }
    
    if (x[0].sizeOut < 0) {
      printf("ao_sgi, init: AL_RATE was not accepted on the given resource\n");
    }

    if (alGetParams(rv,x, 1)<0) {
      printf("ao_sgi, init: getparams failed: %s\n", alGetErrorString(oserror()));
    }
    
    if (frate != alFixedToDouble(x[0].value.ll)) {
      printf("ao_sgi, init: samplerate is now %lf (desired rate is %lf)\n", alFixedToDouble(x[0].value.ll), frate);
    } 

  }
  
  ao_data.buffersize=131072;
  ao_data.outburst = ao_data.buffersize/16;
  ao_data.channels = channels;
  
  ao_config = alNewConfig();
  
  if (!ao_config) {
    printf("ao_sgi, init: %s\n", alGetErrorString(oserror()));
    return 0;
  }
  
  if(channels == 2) alSetChannels(ao_config, AL_STEREO);
  else alSetChannels(ao_config, AL_MONO);
  
  alSetWidth(ao_config, AL_SAMPLE_16);
  alSetSampFmt(ao_config, AL_SAMPFMT_TWOSCOMP);
  alSetQueueSize(ao_config, 48000);
  
  if (alSetDevice(ao_config, AL_DEFAULT_OUTPUT) < 0) {
    printf("ao_sgi, init: %s\n", alGetErrorString(oserror()));
    return 0;
  }
  
  ao_port = alOpenPort("mplayer", "w", ao_config);
  
  if (!ao_port) {
    printf("ao_sgi, init: Unable to open audio channel: %s\n", alGetErrorString(oserror()));
    return 0;
  }
  
  // printf("ao_sgi, init: port %d config %d\n", ao_port, ao_config);
  
  return 1;  

}

// close audio device
static void uninit() {

  /* TODO: samplerate should be set back to the value before mplayer was started! */

  printf("ao_sgi, uninit: ...\n");

  if (ao_port) {
    while(alGetFilled(ao_port) > 0) sginap(1);  
    alClosePort(ao_port);
    alFreeConfig(ao_config);
  }
	
}

// stop playing and empty buffers (for seeking/pause)
static void reset() {
  
  printf("ao_sgi, reset: ...\n");
  
}

// stop playing, keep buffers (for pause)
static void audio_pause() {
    
  printf("ao_sgi, audio_pause: ...\n");
    
}

// resume playing, after audio_pause()
static void audio_resume() {

  printf("ao_sgi, audio_resume: ...\n");

}

// return: how many bytes can be played without blocking
static int get_space() {
  
  // printf("ao_sgi, get_space: (ao_outburst %d)\n", ao_outburst);
  // printf("ao_sgi, get_space: alGetFillable [%d] \n", alGetFillable(ao_port));
  
  return alGetFillable(ao_port)*(2*ao_data.channels);
    
}


// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data, int len, int flags) {
    
  // printf("ao_sgi, play: len %d flags %d (%d %d)\n", len, flags, ao_port, ao_config);
  // printf("channels %d\n", ao_channels);

  alWriteFrames(ao_port, data, len/(2*ao_data.channels));
  
  return len;
  
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(){
  
  // printf("ao_sgi, get_delay: (ao_buffersize %d)\n", ao_buffersize);
  
  return 0;

}






