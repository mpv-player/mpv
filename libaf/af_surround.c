/*
 * Filter to do simple decoding of matrixed surround sound.
 * This will provide a (basic) surround-sound effect from
 * audio encoded for Dolby Surround, Pro Logic etc.
 *
 * original author: Steve Davies <steve@daviesfam.org>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* The principle:  Make rear channels by extracting anti-phase data
   from the front channels, delay by 20ms and feed to rear in anti-phase
*/


/* SPLITREAR: Define to decode two distinct rear channels - this
     doesn't work so well in practice because separation in a passive
     matrix is not high. C (dialogue) to Ls and Rs 14dB or so - so
     dialogue leaks to the rear.  Still - give it a try and send
     feedback. Comment this define for old behavior of a single
     surround sent to rear in anti-phase */
#define SPLITREAR 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "af.h"
#include "dsp.h"

#define L  32    // Length of fir filter
#define LD 65536 // Length of delay buffer

// 32 Tap fir filter loop unrolled
#define FIR(x,w,y) \
  y = ( w[0] *x[0] +w[1] *x[1] +w[2] *x[2] +w[3] *x[3]  \
      + w[4] *x[4] +w[5] *x[5] +w[6] *x[6] +w[7] *x[7]  \
      + w[8] *x[8] +w[9] *x[9] +w[10]*x[10]+w[11]*x[11] \
      + w[12]*x[12]+w[13]*x[13]+w[14]*x[14]+w[15]*x[15] \
      + w[16]*x[16]+w[17]*x[17]+w[18]*x[18]+w[19]*x[19] \
      + w[20]*x[20]+w[21]*x[21]+w[22]*x[22]+w[23]*x[23] \
      + w[24]*x[24]+w[25]*x[25]+w[26]*x[26]+w[27]*x[27] \
      + w[28]*x[28]+w[29]*x[29]+w[30]*x[30]+w[31]*x[31])

// Add to circular queue macro + update index
#ifdef SPLITREAR
#define ADDQUE(qi,rq,lq,r,l)\
  lq[qi]=lq[qi+L]=(l);\
  rq[qi]=rq[qi+L]=(r);\
  qi=(qi-1)&(L-1);
#else
#define ADDQUE(qi,lq,l)\
  lq[qi]=lq[qi+L]=(l);\
  qi=(qi-1)&(L-1);
#endif

// Macro for updating queue index in delay queues
#define UPDATEQI(qi) qi=(qi+1)&(LD-1)

// instance data
typedef struct af_surround_s
{
  float lq[2*L]; // Circular queue for filtering left rear channel
  float rq[2*L]; // Circular queue for filtering right rear channel
  float w[L]; 	 // FIR filter coefficients for surround sound 7kHz low-pass
  float* dr;	 // Delay queue right rear channel
  float* dl;	 // Delay queue left rear channel
  float  d;	 // Delay time
  int i;       	 // Position in circular buffer
  int wi;	 // Write index for delay queue
  int ri;	 // Read index for delay queue
}af_surround_t;

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_surround_t *s = af->setup;
  switch(cmd){
  case AF_CONTROL_REINIT:{
    float fc;
    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = ((af_data_t*)arg)->nch*2;
    af->data->format = AF_FORMAT_FLOAT_NE;
    af->data->bps    = 4;

    if (af->data->nch != 4){
      af_msg(AF_MSG_ERROR,"[surround] Only stereo input is supported.\n");
      return AF_DETACH;
    }
    // Surround filer coefficients
    fc = 2.0 * 7000.0/(float)af->data->rate;
    if (-1 == af_filter_design_fir(L, s->w, &fc, LP|HAMMING, 0)){
      af_msg(AF_MSG_ERROR,"[surround] Unable to design low-pass filter.\n");
      return AF_ERROR;
    }

    // Free previous delay queues
    if(s->dl)
      free(s->dl);
    if(s->dr)
      free(s->dr);
    // Allocate new delay queues
    s->dl = calloc(LD,af->data->bps);
    s->dr = calloc(LD,af->data->bps);
    if((NULL == s->dl) || (NULL == s->dr))
      af_msg(AF_MSG_FATAL,"[delay] Out of memory\n");
    
    // Initialize delay queue index
    if(AF_OK != af_from_ms(1, &s->d, &s->wi, af->data->rate, 0.0, 1000.0))
      return AF_ERROR;
//    printf("%i\n",s->wi);
    s->ri = 0;

    if((af->data->format != ((af_data_t*)arg)->format) || 
       (af->data->bps    != ((af_data_t*)arg)->bps)){
      ((af_data_t*)arg)->format = af->data->format;
      ((af_data_t*)arg)->bps = af->data->bps;
      return AF_FALSE;
    }
    return AF_OK;
  }
  case AF_CONTROL_COMMAND_LINE:{
    float d = 0;
    sscanf((char*)arg,"%f",&d);
    if ((d < 0) || (d > 1000)){
      af_msg(AF_MSG_ERROR,"[surround] Invalid delay time, valid time values"
	     " are 0ms to 1000ms current value is %0.3f ms\n",d);
      return AF_ERROR;
    }
    s->d = d;
    return AF_OK;
  }
  }
  return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance_s* af)
{
  if(af->data)
    free(af->data->audio);
  free(af->data);
  free(af->setup);
}

