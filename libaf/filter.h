/*=============================================================================
//	
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2001 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

#if !defined _DSP_H
# error "Never use <filter.h> directly; include <dsp.h> instead"
#endif

#ifndef _FILTER_H
#define _FILTER_H	1


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
extern _ftype_t af_filter_fir(unsigned int n, _ftype_t* w, _ftype_t* x);

extern _ftype_t* af_filter_pfir(unsigned int n, unsigned int k, unsigned int xi, _ftype_t** w, _ftype_t** x, _ftype_t* y, unsigned int s);

//extern int af_filter_updateq(unsigned int n, unsigned int xi, _ftype_t* xq, _ftype_t* in);
extern int af_filter_updatepq(unsigned int n, unsigned int k, unsigned int xi, _ftype_t** xq, _ftype_t* in, unsigned int s);

extern int af_filter_design_fir(unsigned int n, _ftype_t* w, _ftype_t* fc, unsigned int flags, _ftype_t opt);

extern int af_filter_design_pfir(unsigned int n, unsigned int k, _ftype_t* w, _ftype_t** pw, _ftype_t g, unsigned int flags);

extern int af_filter_szxform(_ftype_t* a, _ftype_t* b, _ftype_t Q, _ftype_t fc, _ftype_t fs, _ftype_t *k, _ftype_t *coef);

/* Add new data to circular queue designed to be used with a FIR
   filter. xq is the circular queue, in pointing at the new sample, xi
   current index for xq and n the length of the filter. xq must be n*2
   long. 
*/
#define af_filter_updateq(n,xi,xq,in)\
  xq[xi]=(xq)[(xi)+(n)]=*(in);\
  xi=(++(xi))&((n)-1);

#endif
