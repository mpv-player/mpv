#include <stdio.h>
#include <stdlib.h>

#include "../config.h"

#include "audio_out.h"
#include "audio_out_internal.h"

#include "audio_plugin.h"

static ao_info_t info = 
{
	"Plugin audio output",
	"plugin",
	"Anders",
	""
};

LIBAO_EXTERN(plugin)

#define plugin(i) (ao_plugin_local_data.ao_plugins[i])
#define driver() (ao_plugin_local_data.ao_driver)

/* local data */
typedef struct ao_plugin_local_data_s
{
  ao_plugin_functions_t** ao_plugins; /* List of all plugins */
  ao_functions_t* ao_driver;          /* ao driver used by ao_plugin */
} ao_plugin_local_data_t;

ao_plugin_local_data_t ao_plugin_local_data;

/* gloabal data */
ao_plugin_data_t ao_plugin_data;

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  return driver()->control(cmd,arg);
}

// open & setup audio device and plugins
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){
  int ok=1;

  /* FIXME these are cfg file parameters */
  int i=0; 
  ao_plugin_local_data.ao_plugins=malloc((i+1)*sizeof(ao_plugin_functions_t*));
  plugin(i)=NULL;
  ao_plugin_local_data.ao_driver=audio_out_drivers[1];

  /* Set input parameters and itterate through plugins each plugin
     changes the parameters according to its output */
  ao_plugin_data.rate=rate;
  ao_plugin_data.channels=channels;
  ao_plugin_data.format=format;
  ao_plugin_data.sz_mult=1;
  ao_plugin_data.sz_fix=0;
  ao_plugin_data.delay_mult=1;
  ao_plugin_data.delay_fix=0;
  i=0;
  while(plugin(i)&&ok)
    ok=plugin(i++)->init();
  
  if(!ok) return 0;

  ok = driver()->init(ao_plugin_data.rate,
			ao_plugin_data.channels,
			ao_plugin_data.format,
			flags);
  if(!ok) return 0;

  /* Now that the driver is initialized we can calculate and set the
     input and output buffers for each plugin */
  ao_plugin_data.len=driver()->get_space();

  return 1;
}

// close audio device
static void uninit(){
  int i=0;
  driver()->uninit();
  while(plugin(i))
    plugin(i++)->uninit();
  if(ao_plugin_local_data.ao_plugins)
    free(ao_plugin_local_data.ao_plugins);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(){
  int i=0;
  driver()->reset();
  while(plugin(i))
    plugin(i++)->reset();
}

// stop playing, keep buffers (for pause)
static void audio_pause(){
  driver()->pause();
}

// resume playing, after audio_pause()
static void audio_resume(){
  driver()->resume();
}

// return: how many bytes can be played without blocking
static int get_space(){
  double sz=(double)(driver()->get_space());
  sz*=ao_plugin_data.sz_mult;
  sz+=ao_plugin_data.sz_fix;
  return (int)(sz);
}

// plays 'len' bytes of 'data'
// return: number of bytes played
static int play(void* data,int len,int flags){
  int i=0;
  /* Due to constant buffer sizes in plugins limit length */
  int tmp = get_space();
  int ret_len =(tmp<len)?tmp:len;
  /* Filter data */
  ao_plugin_data.len=ret_len;  
  ao_plugin_data.data=data;
  while(plugin(i))
    plugin(i++)->play();
  /* Send data to output */
  len=driver()->play(ao_plugin_data.data,ao_plugin_data.len,flags);

  if(len!=ao_plugin_data.len)
    printf("Buffer over flow in sound plugin ");
  
  return ret_len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(){
  float delay=driver()->get_delay();
  delay*=ao_plugin_data.delay_mult;
  delay+=ao_plugin_data.delay_fix;
  return delay;
}


