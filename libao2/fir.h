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

/* 4, 8 and 16 tap FIR filters implemented using SSE instructions 
   int16_t* x Input data
   int16_t* y Output value
   int16_t* w Filter weights 
   
   C function
   for(int i = 0 ; i < L ; i++)
     *y += w[i]*x[i];
*/

#ifdef HAVE_SSE

// This block should be MMX only compatible, but it isn't...
#ifdef L4
#define LOAD_QUE(x) \
        __asm __volatile("movq %0, %%mm2\n\t" \
                         :                    \
                         :"m"((x)[0])         \
                         :"memory");
#define SAVE_QUE(x) \
        __asm __volatile("movq %%mm2, %0\n\t" \
                         "emms          \n\t" \
                         :"=m"(x[0])          \
                         :                    \
                         :"memory");
#define UPDATE_QUE(in) \
        __asm __volatile("psllq   $16,   %%mm2\n\t"    \
                         "pinsrw  $0,    %0,%%mm2\n\t" \
                          :                            \
                          :"m" ((in)[0])               \
                          :"memory");                  
#define FIR(x,w,y) \
        __asm __volatile("movq	  %%mm2, %%mm0\n\t" \
                         "pmaddwd %1,    %%mm0\n\t" \
                         "movq    %%mm0, %%mm1\n\t" \
                         "psrlq   $32, 	 %%mm1\n\t" \
                         "paddd   %%mm0, %%mm1\n\t" \
                         "movd    %%mm1, %%esi\n\t" \
                         "shrl    $16,   %%esi\n\t" \
                         "movw    %%si,  %0\n\t"    \
			 : "=m" ((y)[0])            \
			 : "m" ((w)[0])             \
			 : "memory", "%esi"); 
#endif /* L4 */

// It is possible to make the 8 bit filter a lot faster by using the
// 128 bit registers, feel free to optimize.
#ifdef L8
#define LOAD_QUE(x) \
        __asm __volatile("movq %0, %%mm5\n\t" \
                         "movq %1, %%mm4\n\t" \
                         :                    \
                         :"m"((x)[0]),        \
                          "m"((x)[4])         \
                         :"memory");
#define SAVE_QUE(x) \
        __asm __volatile("movq %%mm5, %0\n\t" \
                         "movq %%mm4, %1\n\t" \
                         "emms          \n\t" \
                         :"=m"((x)[0]),       \
                          "=m"((x)[4])        \
                         :                    \
                         :"memory");

// Below operation could replace line 2 to 5 in macro below but can
// not cause of compiler bug ???
// "pextrw $3, %%mm5,%%eax\n\t"
#define UPDATE_QUE(in) \
        __asm __volatile("psllq    $16,   %%mm4\n\t"        \
                         "movq	   %%mm5, %%mm0\n\t" 	    \
                         "psrlq    $48,   %%mm0\n\t"        \
                         "movd     %%mm0, %%eax\n\t"        \
			 "pinsrw   $0,    %%eax,%%mm4\n\t"  \
                         "psllq    $16,   %%mm5\n\t"        \
                         "pinsrw   $0,    %0,%%mm5\n\t"     \
                          :                                 \
                          :"m" ((in)[0])                    \
                          :"memory", "%eax");                  
#define FIR(x,w,y) \
        __asm __volatile("movq	  %%mm5, %%mm0\n\t" \
                         "pmaddwd %1,    %%mm0\n\t" \
                         "movq	  %%mm4, %%mm1\n\t" \
                         "pmaddwd %2,    %%mm1\n\t" \
                         "paddd   %%mm1, %%mm0\n\t" \
                         "movq    %%mm0, %%mm1\n\t" \
                         "psrlq   $32, 	 %%mm1\n\t" \
                         "paddd   %%mm0, %%mm1\n\t" \
                         "movd    %%mm1, %%esi\n\t" \
                         "shrl    $16,   %%esi\n\t" \
                         "movw    %%si,  %0\n\t"    \
			 : "=m" ((y)[0])            \
			 : "m" ((w)[0]),            \
			   "m" ((w)[4])             \
			 : "memory", "%esi"); 
#endif /* L8 */

#else /* HAVE_SSE */

#define LOAD_QUE(x)
#define SAVE_QUE(x)
#define UPDATE_QUE(inm) \
  xi=(--xi)&(L-1);     \
  x[xi]=x[xi+L]=*(inm);

#ifdef L4
#define FIR(x,w,y) \
        y[0]=(w[0]*x[0]+w[1]*x[1]+w[2]*x[2]+w[3]*x[3]) >> 16;
#else
#define FIR(x,w,y){ \
  int16_t a = (w[0]*x[0]+w[1]*x[1]+w[2]*x[2]+w[3]*x[3]) >> 16; \
  int16_t b = (w[4]*x[4]+w[5]*x[5]+w[6]*x[6]+w[7]*x[7]) >> 16; \
  y[0]      = a+b; \
}
#endif /* L4 */

#endif /* HAVE_SSE */

#endif /* __FIR_H__ */


