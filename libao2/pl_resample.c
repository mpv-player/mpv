/*=============================================================================
//	
//  This software has been released under the terms of the GNU Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2001 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* This audio output plugin changes the sample rate. The output
   samplerate from this plugin is specified by using the switch
   `fout=F' where F is the desired output sample frequency 
*/

#define PLUGIN

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "audio_out.h"
#include "audio_plugin.h"
#include "audio_plugin_internal.h"
#include "afmt.h"
#include "../config.h"

static ao_info_t info =
{
        "Sample frequency conversion audio plugin",
        "resample",
        "Anders",
        ""
};

LIBAO_PLUGIN_EXTERN(resample)

#define min(a,b)   (((a) < (b)) ? (a) : (b))
#define max(a,b)   (((a) > (b)) ? (a) : (b))

/* Below definition selects the length of each poly phase component.
   Valid definitions are L8 and L16, where the number denotes the
   length of the filter. This definition affects the computational
   complexity (see play()), the performance (see filter.h) and the
   memory usage. The filterlenght is choosen to 8 if the machine is
   slow and to 16 if the machine is fast and has MMX.  
*/

#if !defined(HAVE_SSE) && !defined(HAVE_3DNOW) //This machine is slow

#define W 	W8	// Filter bank parameters
#define L   	8	// Filter length
#ifdef HAVE_MMX
#define FIR(x,w,y) *y=(int16_t)firn(x,w,8);
#else /* HAVE_MMX */
// Unrolled loop to speed up execution 
#define FIR(x,w,y){ \
  int16_t a = (w[0]*x[0]+w[1]*x[1]+w[2]*x[2]+w[3]*x[3]) >> 16; \
  int16_t b = (w[4]*x[4]+w[5]*x[5]+w[6]*x[6]+w[7]*x[7]) >> 16; \
  y[0]      = a+b; \
}
#endif /* HAVE_MMX */

#else  /* Fast machine */

#define W 	W16
#define L   	16
#define FIR(x,w,y) *y=(int16_t)firn(x,w,16);

#endif

#define CH  6	// Max number of channels
#define UP  128  /* Up sampling factor. Increasing this value will
                    improve frequency accuracy. Think about the L1
                    cashing of filter parameters - how big can it be? */

#include "fir.h"
#include "filter.h"

// local data
typedef struct pl_resample_s
{
  int16_t*	data;		// Data buffer
  int16_t*  	w;		// Current filter weights
  uint16_t  	dn;     	// Down sampling factor
  uint16_t	up;		// Up sampling factor 
  int 		channels;	// Number of channels
  int 		len;		// Lenght of buffer
  int16_t	ws[UP*L];	// List of all available filters	
  int16_t 	xs[CH][L*2]; 	// Circular buffers
} pl_resample_t;

static pl_resample_t 	pl_resample	= {NULL,NULL,1,1,1,0,W};

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    if(pl_resample.data) 
      free(pl_resample.data);
    pl_resample.len = ao_plugin_data.len;
    pl_resample.data=(int16_t*)malloc(pl_resample.len);
    if(!pl_resample.data)
      return CONTROL_ERROR;
    ao_plugin_data.len = (int)((double)ao_plugin_data.len * 
			     ((double)pl_resample.dn)/
			     ((double)pl_resample.up));
    return CONTROL_OK;
  }
  return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(){
  int fin=ao_plugin_data.rate;
  int fout=ao_plugin_cfg.pl_resample_fout;
  pl_resample.w=pl_resample.ws;
  pl_resample.up=UP;

  // Sheck input format
  if(ao_plugin_data.format != AFMT_S16_LE){
    fprintf(stderr,"[pl_resample] Input audio format not yet suported. \n");
    return 0;
  }
  // Sanity check and calculate down sampling factor
  if((float)max(fin,fout)/(float)min(fin,fout) > 10){
    fprintf(stderr,"[pl_resample] The difference between fin and fout is too large.\n");
    return 0;
  }
  pl_resample.dn=(int)(0.5+((float)(fin*pl_resample.up))/((float)fout));

  pl_resample.channels=ao_plugin_data.channels;
  if(ao_plugin_data.channels>CH){
     fprintf(stderr,"[pl_resample] Too many channels, max is 6.\n");
    return 0;
  }

  // Tell the world what we are up to
  printf("[pl_resample] Up=%i, Down=%i, True fout=%f\n",
	 pl_resample.up,pl_resample.dn,
	 ((float)fin*pl_resample.up)/((float)pl_resample.dn));

  // This plugin changes buffersize and adds some delay
  ao_plugin_data.sz_mult/=((float)pl_resample.up)/((float)pl_resample.dn);
  ao_plugin_data.delay_fix-= ((float)L/2) * (1/fout);
  ao_plugin_data.rate=fout;
  return 1;
}

