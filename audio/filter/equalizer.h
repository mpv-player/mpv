/*
 * Copyright (C) 2002 Anders Johansson ajh@atri.curtin.edu.au
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

#ifndef MPLAYER_EQUALIZER_H
#define MPLAYER_EQUALIZER_H

/* Equalizer plugin header file defines struct used for setting or
   getting the gain of a specific channel and frequency */

typedef struct equalizer_s
{
  float gain;           // Gain in dB  -15 - 15
  int   channel;        // Channel number 0 - 5
  int   band;           // Frequency band 0 - 9
}equalizer_t;

/* The different frequency bands are:
nr.     center frequency
0       31.25 Hz
1       62.50 Hz
2       125.0 Hz
3       250.0 Hz
4       500.0 Hz
5       1.000 kHz
6       2.000 kHz
7       4.000 kHz
8       8.000 kHz
9       16.00 kHz
*/

#endif /* MPLAYER_EQUALIZER_H */
