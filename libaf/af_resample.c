/*=============================================================================
//	
//  This software has been released under the terms of the GNU Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2002 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* This audio filter changes the sample rate. */

#define PLUGIN

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"
#include "../libao2/afmt.h"

#include "af.h"
#include "dsp.h"

/* Below definition selects the length of each poly phase component.
   Valid definitions are L8 and L16, where the number denotes the
   length of the filter. This definition affects the computational
   complexity (see play()), the performance (see filter.h) and the
   memory usage. The filterlenght is choosen to 8 if the machine is
   slow and to 16 if the machine is fast and has MMX.  
*/

#if !defined(HAVE_SSE) && !defined(HAVE_3DNOW) // This machine is slow

#define L   	8	// Filter length
// Unrolled loop to speed up execution 
#define FIR(x,w,y) \
  (y[0])  = ( w[0]*x[0]+w[1]*x[1]+w[2]*x[2]+w[3]*x[3] \
            + w[4]*x[4]+w[5]*x[5]+w[6]*x[6]+w[7]*x[7] ) >> 16

#else  /* Fast machine */

#define L   	16
// Unrolled loop to speed up execution 
#define FIR(x,w,y) \
  y[0] = ( w[0] *x[0] +w[1] *x[1] +w[2] *x[2] +w[3] *x[3] \
         + w[4] *x[4] +w[5] *x[5] +w[6] *x[6] +w[7] *x[7] \
         + w[8] *x[8] +w[9] *x[9] +w[10]*x[10]+w[11]*x[11] \
         + w[12]*x[12]+w[13]*x[13]+w[14]*x[14]+w[15]*x[15] ) >> 16

#endif /* Fast machine */

// Macro to add data to circular que 
#define ADDQUE(xi,xq,in)\
  xq[xi]=xq[xi+L]=(*in);\
  xi=(--xi)&(L-1);



// local data
typedef struct af_resample_s
{
  int16_t*  	w;	// Current filter weights
  int16_t** 	xq; 	// Circular buffers
  uint32_t	xi; 	// Index for circular buffers
  uint32_t	wi;	// Index for w
  uint32_t	i; 	// Number of new samples to put in x queue
  uint32_t  	dn;     // Down sampling factor
  uint32_t	up;	// Up sampling factor 
  int		sloppy;	// Enable sloppy resampling to reduce memory usage
  int		fast;	// Enable linear interpolation instead of filtering
} af_resample_t;

// Euclids algorithm for calculating Greatest Common Divisor GCD(a,b)
static inline int gcd(register int a, register int b)
{
  register int r = min(a,b);
  a=max(a,b);
  b=r;

  r=a%b;
  while(r!=0){
    a=b;
    b=r;
    r=a%b;
  }
  return b;
}