// close plugin
static void uninit(){
  if(pl_resample.data) 
    free(pl_resample.data);
  pl_resample.data=NULL;
}

// empty buffers
static void reset(){
}

// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
// FIXME: this routine needs to be optimized (it is probably possible to do a lot here)
static int play(){
  if(pl_resample.up==pl_resample.dn){
    register int16_t*	in    = ((int16_t*)ao_plugin_data.data);
    register int16_t* 	end   = in+ao_plugin_data.len/2;
    while(in < end) *in=(*in++)>>1;
    return 1;
  }
  if(pl_resample.up>pl_resample.dn)
    return upsample();
//  if(pl_resample.up<pl_resample.dn)
    return downsample();
}

int upsample(){
  static uint16_t	pwi = 0; // Index for w
  static uint16_t	pxi = 0; // Index for circular queue

  uint16_t		ci    = pl_resample.channels; 	// Index for channels
  uint16_t		nch   = pl_resample.channels;   // Number of channels
  uint16_t		len   = 0; 			// Number of input samples
  uint16_t		inc   = pl_resample.up/pl_resample.dn; 
  uint16_t		level = pl_resample.up%pl_resample.dn; 
  uint16_t		up    = pl_resample.up;
  uint16_t		dn    = pl_resample.dn;

  register int16_t*	w     = pl_resample.w;
  register uint16_t	wi,xi; // Temporary indexes

  // Index current channel
  while(ci--){
    // Temporary pointers
    register int16_t*	x     = pl_resample.xs[ci];
    register int16_t*	in    = ((int16_t*)ao_plugin_data.data)+ci;
    register int16_t*	out   = pl_resample.data+ci;
    int16_t* 		end   = in+ao_plugin_data.len/2; // Block loop end

    wi = pwi; xi = pxi;

    while(in < end){
      register uint16_t	i = inc;
      if(wi<level) i++;

      xi=updateq(x,in,xi,L);
      in+=nch;
      while(i--){
	// Run the FIR filter
	FIR((&x[xi]),(&w[wi*L]),out);
	len++; out+=nch;
	// Update wi to point at the correct polyphase component
	wi=(wi+dn)%up;
      }
    }
  }

  // Save values that needs to be kept for next time
  pwi = wi;
  pxi = xi;

  // Set new data
  ao_plugin_data.len=len*2;
  ao_plugin_data.data=pl_resample.data;
  return 1;
}

int downsample(){
  static uint16_t	pwi = 0; // Index for w
  static uint16_t	pxi = 0; // Index for circular queue
  static uint16_t	pi =  1; // Number of new samples to put in x queue

  uint16_t		ci    = pl_resample.channels; 	// Index for channels
  uint16_t		len   = 0; 			// Number of input samples
  uint16_t		nch   = pl_resample.channels;   // Number of channels
  uint16_t		inc   = pl_resample.dn/pl_resample.up; 
  uint16_t		level = pl_resample.dn%pl_resample.up; 
  uint16_t		up    = pl_resample.up;
  uint16_t		dn    = pl_resample.dn;

  register uint16_t	i,wi,xi; // Temporary indexes

  
  // Index current channel
  while(ci--){
    // Temporary pointers
    register int16_t*	x     = pl_resample.xs[ci];
    register int16_t*	in    = ((int16_t*)ao_plugin_data.data)+ci;
    register int16_t*	out   = pl_resample.data+ci;
    // Block loop end
    register int16_t* 	end   = in+ao_plugin_data.len/2;
    i = pi; wi = pwi; xi = pxi;

    while(in < end){

      xi=updateq(x,in,xi,L);
      in+=nch;
      if(!--i){
	// Run the FIR filter
	FIR((&x[xi]),(&pl_resample.w[wi*L]),out);
	len++;	out+=nch;

	// Update wi to point at the correct polyphase component
	wi=(wi+dn)%up;  

	// Insert i number of new samples in queue
	i = inc;
	if(wi<level) i++;
      }
    }
  }
  // Save values that needs to be kept for next time
  pwi = wi;
  pxi = xi;
  pi = i;
  // Set new data
  ao_plugin_data.len=len*2;
  ao_plugin_data.data=pl_resample.data;
  return 1;
}
