/*
 * Copyright (C) 2001 Anders Johansson ajh@atri.curtin.edu.au
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
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

void af_window_boxcar(int n, FLOAT_TYPE* w);
void af_window_triang(int n, FLOAT_TYPE* w);
void af_window_hanning(int n, FLOAT_TYPE* w);
void af_window_hamming(int n, FLOAT_TYPE* w);
void af_window_blackman(int n, FLOAT_TYPE* w);
void af_window_flattop(int n, FLOAT_TYPE* w);
void af_window_kaiser(int n, FLOAT_TYPE* w, FLOAT_TYPE b);

#endif /* MPLAYER_WINDOW_H */
