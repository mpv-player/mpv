#include <stdio.h>
#include <stdlib.h>

#include "../config.h"

#include "afmt.h"
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

#define plugin(i) (ao_plugin_local_data.plugins[i])
#define driver() (ao_plugin_local_data.driver)

// local data 
typedef struct ao_plugin_local_data_s
{
  void* buf;					 // Output data buffer
  int len;					 // Amount of data in buffer
  float bps;					 // Bytes per second out
  ao_functions_t* driver;      			 // Output driver
  ao_plugin_functions_t** plugins;               // List of used plugins
  ao_plugin_functions_t* available_plugins[NPL]; // List of available plugins
} ao_plugin_local_data_t;

static ao_plugin_local_data_t ao_plugin_local_data={NULL,0,0.0,NULL,NULL,AO_PLUGINS};

// global data 
volatile ao_plugin_data_t ao_plugin_data;             // Data used by the plugins
ao_plugin_cfg_t  ao_plugin_cfg=CFG_DEFAULTS; // Set in cfg-mplayer.h

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_SET_PLUGIN_DRIVER:
    ao_plugin_local_data.driver=(ao_functions_t*)arg;
    return CONTROL_OK;
  case AOCONTROL_GET_VOLUME:
  case AOCONTROL_SET_VOLUME:
  {
    int r=audio_plugin_volume.control(cmd,arg);
    if(CONTROL_OK != r)
      return driver()->control(cmd,arg);
    else
      return r;
  }
  default:
    return driver()->control(cmd,arg);
  }
  return CONTROL_UNKNOWN;
}

// Recursive function for adding plugins
// return 1 for success and 0 for error
int add_plugin(int i,char* cfg){
  int cnt=0;
  // Find end of plugin name
  while((cfg[cnt]!=',')&&(cfg[cnt]!='\0')&&(cnt<100)) cnt++;
  if(cnt >= 100)
    return 0;

  // Is this the last iteration or just another plugin
  if(cfg[cnt]=='\0'){
    ao_plugin_local_data.plugins=malloc((i+1)*sizeof(ao_plugin_functions_t*));
    if(ao_plugin_local_data.plugins){
      ao_plugin_local_data.plugins[i+1]=NULL;
      // Find the plugin matching the cfg string name
      cnt=0;
      while(ao_plugin_local_data.available_plugins[cnt] && cnt<20){
	if(0==strcmp(ao_plugin_local_data.available_plugins[cnt]->info->short_name,cfg)){
	  ao_plugin_local_data.plugins[i]=ao_plugin_local_data.available_plugins[cnt];
	  return 1;
	}
	cnt++;
      }
      printf("[plugin]: Invalid plugin: %s \n",cfg);
      return 0;
    }
    else 
      return 0;
  } else {
    cfg[cnt]='\0';
    if(add_plugin(i+1,&cfg[cnt+1])){
      cnt=0;
      // Find the plugin matching the cfg string name
      while(ao_plugin_local_data.available_plugins[cnt] && cnt < 20){
	if(0==strcmp(ao_plugin_local_data.available_plugins[cnt]->info->short_name,cfg)){
	  ao_plugin_local_data.plugins[i]=ao_plugin_local_data.available_plugins[cnt];
	  return 1;
	}
	cnt++;
      }
      printf("[plugin]: Invalid plugin: %s \n",cfg);
      return 0;
    }
    else 
      return 0;
  }	
  return 0; // Will never happen...
}

// open & setup audio device and plugins
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){
  int ok=1;

  // Create list of plugins from cfg option
  int i=0; 
  if(ao_plugin_cfg.plugin_list){
    if(!add_plugin(i,ao_plugin_cfg.plugin_list))
      return 0;
  }

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

  // Calculate bps
  ao_plugin_local_data.bps=(float)(ao_plugin_data.rate * 
				   ao_plugin_data.channels);

  if(ao_plugin_data.format == AFMT_S16_LE ||
     ao_plugin_data.format == AFMT_S16_BE ||
     ao_plugin_data.format == AFMT_U16_LE ||
     ao_plugin_data.format == AFMT_U16_BE)
    ao_plugin_local_data.bps *= 2;

  if(ao_plugin_data.format == AFMT_S32_LE ||
     ao_plugin_data.format == AFMT_S32_BE)
    ao_plugin_local_data.bps *= 4;

  // This should never happen but check anyway 
  if(NULL==ao_plugin_local_data.driver)
    return 0;
  
  ok = driver()->init(ao_plugin_data.rate,
		      ao_plugin_data.channels,
		      ao_plugin_data.format,
		      flags);
  if(!ok) return 0;

  /* Now that the driver is initialized we can calculate and set the
     input and output buffers for each plugin */
  ao_plugin_data.len=driver()->get_space();
  while((i>0) && ok)
    ok=plugin(--i)->control(AOCONTROL_PLUGIN_SET_LEN,0);

  if(!ok) return 0;

  // Allocate output buffer
  if(ao_plugin_local_data.buf)
    free(ao_plugin_local_data.buf);
  ao_plugin_local_data.buf=malloc(MAX_OUTBURST);

  if(!ao_plugin_local_data.buf) return 0;

  return 1;
}

// close audio device
static void uninit(){
  int i=0;
  driver()->uninit();
  while(plugin(i))
    plugin(i++)->uninit();
  if(ao_plugin_local_data.plugins)
    free(ao_plugin_local_data.plugins);
  ao_plugin_local_data.plugins=NULL;
  if(ao_plugin_local_data.buf)
    free(ao_plugin_local_data.buf);
  ao_plugin_local_data.buf=NULL;
}

// stop playing and empty buffers (for seeking/pause)
static void reset(){
  int i=0;
  driver()->reset();
  while(plugin(i))
    plugin(i++)->reset();
  ao_plugin_local_data.len=0;
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
  if(sz+(double)ao_plugin_local_data.len > (double)MAX_OUTBURST)
    sz=(double)MAX_OUTBURST-(double)ao_plugin_local_data.len;
  sz*=ao_plugin_data.sz_mult;
  sz+=ao_plugin_data.sz_fix;
  return (int)(sz);
}

// plays 'len' bytes of 'data'
// return: number of bytes played
static int play(void* data,int len,int flags){
  int l,i=0;
  // Limit length to avoid over flow in plugins
  int tmp = get_space();
  int ret_len =(tmp<len)?tmp:len;
  if(ret_len){
    // Filter data
    ao_plugin_data.len=ret_len;
    ao_plugin_data.data=data;
    while(plugin(i))
      plugin(i++)->play();
    // Copy data to output buffer
    memcpy(ao_plugin_local_data.buf+ao_plugin_local_data.len,
	   ao_plugin_data.data,ao_plugin_data.len);
    // Send data to output
    l=driver()->play(ao_plugin_local_data.buf,
		     ao_plugin_data.len+ao_plugin_local_data.len,flags);
    // Save away unsent data
    ao_plugin_local_data.len=ao_plugin_data.len+ao_plugin_local_data.len-l;
    memcpy(ao_plugin_local_data.buf,ao_plugin_local_data.buf+l,
	   ao_plugin_local_data.len);
  }
  return ret_len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(){
  float delay=driver()->get_delay();
  delay+=(float)ao_plugin_local_data.len/ao_plugin_local_data.bps;
  delay*=ao_plugin_data.delay_mult;
  delay+=ao_plugin_data.delay_fix;
  return delay;
}




