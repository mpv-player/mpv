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

#if !defined MPLAYER_DSP_H
# error Never use window.h directly; include dsp.h instead.
#endif

#ifndef MPLAYER_WINDOW_H
#define MPLAYER_WINDOW_H

extern void af_window_boxcar(int n, FLOAT_TYPE* w);
extern void af_window_triang(int n, FLOAT_TYPE* w);
extern void af_window_hanning(int n, FLOAT_TYPE* w);
extern void af_window_hamming(int n, FLOAT_TYPE* w);
extern void af_window_blackman(int n, FLOAT_TYPE* w);
extern void af_window_flattop(int n, FLOAT_TYPE* w);
extern void af_window_kaiser(int n, FLOAT_TYPE* w, FLOAT_TYPE b);

#endif /* MPLAYER_WINDOW_H */
