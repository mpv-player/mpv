/* This audio filter changes the volume of the sound, and can be used
   when the mixer doesn't support the PCM channel. It can handel
   between 1 and 6 channels. The volume can be adjusted between -60dB
   to +10dB and is set on a per channels basis. The volume can be
   written ad read by AF_CONTROL_VOLUME_SET and AF_CONTROL_VOLUME_GET
   respectivly.

   The plugin has support for softclipping, it is enabled by
   AF_CONTROL_VOLUME_SOFTCLIPP. It has also a probing feature which
   can be used to measure the power in the audio stream, both an
   instantanious value and the maximum value can be probed. The
   probing is enable by AF_CONTROL_VOLUME_PROBE_ON_OFF and is done on a
   per channel basis. The result from the probing is obtained using
   AF_CONTROL_VOLUME_PROBE_GET and AF_CONTROL_VOLUME_PROBE_GET_MAX. The
   probed values are calculated in dB. The control of the volume can
   be turned off by the AF_CONTROL_VOLUME_ON_OFF switch.
*/

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <inttypes.h>
#include <math.h>

#include "../config.h"
#include "../mp_msg.h"

#include "af.h"

// Some limits
#define MIN_S16 -32650
#define MAX_S16  32650

#define MAX_VOL +10.0
#define MIN_VOL	-60.0

// Number of channels
#define NCH 6

#include "../libao2/afmt.h"


// Data for specific instances of this filter
typedef struct af_volume_s
{
  double volume[NCH];	// Volume for each channel
  double power[NCH];	// Instantaneous power in each channel
  double maxpower[NCH];	// Maximum power in each channel
  double alpha;		// Forgetting factor for power estimate
  int softclip;		// Soft clippng on/off
  int probe;		// Probing on/off
  int onoff;		// Volume control on/off
}af_volume_t;

/* Convert to gain value from dB. Returns AF_OK if of and AF_ERROR if
   fail */
inline int from_dB(double* in, double* out) 
{
  int i = 0; 
  // Sanity check
  if(!in || !out) 
    return AF_ERROR;

  for(i=0;i<NCH;i++) 
    out[i]=pow(10.0,clamp(in[i],MIN_VOL,MAX_VOL)/10.0);
  return AF_OK;
}

/* Convert from gain value to dB. Returns AF_OK if of and AF_ERROR if
   fail */
inline int to_dB(double* in, double* out) 
{
  int i = 0; 
  // Sanity check
  if(!in || !out) 
    return AF_ERROR;

  for(i=0;i<NCH;i++) 
    out[i]=10.0*log10(clamp(in[i],MIN_VOL,MAX_VOL));
  return AF_OK;
}

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_volume_t* s   = (af_volume_t*)af->setup; 

  switch(cmd){
  case AF_CONTROL_REINIT:
    // Sanity check
    if(!arg) return AF_ERROR;
    
    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = ((af_data_t*)arg)->nch;
    af->data->format = AFMT_S16_LE;
    af->data->bps    = 2;
    
    // Time constant set to 0.1s
    s->alpha = (1.0/0.2)/(2.0*M_PI*(double)((af_data_t*)arg)->rate); 

    // Only AFMT_S16_LE is supported for now
    if(af->data->format != ((af_data_t*)arg)->format || 
       af->data->bps != ((af_data_t*)arg)->bps)
      return AF_FALSE;
    return AF_OK;
  case AF_CONTROL_VOLUME_SET:
    return from_dB((double*)arg,s->volume);
  case AF_CONTROL_VOLUME_GET:
    return to_dB(s->volume,(double*)arg);
  case AF_CONTROL_VOLUME_PROBE_GET:
    return to_dB(s->power,(double*)arg);
  case AF_CONTROL_VOLUME_PROBE_GET_MAX:
    return to_dB(s->maxpower,(double*)arg);
  case AF_CONTROL_VOLUME_SOFTCLIP:
    s->softclip = (int)arg;
    return AF_OK;
  case AF_CONTROL_VOLUME_PROBE_ON_OFF:
    s->probe = (int)arg;
    return AF_OK;
  case AF_CONTROL_VOLUME_ON_OFF:
    s->onoff = (int)arg;
    return AF_OK;
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void uninit(struct af_instance_s* af)
{
  if(af->data)
    free(af->data);
  if(af->setup)
    free(af->setup);
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*     c = data;			// Current working data
  af_volume_t*   s = (af_volume_t*)af->setup; 	// Setup for this instance
  int16_t*       a = (int16_t*)c->audio;	// Audio data
  int          len = c->len/2;			// Number of samples
  int           ch = 0;				// Channel counter
  register int nch = c->nch;			// Number of channels	
  register int   i = 0;
  
  // Probe the data stream 
  if(s->probe){
    for(ch = 0; ch < nch ; ch++){
      double alpha  = s->alpha;
      double beta   = 1 - alpha;
      double pow    = s->power[ch];
      double maxpow = s->maxpower[ch];
      register double t = 0;
      for(i=ch;i<len;i+=nch){
	t = ((double)a[i])/32768.0;
	t *= t;
	// Check maximum power value
	if(t>maxpow) 
	  maxpow=t;
	// Power estimate made using first order AR model
	if(t>pow)
	  pow=t;
	else
	  pow = beta*pow+alpha*t;
      }
      s->power[ch]    = pow;
      s->maxpower[ch] = maxpow;
    }
  }

  // Change the volume.
  if(s->onoff){
    register int sc  = s->softclip;
    for(ch = 0; ch < nch ; ch++){
      register int vol = (int)(255.0 * s->volume[ch]); 
      for(i=ch;i<len;i+=nch){
	register int x;
	x=(a[i] * vol) >> 8;
	if(sc){
	  int64_t t=x*x;
	  t=(t*x) >> 30;
	  x = (3*x - (int)t) >> 1;
	}
	a[i]=clamp(x,MIN_S16,MAX_S16);
      }
    }
  }

  return c;
}

// Allocate memory and set function pointers
static int open(af_instance_t* af){
  int i = 0;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_volume_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  // Enable volume control and set initial volume to 0.1
  ((af_volume_t*)af->setup)->onoff = 1;
  for(i=0;i<NCH;i++)
    ((af_volume_t*)af->setup)->volume[i]=1.0; //0.1;

  return AF_OK;
}

// Description of this filter
af_info_t af_info_volume = {
    "Volume control audio filter",
    "volume",
    "Anders",
    "",
    AF_FLAGS_NOT_REENTRANT,
    open
};
