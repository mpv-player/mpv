/*
 * Copyright (C) 2001 Anders Johansson ajh@atri.curtin.edu.au
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

#if !defined MPLAYER_DSP_H
# error Never use filter.h directly; include dsp.h instead.
#endif

#ifndef MPLAYER_FILTER_H
#define MPLAYER_FILTER_H


// Design and implementation of different types of digital filters


// Flags used for filter design

// Filter characteristics
#define LP          0x00010000 // Low pass
#define HP          0x00020000 // High pass
#define BP          0x00040000 // Band pass
#define BS          0x00080000 // Band stop
#define TYPE_MASK   0x000F0000

// Window types
#define BOXCAR      0x00000001
#define TRIANG      0x00000002
#define HAMMING     0x00000004
#define HANNING     0x00000008
#define BLACKMAN    0x00000010
#define FLATTOP     0x00000011
#define KAISER      0x00000012
#define WINDOW_MASK 0x0000001F

// Parallel filter design
#define	FWD   	    0x00000001 // Forward indexing of polyphase filter
#define REW         0x00000002 // Reverse indexing of polyphase filter
#define ODD         0x00000010 // Make filter HP

// Exported functions
FLOAT_TYPE af_filter_fir(unsigned int n, const FLOAT_TYPE* w, const FLOAT_TYPE* x);

FLOAT_TYPE* af_filter_pfir(unsigned int n, unsigned int k,
                           unsigned int xi, const FLOAT_TYPE** w,
                           const FLOAT_TYPE** x, FLOAT_TYPE* y,
                           unsigned int s);

//int af_filter_updateq(unsigned int n, unsigned int xi,
//                      FLOAT_TYPE* xq, FLOAT_TYPE* in);
int af_filter_updatepq(unsigned int n, unsigned int k, unsigned int xi,
                       FLOAT_TYPE** xq, const FLOAT_TYPE* in, unsigned int s);

int af_filter_design_fir(unsigned int n, FLOAT_TYPE* w, const FLOAT_TYPE* fc,
                         unsigned int flags, FLOAT_TYPE opt);

int af_filter_design_pfir(unsigned int n, unsigned int k, const FLOAT_TYPE* w,
                          FLOAT_TYPE** pw, FLOAT_TYPE g, unsigned int flags);

int af_filter_szxform(const FLOAT_TYPE* a, const FLOAT_TYPE* b, FLOAT_TYPE Q,
                      FLOAT_TYPE fc, FLOAT_TYPE fs, FLOAT_TYPE *k,
                      FLOAT_TYPE *coef);

/* Add new data to circular queue designed to be used with a FIR
   filter. xq is the circular queue, in pointing at the new sample, xi
   current index for xq and n the length of the filter. xq must be n*2
   long.
*/
#define af_filter_updateq(n,xi,xq,in)\
  xq[xi]=(xq)[(xi)+(n)]=*(in);\
  xi=(++(xi))&((n)-1);

#endif /* MPLAYER_FILTER_H */