static int upsample(af_data_t* c,af_data_t* l, af_resample_t* s)
{
  uint32_t		ci    = l->nch; 	// Index for channels
  uint32_t		len   = 0; 		// Number of input samples
  uint32_t		nch   = l->nch;   	// Number of channels
  uint32_t		inc   = s->up/s->dn; 
  uint32_t		level = s->up%s->dn; 
  uint32_t		up    = s->up;
  uint32_t		dn    = s->dn;

  register int16_t*	w     = s->w;
  register uint32_t	wi    = 0;
  register uint32_t	xi    = 0; 

  // Index current channel
  while(ci--){
    // Temporary pointers
    register int16_t*	x     = s->xq[ci];
    register int16_t*	in    = ((int16_t*)c->audio)+ci;
    register int16_t*	out   = ((int16_t*)l->audio)+ci;
    int16_t* 		end   = in+c->len/2; // Block loop end
    wi = s->wi; xi = s->xi;

    while(in < end){
      register uint32_t	i = inc;
      if(wi<level) i++;

      ADDQUE(xi,x,in);
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
  s->wi = wi;
  s->xi = xi;
  return len;
}

static int downsample(af_data_t* c,af_data_t* l, af_resample_t* s)
{
  uint32_t		ci    = l->nch; 	// Index for channels
  uint32_t		len   = 0; 		// Number of output samples
  uint32_t		nch   = l->nch;   	// Number of channels
  uint32_t		inc   = s->dn/s->up; 
  uint32_t		level = s->dn%s->up; 
  uint32_t		up    = s->up;
  uint32_t		dn    = s->dn;

  register int32_t	i     = 0;
  register uint32_t	wi    = 0;
  register uint32_t	xi    = 0;
  
  // Index current channel
  while(ci--){
    // Temporary pointers
    register int16_t*	x     = s->xq[ci];
    register int16_t*	in    = ((int16_t*)c->audio)+ci;
    register int16_t*	out   = ((int16_t*)l->audio)+ci;
    register int16_t* 	end   = in+c->len/2;    // Block loop end
    i = s->i; wi = s->wi; xi = s->xi;

    while(in < end){

      ADDQUE(xi,x,in);
      in+=nch;
      if((--i)<=0){
	// Run the FIR filter
	FIR((&x[xi]),(&s->w[wi*L]),out);
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
  s->wi = wi;
  s->xi = xi;
  s->i = i;

  return len;
}

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  switch(cmd){
  case AF_CONTROL_REINIT:{
    af_resample_t* s   = (af_resample_t*)af->setup; 
    af_data_t* 	   n   = (af_data_t*)arg; // New configureation
    int            i,d = 0;
    int 	   rv  = AF_OK;

    // Make sure this filter isn't redundant 
    if(af->data->rate == n->rate)
      return AF_DETACH;

    // Create space for circular bufers (if nesessary)
    if(af->data->nch != n->nch){
      // First free the old ones
      if(s->xq){
	for(i=1;i<af->data->nch;i++)
	  if(s->xq[i])
	    free(s->xq[i]);
	free(s->xq);
      }
      // ... then create new
      s->xq = malloc(n->nch*sizeof(int16_t*));
      for(i=0;i<n->nch;i++)
	s->xq[i] = malloc(2*L*sizeof(int16_t));
      s->xi = 0;
    }

    // Set parameters
    af->data->nch    = n->nch;
    af->data->format = AFMT_S16_NE;
    af->data->bps    = 2;
    if(af->data->format != n->format || af->data->bps != n->bps)
      rv = AF_FALSE;
    n->format = AFMT_S16_NE;
    n->bps = 2;

    // Calculate up and down sampling factors
    d=gcd(af->data->rate,n->rate);

    // If sloppy resampling is enabled limit the upsampling factor
    if(s->sloppy && (af->data->rate/d > 5000)){
      int up=af->data->rate/2;
      int dn=n->rate/2;
      int m=2;
      while(af->data->rate/(d*m) > 5000){
	d=gcd(up,dn); 
	up/=2; dn/=2; m*=2;
      }
      d*=m;
    }
    printf("\n%i %i %i\n",d,af->data->rate/d,n->rate/d);

    // Check if the the design needs to be redone
    if(s->up != af->data->rate/d || s->dn != n->rate/d){
      float* w;
      float* wt;
      float fc;
      int j;
      s->up = af->data->rate/d;	
      s->dn = n->rate/d;
      
      // Calculate cuttof frequency for filter
      fc = 1/(float)(max(s->up,s->dn));
      // Allocate space for polyphase filter bank and protptype filter
      w = malloc(sizeof(float) * s->up *L);
      if(NULL != s->w)
	free(s->w);
      s->w = malloc(L*s->up*sizeof(int16_t));

      // Design prototype filter type using Kaiser window with beta = 10
      if(NULL == w || NULL == s->w || 
	 -1 == design_fir(s->up*L, w, &fc, LP|KAISER , 10.0)){
	mp_msg(MSGT_AFILTER,MSGL_ERR,"[resample] Unable to design prototype filter.\n");
	return AF_ERROR;
      }
      // Copy data from prototype to polyphase filter
      wt=w;
      for(j=0;j<L;j++){//Columns
	for(i=0;i<s->up;i++){//Rows
	  float t=(float)s->up*32767.0*(*wt);
	  s->w[i*L+j] = (int16_t)((t>=0.0)?(t+0.5):(t-0.5));
	  wt++;
	}
      }
      free(w);
      mp_msg(MSGT_AFILTER,MSGL_V,"[resample] New filter designed up: %i down: %i\n", s->up, s->dn);
    }

    // Set multiplier and delay
    af->delay = (double)(1000*L/2)/((double)n->rate);
    af->mul.n = s->up;
    af->mul.d = s->dn;
    return rv;
  }
  case AF_CONTROL_COMMAND_LINE:{
    af_resample_t* s   = (af_resample_t*)af->setup; 
    int rate=0;
    sscanf((char*)arg,"%i:%i:%i",&rate,&(s->sloppy), &(s->fast));
    return af->control(af,AF_CONTROL_RESAMPLE,&rate);
  }
  case AF_CONTROL_RESAMPLE: 
    // Reinit must be called after this function has been called
    
    // Sanity check
    if(((int*)arg)[0] < 8000 || ((int*)arg)[0] > 192000){
      mp_msg(MSGT_AFILTER,MSGL_ERR,"[resample] The output sample frequency must be between 8kHz and 192kHz. Current value is %i \n",((int*)arg)[0]);
      return AF_ERROR;
    }

    af->data->rate=((int*)arg)[0]; 
    mp_msg(MSGT_AFILTER,MSGL_V,"[resample] Changing sample rate to %iHz\n",af->data->rate);
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
  int 		 len = 0; 	 // Length of output data
  af_data_t*     c   = data;	 // Current working data
  af_data_t*     l   = af->data; // Local data
  af_resample_t* s   = (af_resample_t*)af->setup;

  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  // Run resampling
  if(s->up>s->dn)
    len = upsample(c,l,s);
  else
    len = downsample(c,l,s);

  // Set output data
  c->audio = l->audio;
  c->len   = len*2;
  c->rate  = l->rate;
  
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
  af->setup=calloc(1,sizeof(af_resample_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  return AF_OK;
}

// Description of this plugin
af_info_t af_info_resample = {
  "Sample frequency conversion",
  "resample",
  "Anders",
  "",
  AF_FLAGS_REENTRANT,
  open
};

