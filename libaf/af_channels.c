/* Audio filter that adds and removes channels, according to the
   command line parameter channels. It is stupid and can only add
   silence or copy channels not mix or filter.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "af.h"

// Local function for copying data
void copy(void* in, void* out, int ins, int inos,int outs, int outos, int len, int bps)
{
  switch(bps){
  case 1:{
    int8_t* tin  = (int8_t*)in;
    int8_t* tout = (int8_t*)out;
    tin  += inos;
    tout += outos;
    len = len/ins;
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  case 2:{
    int16_t* tin  = (int16_t*)in;
    int16_t* tout = (int16_t*)out;
    tin  += inos;
    tout += outos;
    len = len/(2*ins);
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  case 4:{
    int32_t* tin  = (int32_t*)in;
    int32_t* tout = (int32_t*)out;
    tin  += inos;
    tout += outos;
    len = len/(4*ins);
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  case 8:{
    int64_t* tin  = (int64_t*)in;
    int64_t* tout = (int64_t*)out;
    tin  += inos;
    tout += outos;
    len = len/(8*ins);
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  default:
    mp_msg(MSGT_AFILTER,MSGL_ERR,"[channels] Unsupported number of bytes/sample: %i please report this error on the MPlayer mailing list. \n",bps);
  }
}

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  switch(cmd){
  case AF_CONTROL_REINIT:
    // Make sure this filter isn't redundant 
    if(af->data->nch == ((af_data_t*)arg)->nch)
      return AF_DETACH;

    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->format = ((af_data_t*)arg)->format;
    af->data->bps    = ((af_data_t*)arg)->bps;
    af->mul.n        = af->data->nch;
    af->mul.d	     = ((af_data_t*)arg)->nch;
    return AF_OK;
  case AF_CONTROL_CHANNELS: 
    // Reinit must be called after this function has been called
    
    // Sanity check
    if(((int*)arg)[0] <= 0 || ((int*)arg)[0] > 6){
      mp_msg(MSGT_AFILTER,MSGL_ERR,"[channels] The number of output channels must be between 1 and 6. Current value is%i \n",((int*)arg)[0]);
      return AF_ERROR;
    }

    af->data->nch=((int*)arg)[0]; 
    mp_msg(MSGT_AFILTER,MSGL_V,"[channels] Changing number of channels to %i\n",af->data->nch);
    return AF_OK;
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void uninit(struct af_instance_s* af)
{
  if(af->data)
    free(af->data);
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*   c = data;			// Current working data
  af_data_t*   l = af->data;	 		// Local data

  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  // Reset unused channels if nch in < nch out
  if(af->mul.n > af->mul.d)
    memset(l->audio,0,af_lencalc(af->mul, c->len));
  
  // Special case always output L & R
  if(c->nch == 1){
    copy(c->audio,l->audio,1,0,l->nch,0,c->len,c->bps);
    copy(c->audio,l->audio,1,0,l->nch,1,c->len,c->bps);
  }
  else{
    int i;
    if(l->nch < c->nch){ 
      for(i=0;i<l->nch;i++) // Truncates R if l->nch == 1 not good need mixing
	copy(c->audio,l->audio,c->nch,i,l->nch,i,c->len,c->bps);	
    }
    else{
      for(i=0;i<c->nch;i++)
	copy(c->audio,l->audio,c->nch,i,l->nch,i,c->len,c->bps);	
    }
  }
  
  // Set output data
  c->audio = l->audio;
  c->len   = af_lencalc(af->mul, c->len);
  c->nch   = l->nch;

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
  if(af->data == NULL)
    return AF_ERROR;
  return AF_OK;
}

// Description of this filter
af_info_t af_info_channels = {
  "Insert or remove channels",
  "channels",
  "Anders",
  "",
  open
};
