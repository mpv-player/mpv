/*=============================================================================
//	
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2001 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* Calculates a number of window functions. The following window
   functions are currently implemented: Boxcar, Triang, Hanning,
   Hamming, Blackman, Flattop and Kaiser. In the function call n is
   the number of filter taps and w the buffer in which the filter
   coefficients will be stored.
*/

#if !defined _DSP_H
# error "Never use <window.h> directly; include <dsp.h> instead"
#endif

#ifndef _WINDOW_H
#define _WINDOW_H	1

extern void af_window_boxcar(int n, _ftype_t* w);
extern void af_window_triang(int n, _ftype_t* w);
extern void af_window_hanning(int n, _ftype_t* w);
extern void af_window_hamming(int n,_ftype_t* w);
extern void af_window_blackman(int n,_ftype_t* w);
extern void af_window_flattop(int n,_ftype_t* w);
extern void af_window_kaiser(int n, _ftype_t* w,_ftype_t b);

#endif
