/*=============================================================================
//	
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2002 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* Equalizer plugin header file defines struct used for setting or
   getting the gain of a specific channel and frequency */

typedef struct equalizer_s
{
  float gain;   	// Gain in dB  -15 - 15 
  int	channel; 	// Channel number 0 - 5 
  int 	band;		// Frequency band 0 - 9
}equalizer_t;

/* The different frequency bands are:
nr.    	center frequency
0  	31.25 Hz
1 	62.50 Hz
2	125.0 Hz
3	250.0 Hz
4	500.0 Hz
5	1.000 kHz
6	2.000 kHz
7	4.000 kHz
8	8.000 kHz
9       16.00 kHz
*/
