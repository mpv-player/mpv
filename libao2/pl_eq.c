/*=============================================================================
//	
//  This software has been released under the terms of the GNU Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2001 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* Equalizer plugin, implementation of a 10 band time domain graphic
   equalizer using IIR filters. The IIR filters are implemented using a
   Direct Form II approach. But has been modified (b1 == 0 always) to
   save computation.
*/
#define PLUGIN

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <math.h>

#include "audio_out.h"
#include "audio_plugin.h"
#include "audio_plugin_internal.h"
#include "afmt.h"
#include "eq.h"

static ao_info_t info =
{
        "Equalizer audio plugin",
        "eq",
        "Anders",
        ""
};

LIBAO_PLUGIN_EXTERN(eq)


#define CH  6     // Max number of channels
#define L   2     // Storage for filter taps
#define KM  10    // Max number of octaves

#define Q   1.2247 /* Q value for band-pass filters 1.2247=(3/2)^(1/2)
		      gives 4db suppression @ Fc*2 and Fc/2 */

// Center frequencies for band-pass filters
#define CF  {31.25,62.5,125,250,500,1000,2000,4000,8000,16000}

// local data
typedef struct pl_eq_s
{
  int16_t   a[KM][L];        // A weights
  int16_t   b[KM][L];        // B weights
  int16_t   wq[CH][KM][L];   // Circular buffer for W data
  int16_t   g[CH][KM];       // Gain factor for each channel and band
  int16_t   K; 		     // Number of used eq bands
  int       channels;        // Number of channels
} pl_eq_t;

static pl_eq_t pl_eq;

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    return CONTROL_OK;
  case AOCONTROL_PLUGIN_EQ_SET_GAIN:{
    float gain = ((equalizer_t*)arg)->gain;
    int ch     =((equalizer_t*)arg)->channel;
    int band   =((equalizer_t*)arg)->band;
    if(ch > CH || ch < 0 || band > KM || band < 0)
      return CONTROL_ERROR;
    
    pl_eq.g[ch][band]=(int16_t) 4096 * (pow(10.0,gain/20.0)-1.0);
    return CONTROL_OK;
  }
  case AOCONTROL_PLUGIN_EQ_GET_GAIN:{
    int ch     =((equalizer_t*)arg)->channel;
    int band   =((equalizer_t*)arg)->band;
    if(ch > CH || ch < 0 || band > KM || band < 0)
      return CONTROL_ERROR;
    
    ((equalizer_t*)arg)->gain = log10((float)pl_eq.g[ch][band]/4096.0+1) * 20.0;
    return CONTROL_OK;
  }
  }
  return CONTROL_UNKNOWN;
}

// 2nd order Band-pass Filter design
void bp2(int16_t* a, int16_t* b, float fc, float q){
  double th=2*3.141592654*fc;
  double C=(1 - tan(th*q/2))/(1 + tan(th*q/2));
  
  a[0] = (int16_t)( 16383.0 * (1 + C) * cos(th) + 0.5);
  a[1] = (int16_t)(-16383.0 * C + 0.5);
  
  b[0] = (int16_t)(-16383.0 * (C - 1)/2 + 0.5);
  b[1] = (int16_t)(-16383.0 * 1.0050 + 0.5);
}

// empty buffers
static void reset(){
  int k,l,c;
  for(c=0;c<pl_eq.channels;c++)
    for(k=0;k<pl_eq.K;k++)
      for(l=0;l<L*2;l++)
	pl_eq.wq[c][k][l]=0;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(){
  int   c,k   = 0;
  float F[KM] = CF;
  
  // Check input format
  if(ao_plugin_data.format != AFMT_S16_LE){
    fprintf(stderr,"[pl_eq] Input audio format not yet supported. \n");
    return 0;
  }

  // Check number of channels
  if(ao_plugin_data.channels>CH){
     fprintf(stderr,"[pl_eq] Too many channels, max is 6.\n");
    return 0;
  }
  pl_eq.channels=ao_plugin_data.channels;

  // Calculate number of active filters
  pl_eq.K=KM;
  while(F[pl_eq.K-1] > (float)ao_plugin_data.rate/2)
    pl_eq.K--;

  // Generate filter taps
  for(k=0;k<pl_eq.K;k++)
    bp2(pl_eq.a[k],pl_eq.b[k],F[k]/((float)ao_plugin_data.rate),Q);

  // Reset buffers
  reset();

  // Reset gain factors
  for(c=0;c<pl_eq.channels;c++)
    for(k=0;k<pl_eq.K;k++)
      pl_eq.g[c][k]=0;

  // Tell ao_plugin how much this plugin adds to the overall time delay
  ao_plugin_data.delay_fix-=2/((float)pl_eq.channels*(float)ao_plugin_data.rate);
  // Print some cool remark of what the plugin does
  printf("[pl_eq] Equalizer in use.\n");
  return 1;
}

// close plugin
static void uninit(){
}

// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
static int play(){
  uint16_t  	ci    	= pl_eq.channels; 	// Index for channels
  uint16_t	nch   	= pl_eq.channels;   	// Number of channels

  while(ci--){
    int16_t*	g	= pl_eq.g[ci]; 	// Gain factor 
    int16_t*	in    	= ((int16_t*)ao_plugin_data.data)+ci;
    int16_t*	out   	= ((int16_t*)ao_plugin_data.data)+ci;
    int16_t* 	end   	= in+ao_plugin_data.len/2; // Block loop end

    while(in < end){
      register int16_t k   = 0;	   	// Frequency band index
      register int32_t yt  = 0;    	// Total output from filters
      register int16_t x   = *in; 	/* Current input sample scale
					   to prevent overflow in wq */
      in+=nch;

      // Run the filters 
      for(;k<pl_eq.K;k++){
	// Pointer to circular buffer wq
	register int16_t* wq = pl_eq.wq[ci][k];
	// Calculate output from AR part of current filter
	register int32_t xt = (x*pl_eq.b[k][0]) >> 4;
	register int32_t w = xt + wq[0]*pl_eq.a[k][0] + wq[1]*pl_eq.a[k][1];
	// Calculate output form MA part of current filter
	yt+=(((w + wq[1]*pl_eq.b[k][1]) >> 10)*g[k]) >> 12;
	// Update circular buffer
	wq[1] = wq[0]; wq[0] = w >> 14;
      }	

      // Calculate output 
      *out=(int16_t)(yt+x);
      out+=nch;
    }
  }
  return 1;
}







