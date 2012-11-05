/*
 * Copyright (C) 2002 Anders Johansson ajh@atri.curtin.edu.au
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

/* This file contains the resampling engine, the sample format is
   controlled by the FORMAT parameter, the filter length by the L
   parameter and the resampling type by UP and DN. This file should
   only be included by af_resample.c
*/

#undef L
#undef SHIFT
#undef FORMAT
#undef FIR
#undef ADDQUE

/* The length Lxx definition selects the length of each poly phase
   component. Valid definitions are L8 and L16 where the number
   defines the nuber of taps. This definition affects the
   computational complexity, the performance and the memory usage.
*/

/* The FORMAT_x parameter selects the sample format type currently
   float and int16 are supported. Thes two formats are selected by
   defining eiter FORMAT_F or FORMAT_I. The advantage of using float
   is that the amplitude and therefore the SNR isn't affected by the
   filtering, the disadvantage is that it is a lot slower.
*/

#if defined(FORMAT_I)
#define SHIFT >>16
#define FORMAT int16_t
#else
#define SHIFT
#define FORMAT float
#endif

// Short filter
#if defined(L8)

#define L   	8	// Filter length
// Unrolled loop to speed up execution
#define FIR(x,w,y) \
  (y[0])  = ( w[0]*x[0]+w[1]*x[1]+w[2]*x[2]+w[3]*x[3] \
            + w[4]*x[4]+w[5]*x[5]+w[6]*x[6]+w[7]*x[7] ) SHIFT



#else  /* L8/L16 */

#define L   	16
// Unrolled loop to speed up execution
#define FIR(x,w,y) \
  y[0] = ( w[0] *x[0] +w[1] *x[1] +w[2] *x[2] +w[3] *x[3] \
         + w[4] *x[4] +w[5] *x[5] +w[6] *x[6] +w[7] *x[7] \
         + w[8] *x[8] +w[9] *x[9] +w[10]*x[10]+w[11]*x[11] \
         + w[12]*x[12]+w[13]*x[13]+w[14]*x[14]+w[15]*x[15] ) SHIFT

#endif /* L8/L16 */

// Macro to add data to circular que
#define ADDQUE(xi,xq,in)\
  xq[xi]=xq[(xi)+L]=*(in);\
  xi=((xi)-1)&(L-1);

#if defined(UP)

  uint32_t		ci    = l->nch; 	// Index for channels
  uint32_t		nch   = l->nch;   	// Number of channels
  uint32_t		inc   = s->up/s->dn;
  uint32_t		level = s->up%s->dn;
  uint32_t		up    = s->up;
  uint32_t		dn    = s->dn;
  uint32_t		ns    = c->len/l->bps;
  register FORMAT*	w     = s->w;

  register uint32_t	wi    = 0;
  register uint32_t	xi    = 0;

  // Index current channel
  while(ci--){
    // Temporary pointers
    register FORMAT*	x     = s->xq[ci];
    register FORMAT*	in    = ((FORMAT*)c->audio)+ci;
    register FORMAT*	out   = ((FORMAT*)l->audio)+ci;
    FORMAT* 		end   = in+ns; // Block loop end
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
#endif /* UP */

#if defined(DN) /* DN */
  uint32_t		ci    = l->nch; 	// Index for channels
  uint32_t		nch   = l->nch;   	// Number of channels
  uint32_t		inc   = s->dn/s->up;
  uint32_t		level = s->dn%s->up;
  uint32_t		up    = s->up;
  uint32_t		dn    = s->dn;
  uint32_t		ns    = c->len/l->bps;
  FORMAT*		w     = s->w;

  register int32_t	i     = 0;
  register uint32_t	wi    = 0;
  register uint32_t	xi    = 0;

  // Index current channel
  while(ci--){
    // Temporary pointers
    register FORMAT*	x     = s->xq[ci];
    register FORMAT*	in    = ((FORMAT*)c->audio)+ci;
    register FORMAT*	out   = ((FORMAT*)l->audio)+ci;
    register FORMAT* 	end   = in+ns;    // Block loop end
    i = s->i; wi = s->wi; xi = s->xi;

    while(in < end){

      ADDQUE(xi,x,in);
      in+=nch;
      if((--i)<=0){
	// Run the FIR filter
	FIR((&x[xi]),(&w[wi*L]),out);
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
#endif /* DN */
