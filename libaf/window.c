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

#include <math.h>
#include "dsp.h"

/*
// Boxcar
//
// n window length
// w buffer for the window parameters
*/
void af_window_boxcar(int n, _ftype_t* w)
{
  int i;
  // Calculate window coefficients
  for (i=0 ; i<n ; i++)
    w[i] = 1.0;
}


/*
// Triang a.k.a Bartlett
//
//               |    (N-1)| 
//           2 * |k - -----|
//               |      2  |
// w = 1.0 - ---------------
//                    N+1
// n window length
// w buffer for the window parameters
*/
void af_window_triang(int n, _ftype_t* w)
{
  _ftype_t k1  = (_ftype_t)(n & 1);
  _ftype_t k2  = 1/((_ftype_t)n + k1);
  int      end = (n + 1) >> 1;
  int	   i;
  
  // Calculate window coefficients
  for (i=0 ; i<end ; i++)
    w[i] = w[n-i-1] = (2.0*((_ftype_t)(i+1))-(1.0-k1))*k2;
}


/*
// Hanning
//                   2*pi*k
// w = 0.5 - 0.5*cos(------), where 0 < k <= N
//                    N+1
// n window length
// w buffer for the window parameters
*/
void af_window_hanning(int n, _ftype_t* w)
{
  int	   i;
  _ftype_t k = 2*M_PI/((_ftype_t)(n+1)); // 2*pi/(N+1)
  
  // Calculate window coefficients
  for (i=0; i<n; i++)
    *w++ = 0.5*(1.0 - cos(k*(_ftype_t)(i+1)));
}

/*
// Hamming
//                        2*pi*k
// w(k) = 0.54 - 0.46*cos(------), where 0 <= k < N
//                         N-1
//
// n window length
// w buffer for the window parameters
*/
void af_window_hamming(int n,_ftype_t* w)
{
  int      i;
  _ftype_t k = 2*M_PI/((_ftype_t)(n-1)); // 2*pi/(N-1)

  // Calculate window coefficients
  for (i=0; i<n; i++)
    *w++ = 0.54 - 0.46*cos(k*(_ftype_t)i);
}

/*
// Blackman
//                       2*pi*k             4*pi*k
// w(k) = 0.42 - 0.5*cos(------) + 0.08*cos(------), where 0 <= k < N
//                        N-1                 N-1
//
// n window length
// w buffer for the window parameters
*/
void af_window_blackman(int n,_ftype_t* w)
{
  int      i;
  _ftype_t k1 = 2*M_PI/((_ftype_t)(n-1)); // 2*pi/(N-1)
  _ftype_t k2 = 2*k1; // 4*pi/(N-1)

  // Calculate window coefficients
  for (i=0; i<n; i++)
    *w++ = 0.42 - 0.50*cos(k1*(_ftype_t)i) + 0.08*cos(k2*(_ftype_t)i);
}

/*
// Flattop
//                                        2*pi*k                     4*pi*k
// w(k) = 0.2810638602 - 0.5208971735*cos(------) + 0.1980389663*cos(------), where 0 <= k < N
//                                          N-1                        N-1
//
// n window length
// w buffer for the window parameters
*/
void af_window_flattop(int n,_ftype_t* w)
{
  int      i;
  _ftype_t k1 = 2*M_PI/((_ftype_t)(n-1)); // 2*pi/(N-1)
  _ftype_t k2 = 2*k1;                   // 4*pi/(N-1)
  
  // Calculate window coefficients
  for (i=0; i<n; i++)
    *w++ = 0.2810638602 - 0.5208971735*cos(k1*(_ftype_t)i) + 0.1980389663*cos(k2*(_ftype_t)i);
}

/* Computes the 0th order modified Bessel function of the first kind.  
// (Needed to compute Kaiser window) 
//   
// y = sum( (x/(2*n))^2 )
//      n
*/
#define BIZ_EPSILON 1E-21 // Max error acceptable 

static _ftype_t besselizero(_ftype_t x)
{ 
  _ftype_t temp;
  _ftype_t sum   = 1.0;
  _ftype_t u     = 1.0;
  _ftype_t halfx = x/2.0;
  int      n     = 1;

  do {
    temp = halfx/(_ftype_t)n;
    u *=temp * temp;
    sum += u;
    n++;
  } while (u >= BIZ_EPSILON * sum);
  return(sum);
}

/*
// Kaiser
//
// n window length
// w buffer for the window parameters
// b beta parameter of Kaiser window, Beta >= 1
//
// Beta trades the rejection of the low pass filter against the
// transition width from passband to stop band.  Larger Beta means a
// slower transition and greater stop band rejection.  See Rabiner and
// Gold (Theory and Application of DSP) under Kaiser windows for more
// about Beta.  The following table from Rabiner and Gold gives some
// feel for the effect of Beta:
// 
// All ripples in dB, width of transition band = D*N where N = window
// length
// 
// BETA    D       PB RIP   SB RIP
// 2.120   1.50  +-0.27      -30
// 3.384   2.23    0.0864    -40
// 4.538   2.93    0.0274    -50
// 5.658   3.62    0.00868   -60
// 6.764   4.32    0.00275   -70
// 7.865   5.0     0.000868  -80
// 8.960   5.7     0.000275  -90
// 10.056  6.4     0.000087  -100
*/
void af_window_kaiser(int n, _ftype_t* w, _ftype_t b)
{
  _ftype_t tmp;
  _ftype_t k1  = 1.0/besselizero(b);
  int	   k2  = 1 - (n & 1);
  int      end = (n + 1) >> 1;
  int      i; 
  
  // Calculate window coefficients
  for (i=0 ; i<end ; i++){
    tmp = (_ftype_t)(2*i + k2) / ((_ftype_t)n - 1.0);
    w[end-(1&(!k2))+i] = w[end-1-i] = k1 * besselizero(b*sqrt(1.0 - tmp*tmp));
  }
}

