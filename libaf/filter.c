/*=============================================================================
//	
//  This software has been released under the terms of the GNU Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2001 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* Design and implementation of different types of digital filters

*/
#include <math.h>
#include "dsp.h"

/* C implementation of FIR filter y=w*x

   n number of filter taps, where mod(n,4)==0
   w filter taps
   x input signal must be a circular buffer which is indexed backwards 
*/
inline _ftype_t fir(register unsigned int n, _ftype_t* w, _ftype_t* x)
{
  register _ftype_t y; // Output
  y = 0.0; 
  do{
    n--;
    y+=w[n]*x[n];
  }while(n != 0);
  return y;
}

/* C implementation of parallel FIR filter y(k)=w(k) * x(k) (where * denotes convolution)

   n  number of filter taps, where mod(n,4)==0
   d  number of filters
   xi current index in xq
   w  filter taps k by n big
   x  input signal must be a circular buffers which are indexed backwards 
   y  output buffer
   s  output buffer stride
*/
inline _ftype_t* pfir(unsigned int n, unsigned int d, unsigned int xi, _ftype_t** w, _ftype_t** x, _ftype_t* y, unsigned int s)
{
  register _ftype_t* xt = *x + xi;
  register _ftype_t* wt = *w;
  register int    nt = 2*n;
  while(d-- > 0){
    *y = fir(n,wt,xt);
    wt+=n;
    xt+=nt;
    y+=s;
  }
  return y;
}

/* Add new data to circular queue designed to be used with a parallel
   FIR filter, with d filters. xq is the circular queue, in pointing
   at the new samples, xi current index in xq and n the length of the
   filter. xq must be n*2 by k big, s is the index for in.
*/
inline int updatepq(unsigned int n, unsigned int d, unsigned int xi, _ftype_t** xq, _ftype_t* in, unsigned int s)  
{
  register _ftype_t* txq = *xq + xi;
  register int nt = n*2;
  
  while(d-- >0){
    *txq= *(txq+n) = *in;
    txq+=nt;
    in+=s;
  }
  return (++xi)&(n-1);
}