// The beginnings of an active matrix...
static float steering_matrix[][12] = {
//	LL	RL	LR	RR	LS	RS
//	LLs	RLs	LRs	RRs	LC	RC
       {.707,	.0,	.0,	.707,	.5,	-.5,
	.5878,	-.3928,	.3928,	-.5878,	.5,	.5},
};

// Experimental moving average dominance
//static int amp_L = 0, amp_R = 0, amp_C = 0, amp_S = 0;

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data){
  af_surround_t* s   = (af_surround_t*)af->setup;
  float*	 m   = steering_matrix[0]; 
  float*     	 in  = data->audio; 	// Input audio data
  float*     	 out = NULL;		// Output audio data
  float*	 end = in + data->len / sizeof(float); // Loop end
  int 		 i   = s->i;	// Filter queue index
  int 		 ri  = s->ri;	// Read index for delay queue
  int 		 wi  = s->wi;	// Write index for delay queue

  if (AF_OK != RESIZE_LOCAL_BUFFER(af, data))
    return NULL;

  out = af->data->audio;

  while(in < end){
    /* Dominance:
       abs(in[0])  abs(in[1]);
       abs(in[0]+in[1])  abs(in[0]-in[1]);
       10 * log( abs(in[0]) / (abs(in[1])|1) );
       10 * log( abs(in[0]+in[1]) / (abs(in[0]-in[1])|1) ); */

    /* About volume balancing...
       Surround encoding does the following:
           Lt=L+.707*C+.707*S, Rt=R+.707*C-.707*S
       So S should be extracted as:
           (Lt-Rt)
       But we are splitting the S to two output channels, so we
       must take 3dB off as we split it:
           Ls=Rs=.707*(Lt-Rt)
       Trouble is, Lt could be +1, Rt -1, so possibility that S will
       overflow. So to avoid that, we cut L/R by 3dB (*.707), and S by
       6dB (/2). This keeps the overall balance, but guarantees no
       overflow. */

    // Output front left and right
    out[0] = m[0]*in[0] + m[1]*in[1];
    out[1] = m[2]*in[0] + m[3]*in[1];

    // Low-pass output @ 7kHz
    FIR((&s->lq[i]), s->w, s->dl[wi]);

    // Delay output by d ms
    out[2] = s->dl[ri];

#ifdef SPLITREAR
    // Low-pass output @ 7kHz
    FIR((&s->rq[i]), s->w, s->dr[wi]);

    // Delay output by d ms
    out[3] = s->dr[ri];
#else
    out[3] = -out[2];
#endif

    // Update delay queues indexes
    UPDATEQI(ri);
    UPDATEQI(wi);

    // Calculate and save surround in circular queue
#ifdef SPLITREAR
    ADDQUE(i, s->rq, s->lq, m[6]*in[0]+m[7]*in[1], m[8]*in[0]+m[9]*in[1]);
#else
    ADDQUE(i, s->lq, m[4]*in[0]+m[5]*in[1]);
#endif

    // Next sample...
    in = &in[data->nch];  
    out = &out[af->data->nch];
  }
  
  // Save indexes
  s->i  = i; s->ri = ri; s->wi = wi;

  // Set output data
  data->audio = af->data->audio;
  data->len   *= 2;
  data->nch   = af->data->nch;

  return data;
}

static int af_open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul=2;
  af->data=calloc(1,sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_surround_t));
  if(af->data == NULL || af->setup == NULL)
    return AF_ERROR;
  ((af_surround_t*)af->setup)->d = 20;
  return AF_OK;
}

af_info_t af_info_surround =
{
        "Surround decoder filter",
        "surround",
        "Steve Davies <steve@daviesfam.org>",
        "",
        AF_FLAGS_NOT_REENTRANT,
        af_open
};
