/*=============================================================================
//	
//  This software has been released under the terms of the GNU Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2001 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

#ifndef __FIR_H__
#define __FIR_H__

/* Fixpoint 16 bit FIR filter. The filter is implemented both in C and
MMX assembly. The filter consists of the two inline functions updateq
and firn, update q is used for adding new data to the circular buffer
used by the filter firn. Limitations: max length of n = 16*4 and n
must be multiple of 4 (pad fiter with zeros for other lengths). 
Sometimes it works with filters longer than 4*16 (the problem is
overshoot and the acumulated energy in the filter taps). */

#ifdef HAVE_MMX
inline int32_t firn(int16_t* x, int16_t* w, int16_t n)
{
  register int32_t y; // Output
  // Prologue
  asm volatile(" pxor %mm1, %mm1;\n" ); // Clear buffer yt
  // Main loop
  while((n-=4)>=0){
    asm volatile(
	" movq 		(%1),	%%mm0;\n"  // Load x(n:n+4)
	" pmaddwd	(%0),	%%mm0;\n"  // yt(n:n+1)=sum(x(n:n+4).*w(n:n+4))
	" psrld	      	$16,	%%mm0;\n"  // yt(n:n+1)=yt(n:n+1)>>16
	" paddd	 	%%mm0,	%%mm1;\n"  // yt(n:n+1)=yt(n-2:n-1)+yt(n:n+1)
	:: "r" (w), "r" (x));
    w+=4; x+=4;
  }
  // Epilogue
  asm volatile(
	" movq        	%%mm1, 	%%mm0;\n"  
	" punpckhdq   	%%mm1, 	%%mm0;\n"  
	" paddd       	%%mm0, 	%%mm1;\n"  //yt(n)=yt(n)+yt(n+1)
	" movd        	%%mm1, 	%0   ;\n"  //y=yt
	" emms                       ;\n"
	: "=&r" (y));
  return y;
}

#else /* HAVE_MMX */

// Same thing as above but in C
inline int32_t firn(int16_t* x, int16_t* w, int16_t n)
{
  register int32_t y=0;
  while((n-=4) >=0)
    y+=w[n]*x[n]+w[n+1]*x[n+1]+w[n+2]*x[n+2]+w[n+3]*x[n+3] >> 16;
  return y;
}

#endif /* HAVE_MMX */

/* Add new data to circular queue designed to be used with a FIR
   filter. xq is the circular queue, in pointing at the new sample, xi
   current index for in xq and l the lenght of the filter */
inline uint16_t updateq(int16_t* xq, int16_t* in, uint16_t xi, uint16_t l)  
{
  xq[xi]=xq[xi+l]=*in;
  return (--xi)&(l-1);      \
}

#endif /* __FIR_H__ */

