/* This audio filter doesn't really do anything useful but serves an
   example of how audio filters work. It delays the output signal by
   the number of seconds set by delay=n where n is the number of
   seconds.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "af.h"

// Data for specific instances of this filter
typedef struct af_delay_s
{
  void* buf; 	    // data block used for delaying audio signal
  int   len;        // local buffer length
  float tlen;       // Delay in seconds
}af_delay_t;

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  switch(cmd){
  case AF_CONTROL_REINIT:{
    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = ((af_data_t*)arg)->nch;
    af->data->format = ((af_data_t*)arg)->format;
    af->data->bps    = ((af_data_t*)arg)->bps;
    
    return af->control(af,AF_CONTROL_SET_DELAY_LEN,&((af_delay_t*)af->setup)->tlen);
  }
  case AF_CONTROL_SET_DELAY_LEN:{
    af_delay_t* s  = (af_delay_t*)af->setup;
    void*       bt = s->buf; // Old buffer
    int         lt = s->len; // Old len

    if(*((float*)arg) > 30 || *((float*)arg) < 0){
      mp_msg(MSGT_AFILTER,MSGL_ERR,"Error setting delay length in af_delay. Delay must be between 0s and 30s\n");
      s->len=0;
      s->tlen=0.0;
      af->delay=0.0;
      return AF_ERROR;
    }

    // Set new len and allocate new buffer
    s->tlen = *((float*)arg);
    af->delay = s->tlen * 1000.0;
    s->len  = af->data->rate*af->data->bps*af->data->nch*(int)s->tlen;
    s->buf  = malloc(s->len);
    mp_msg(MSGT_AFILTER,MSGL_DBG2,"[delay] Delaying audio output by %0.2fs\n",s->tlen);
    mp_msg(MSGT_AFILTER,MSGL_DBG3,"[delay] Delaying audio output by %i bytes\n",s->len);

    // Out of memory error
    if(!s->buf){
      s->len = 0;
      free(bt);
      return AF_ERROR;
    }
      
    // Clear the new buffer
    memset(s->buf, 0, s->len);
    
    /* Copy old buffer to avoid click in output 
       sound (at least most of it) and release it */
    if(bt){
      memcpy(s->buf,bt,min(lt,s->len));
      free(bt);
    }
    return AF_OK;
  }
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void uninit(struct af_instance_s* af)
{
  if(af->data->audio)
    free(af->data->audio);
  if(af->data)
    free(af->data);
  if(((af_delay_t*)(af->setup))->buf)
    free(((af_delay_t*)(af->setup))->buf);
  if(af->setup)
    free(af->setup);
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*   c = data;			// Current working data
  af_data_t*   l = af->data;	 		// Local data
  af_delay_t*  s = (af_delay_t*)af->setup; 	// Setup for this instance
 
  
  if(AF_OK != RESIZE_LOCAL_BUFFER(af , data))
    return NULL;
  
  if(s->len > c->len){ // Delay bigger than buffer
    // Copy beginning of buffer to beginning of output buffer
    memcpy(l->audio,s->buf,c->len);
    // Move buffer left
    memcpy(s->buf,s->buf+c->len,s->len-c->len);
    // Save away current audio to end of buffer
    memcpy(s->buf+s->len-c->len,c->audio,c->len);
  }
  else{
    // Copy end of previous block to beginning of output buffer
    memcpy(l->audio,s->buf,s->len);
    // Copy current block except end
    memcpy(l->audio+s->len,c->audio,c->len-s->len);
    // Save away end of current block for next call
    memcpy(s->buf,c->audio+c->len-s->len,s->len);
  }
  
  // Set output data
  c->audio=l->audio;

  return c;
}

// Allocate memory and set function pointers
static int open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_delay_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  return AF_OK;
}

// Description of this filter
af_info_t af_info_delay = {
    "Delay audio filter",
    "delay",
    "Anders",
    "",
    AF_FLAGS_REENTRANT,
    open
};


