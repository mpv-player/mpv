/*=============================================================================
//	
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2002 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "af.h"

// Data for specific instances of this filter
typedef struct af_comp_s
{
  int   enable[AF_NCH];		// Enable/disable / channel
  float time[AF_NCH];		// Forgetting factor for power estimate
  float	pow[AF_NCH];		// Estimated power level [dB]
  float	tresh[AF_NCH];		// Threshold [dB]
  int	attack[AF_NCH];		// Attack time [ms]
  int	release[AF_NCH];	// Release time [ms]
  float	ratio[AF_NCH];		// Compression ratio
}af_comp_t;

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_comp_t* s   = (af_comp_t*)af->setup; 
  int i;

  switch(cmd){
  case AF_CONTROL_REINIT:
    // Sanity check
    if(!arg) return AF_ERROR;
    
    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = ((af_data_t*)arg)->nch;
    af->data->format = AF_FORMAT_FLOAT_NE;
    af->data->bps    = 4;

    // Time constant set to 0.1s
    //    s->alpha = (1.0/0.2)/(2.0*M_PI*(float)((af_data_t*)arg)->rate); 
    return af_test_output(af,(af_data_t*)arg);
  case AF_CONTROL_COMMAND_LINE:{
/*     float v=-10.0; */
/*     float vol[AF_NCH]; */
/*     float s=0.0; */
/*     float clipp[AF_NCH]; */
/*     int i; */
/*     sscanf((char*)arg,"%f:%f", &v, &s); */
/*     for(i=0;i<AF_NCH;i++){ */
/*       vol[i]=v; */
/*       clipp[i]=s; */
/*     } */
/*     if(AF_OK != control(af,AF_CONTROL_VOLUME_SOFTCLIP | AF_CONTROL_SET, clipp)) */
/*       return AF_ERROR; */
/*     return control(af,AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET, vol); */
  }
  case AF_CONTROL_COMP_ON_OFF | AF_CONTROL_SET:
    memcpy(s->enable,(int*)arg,AF_NCH*sizeof(int));
    return AF_OK; 
  case AF_CONTROL_COMP_ON_OFF | AF_CONTROL_GET:
    memcpy((int*)arg,s->enable,AF_NCH*sizeof(int));
    return AF_OK; 
  case AF_CONTROL_COMP_THRESH | AF_CONTROL_SET:
    return af_from_dB(AF_NCH,(float*)arg,s->tresh,20.0,-60.0,-1.0);
  case AF_CONTROL_COMP_THRESH | AF_CONTROL_GET:
    return af_to_dB(AF_NCH,s->tresh,(float*)arg,10.0);
  case AF_CONTROL_COMP_ATTACK | AF_CONTROL_SET:
    return af_from_ms(AF_NCH,(float*)arg,s->attack,af->data->rate,500.0,0.1);
  case AF_CONTROL_COMP_ATTACK | AF_CONTROL_GET:
    return af_to_ms(AF_NCH,s->attack,(float*)arg,af->data->rate);
  case AF_CONTROL_COMP_RELEASE | AF_CONTROL_SET:
    return af_from_ms(AF_NCH,(float*)arg,s->release,af->data->rate,3000.0,10.0);
  case AF_CONTROL_COMP_RELEASE | AF_CONTROL_GET:
    return af_to_ms(AF_NCH,s->release,(float*)arg,af->data->rate);
  case AF_CONTROL_COMP_RATIO | AF_CONTROL_SET:
    for(i=0;i<AF_NCH;i++) 
      s->ratio[i] = clamp(((float*)arg)[i],1.0,10.0);
    return AF_OK;
  case AF_CONTROL_COMP_RATIO | AF_CONTROL_GET:
    for(i=0;i<AF_NCH;i++) 
      ((float*)arg)[i] = s->ratio[i];
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
  af_data_t*    c   = data;			// Current working data
  af_comp_t*  	s   = (af_comp_t*)af->setup; 	// Setup for this instance
  float*   	a   = (float*)c->audio;		// Audio data
  int       	len = c->len/4;			// Number of samples
  int           ch  = 0;			// Channel counter
  register int	nch = c->nch;			// Number of channels	
  register int  i   = 0;

  // Compress/expand
  for(ch = 0; ch < nch ; ch++){
    if(s->enable[ch]){
      float	t   = 1.0 - s->time[ch];
      for(i=ch;i<len;i+=nch){
	register float x 	= a[i];
	register float pow 	= x*x;	
	s->pow[ch] = t*s->pow[ch] + 
	  pow*s->time[ch]; // LP filter
	if(pow < s->pow[ch]){
	  ;
	}
	else{
	  ;
	}
	a[i] = x;
      }
    }
  }
  return c;
}

// Allocate memory and set function pointers
static int af_open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_comp_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  return AF_OK;
}

// Description of this filter
af_info_t af_info_comp = {
    "Compressor/expander audio filter",
    "comp",
    "Anders",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