/* Design FIR filter using the Window method

   n     filter length must be odd for HP and BS filters
   w     buffer for the filter taps (must be n long)
   fc    cutoff frequencies (1 for LP and HP, 2 for BP and BS) 
         0 < fc < 1 where 1 <=> Fs/2
   flags window and filter type as defined in filter.h
         variables are ored together: i.e. LP|HAMMING will give a 
	 low pass filter designed using a hamming window  
   opt   beta constant used only when designing using kaiser windows
   
   returns 0 if OK, -1 if fail
*/
int design_fir(unsigned int n, _ftype_t* w, _ftype_t* fc, unsigned int flags, _ftype_t opt)
{
  unsigned int	o   = n & 1;          	// Indicator for odd filter length
  unsigned int	end = ((n + 1) >> 1) - o;       // Loop end
  unsigned int	i;			// Loop index

  _ftype_t k1 = 2 * M_PI;		// 2*pi*fc1
  _ftype_t k2 = 0.5 * (_ftype_t)(1 - o);// Constant used if the filter has even length
  _ftype_t k3;				// 2*pi*fc2 Constant used in BP and BS design
  _ftype_t g  = 0.0;     		// Gain
  _ftype_t t1,t2,t3;     		// Temporary variables
  _ftype_t fc1,fc2;			// Cutoff frequencies

  // Sanity check
  if(!w || (n == 0)) return -1;

  // Get window coefficients
  switch(flags & WINDOW_MASK){
  case(BOXCAR):
    boxcar(n,w); break;
  case(TRIANG):
    triang(n,w); break;
  case(HAMMING):
    hamming(n,w); break;
  case(HANNING):
    hanning(n,w); break;
  case(BLACKMAN):
    blackman(n,w); break;
  case(FLATTOP):
    flattop(n,w); break;
  case(KAISER):
    kaiser(n,w,opt); break;
  default:
    return -1;	
  }

  if(flags & (LP | HP)){ 
    fc1=*fc;
    // Cutoff frequency must be < 0.5 where 0.5 <=> Fs/2
    fc1 = ((fc1 <= 1.0) && (fc1 > 0.0)) ? fc1/2 : 0.25;
    k1 *= fc1;

    if(flags & LP){ // Low pass filter

      // If the filter length is odd, there is one point which is exactly
      // in the middle. The value at this point is 2*fCutoff*sin(x)/x, 
      // where x is zero. To make sure nothing strange happens, we set this
      // value separately.
      if (o){
	w[end] = fc1 * w[end] * 2.0;
	g=w[end];
      }

      // Create filter
      for (i=0 ; i<end ; i++){
	t1 = (_ftype_t)(i+1) - k2;
	w[end-i-1] = w[n-end+i] = w[end-i-1] * sin(k1 * t1)/(M_PI * t1); // Sinc
	g += 2*w[end-i-1]; // Total gain in filter
      }
    }
    else{ // High pass filter
      if (!o) // High pass filters must have odd length
	return -1;
      w[end] = 1.0 - (fc1 * w[end] * 2.0);
      g= w[end];

      // Create filter
      for (i=0 ; i<end ; i++){
	t1 = (_ftype_t)(i+1);
	w[end-i-1] = w[n-end+i] = -1 * w[end-i-1] * sin(k1 * t1)/(M_PI * t1); // Sinc
	g += ((i&1) ? (2*w[end-i-1]) : (-2*w[end-i-1])); // Total gain in filter
      }
    }
  }

  if(flags & (BP | BS)){
    fc1=fc[0];
    fc2=fc[1];
    // Cutoff frequencies must be < 1.0 where 1.0 <=> Fs/2
    fc1 = ((fc1 <= 1.0) && (fc1 > 0.0)) ? fc1/2 : 0.25;
    fc2 = ((fc2 <= 1.0) && (fc2 > 0.0)) ? fc2/2 : 0.25;
    k3  = k1 * fc2; // 2*pi*fc2
    k1 *= fc1;      // 2*pi*fc1

    if(flags & BP){ // Band pass
      // Calculate center tap
      if (o){
	g=w[end]*(fc1+fc2);
	w[end] = (fc2 - fc1) * w[end] * 2.0;
      }

      // Create filter
      for (i=0 ; i<end ; i++){
	t1 = (_ftype_t)(i+1) - k2;
	t2 = sin(k3 * t1)/(M_PI * t1); // Sinc fc2
	t3 = sin(k1 * t1)/(M_PI * t1); // Sinc fc1
	g += w[end-i-1] * (t3 + t2);   // Total gain in filter
	w[end-i-1] = w[n-end+i] = w[end-i-1] * (t2 - t3); 
      }
    }      
    else{ // Band stop
      if (!o) // Band stop filters must have odd length
	return -1;
      w[end] = 1.0 - (fc2 - fc1) * w[end] * 2.0;
      g= w[end];

      // Create filter
      for (i=0 ; i<end ; i++){
	t1 = (_ftype_t)(i+1);
	t2 = sin(k1 * t1)/(M_PI * t1); // Sinc fc1
	t3 = sin(k3 * t1)/(M_PI * t1); // Sinc fc2
	w[end-i-1] = w[n-end+i] = w[end-i-1] * (t2 - t3); 
	g += 2*w[end-i-1]; // Total gain in filter
      }
    }
  }

  // Normalize gain
  g=1/g;
  for (i=0; i<n; i++) 
    w[i] *= g;
  
  return 0;
}

/* Design polyphase FIR filter from prototype filter

   n     length of prototype filter
   k     number of polyphase components
   w     prototype filter taps
   pw    Parallel FIR filter 
   g     Filter gain
   flags FWD forward indexing
         REW reverse indexing
	 ODD multiply every 2nd filter tap by -1 => HP filter

   returns 0 if OK, -1 if fail
*/
int design_pfir(unsigned int n, unsigned int k, _ftype_t* w, _ftype_t** pw, _ftype_t g, unsigned int flags)
{
  int l = (int)n/k;	// Length of individual FIR filters
  int i;     	// Counters
  int j;
  _ftype_t t;	// g * w[i]
  
  // Sanity check
  if(l<1 || k<1 || !w || !pw)
    return -1;

  // Do the stuff
  if(flags&REW){
    for(j=l-1;j>-1;j--){//Columns
      for(i=0;i<(int)k;i++){//Rows
	t=g *  *w++;
	pw[i][j]=t * ((flags & ODD) ? ((j & 1) ? -1 : 1) : 1);
      }
    }
  }
  else{
    for(j=0;j<l;j++){//Columns
      for(i=0;i<(int)k;i++){//Rows
	t=g *  *w++;
	pw[i][j]=t * ((flags & ODD) ? ((j & 1) ? 1 : -1) : 1);
      }
    }
  }
  return -1;
}
