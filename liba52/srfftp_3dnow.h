/* 
 *  srfftp.h
 *
 *  Copyright (C) Yuqing Deng <Yuqing_Deng@brown.edu> - April 2000
 *
 *  64 and 128 point split radix fft for ac3dec
 *
 *  The algorithm is desribed in the book:
 *  "Computational Frameworks of the Fast Fourier Transform".
 *
 *  The ideas and the the organization of code borrowed from djbfft written by
 *  D. J. Bernstein <djb@cr.py.to>.  djbff can be found at 
 *  http://cr.yp.to/djbfft.html.
 *
 *  srfftp.h is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  srfftp.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Modified for using AMD's 3DNow! - 3DNowEx(DSP)! SIMD operations 
 *  by Nick Kurshev <nickols_k@mail.ru>
 */

#ifndef SRFFTP_3DNOW_H__
#define SRFFTP_3DNOW_H__

typedef struct
{
  unsigned long val[2];
}i_cmplx_t;

#define TRANS_FILL_MM6_MM7_3DNOW()\
    __asm__ __volatile__(\
	"movq	%1, %%mm7\n\t"\
	"movq	%0, %%mm6\n\t"\
	::"m"(x_plus_minus_3dnow),\
	"m"(x_minus_plus_3dnow)\
	:"memory");

#ifdef HAVE_3DNOWEX
#define PSWAP_MM(mm_base,mm_hlp) "pswapd	"mm_base","mm_base"\n\t"
#else
#define PSWAP_MM(mm_base,mm_hlp)\
	"movq	"mm_base","mm_hlp"\n\t"\
	"psrlq $32, "mm_base"\n\t"\
	"punpckldq "mm_hlp","mm_base"\n\t"
#endif
#ifdef HAVE_3DNOWEX
#define PFNACC_MM(mm_base,mm_hlp)	"pfnacc	"mm_base","mm_base"\n\t"
#else
#define PFNACC_MM(mm_base,mm_hlp)\
	"movq	"mm_base","mm_hlp"\n\t"\
	"psrlq	$32,"mm_hlp"\n\t"\
	"punpckldq "mm_hlp","mm_hlp"\n\t"\
	"pfsub	"mm_hlp","mm_base"\n\t"
#endif

#define TRANSZERO_3DNOW(A0,A4,A8,A12) \
{ \
    __asm__ __volatile__(\
	"movq	%4, %%mm0\n\t" /* mm0 = wTB[0]*/\
	"movq	%5, %%mm1\n\t" /* mm1 = wTB[k*2]*/ \
	"movq	%%mm0, %%mm5\n\t"/*u.re = wTB[0].re + wTB[k*2].re;*/\
	"pfadd	%%mm1, %%mm5\n\t"/*u.im = wTB[0].im + wTB[k*2].im; mm5 = u*/\
	"pxor	%%mm6, %%mm0\n\t"/*mm0 = wTB[0].re | -wTB[0].im */\
	"pxor	%%mm7, %%mm1\n\t"/*mm1 = -wTB[k*2].re | wTB[k*2].im */\
	"pfadd	%%mm1, %%mm0\n\t"/*v.im = wTB[0].re - wTB[k*2].re;*/\
	"movq	%%mm0, %%mm4\n\t"/*v.re =-wTB[0].im + wTB[k*2].im;*/\
	PSWAP_MM("%%mm4","%%mm2")/* mm4 = v*/\
	"movq	%6, %%mm0\n\t" /* a1 = A0;*/\
	"movq	%7, %%mm2\n\t" /* a1 = A4;*/\
	"movq	%%mm0, %%mm1\n\t"\
	"movq	%%mm2, %%mm3\n\t"\
	"pfadd	%%mm5, %%mm0\n\t" /*A0 = a1 + u;*/\
	"pfadd	%%mm4, %%mm2\n\t" /*A12 = a1 + v;*/\
	"movq	%%mm0, %0\n\t"\
	"pfsub	%%mm5, %%mm1\n\t" /*A1 = a1 - u;*/\
	"movq	%%mm2, %3\n\t"\
	"pfsub	%%mm4, %%mm3\n\t" /*A4  = a1 - v;*/\
	"movq	%%mm1, %1\n\t"\
	"movq	%%mm3, %2"\
	:"=m"(A0), "=m"(A8), "=m"(A4), "=m"(A12)\
	:"m"(wTB[0]), "m"(wTB[k*2]), "m"(A0), "m"(A4)\
	:"memory");\
}

#define TRANSHALF_16_3DNOW(A2,A6,A10,A14)\
{\
    __asm__ __volatile__(\
	"movq	%4, %%mm0\n\t"/*u.re = wTB[2].im + wTB[2].re;*/\
	"movq	%%mm0, %%mm1\n\t"\
	"pxor	%%mm7, %%mm1\n\t"\
	"pfacc	%%mm1, %%mm0\n\t"/*u.im = wTB[2].im - wTB[2].re; mm0 = u*/\
	"movq	%5, %%mm1\n\t"  /*a.re = wTB[6].im - wTB[6].re; */\
	"movq	%%mm1, %%mm2\n\t"\
	"pxor	%%mm7, %%mm1\n\t"\
	"pfacc	%%mm2, %%mm1\n\t"/*a.im = wTB[6].im + wTB[6].re;  mm1 = a*/\
	"movq	%%mm1, %%mm2\n\t"\
	"pxor	%%mm7, %%mm2\n\t"/*v.im = u.re - a.re;*/\
	"movq	%%mm0, %%mm3\n\t"/*v.re = u.im + a.im;*/\
	"pfadd	%%mm2, %%mm3\n\t"\
	PSWAP_MM("%%mm3","%%mm2")/*mm3 = v*/\
	"pxor	%%mm6, %%mm1\n\t"/*u.re = u.re + a.re;*/\
	"pfadd	%%mm1, %%mm0\n\t"/*u.im = u.im - a.im; mm0 = u*/\
	"movq	%8, %%mm2\n\t"\
	"pfmul	%%mm2, %%mm3\n\t" /* v *= HSQRT2_3DNOW; */\
	"pfmul	%%mm2, %%mm0\n\t" /* u *= HSQRT2_3DNOW; */\
	"movq	%6, %%mm1\n\t" /* a1 = A2;*/\
	"movq	%7, %%mm5\n\t" /* a1 = A6;*/\
	"movq	%%mm1, %%mm2\n\t"\
	"movq	%%mm3, %%mm4\n\t"\
	"pfadd	%%mm0, %%mm1\n\t" /*A2 = a1 + u;*/\
	"pxor	%%mm6, %%mm4\n\t"/*A6.re  = a1.re + v.re;*/\
	"pfsub	%%mm0, %%mm2\n\t" /*A2 = a1 - u;*/\
	"pxor	%%mm7, %%mm3\n\t"/*A14.re = a1.re - v.re;*/\
	"movq	%%mm1, %0\n\t"\
	"movq	%%mm2, %1\n\t"\
	"movq	%%mm5, %%mm2\n\t"\
	"pfadd	%%mm4, %%mm5\n\t"/*A6.im  = a1.im - v.im;*/\
	"pfadd	%%mm3, %%mm2\n\t"/*A14.im = a1.im + v.im;*/\
	"movq	%%mm5, %2\n\t"\
	"movq	%%mm2, %3"\
	:"=m"(A2), "=m"(A10), "=m"(A6), "=m"(A14)\
	:"m"(wTB[2]), "m"(wTB[6]), "m"(A2), "m"(A6), "m"(HSQRT2_3DNOW)\
	:"memory");\
}

#define TRANS_3DNOW(A1,A5,A9,A13,WT,WB,D,D3)\
{ \
    __asm__ __volatile__(\
	"movq	%1,	%%mm4\n\t"\
	"movq	%%mm4,	%%mm5\n\t"\
	"punpckldq %%mm4, %%mm4\n\t"/*mm4 = D.re | D.re */\
	"punpckhdq %%mm5, %%mm5\n\t"/*mm5 = D.im | D.im */\
	"movq	%0,	%%mm0\n\t"\
	"pfmul	%%mm0,	%%mm4\n\t"/* mm4 =u.re | u.im */\
	"pfmul	%%mm0,	%%mm5\n\t"/* mm5 = a.re | a.im */\
	PSWAP_MM("%%mm5","%%mm3")\
	"pxor	%%mm7,	%%mm5\n\t"\
	"pfadd	%%mm5,	%%mm4\n\t"/* mm4 = u*/\
	"movq	%3,	%%mm1\n\t"\
	"movq	%2,	%%mm0\n\t"\
	PSWAP_MM("%%mm1","%%mm3")\
	"movq	%%mm0,	%%mm2\n\t"\
	"pfmul	%%mm1,	%%mm0\n\t"/* mm0 = a*/\
	"pfmul	%3,	%%mm2\n\t"/* mm2 = v*/\
	PFNACC_MM("%%mm2","%%mm3")\
	"pfacc	%%mm0,	%%mm0\n\t"\
	"movq	%%mm4,	%%mm5\n\t"\
	"punpckldq %%mm0,%%mm2\n\t"/*mm2 = v.re | a.re*/\
	"pxor	%%mm6,	%%mm5\n\t"\
	"movq	%%mm2,	%%mm3\n\t"\
	"pxor	%%mm7,	%%mm3\n\t"\
	"pfadd	%%mm3,	%%mm5\n\t"\
	PSWAP_MM("%%mm5","%%mm3")/* mm5 = v*/\
	"pfadd	%%mm2,	%%mm4\n\t"\
	:\
	:"m"(WT), "m"(D), "m"(WB), "m"(D3)\
	:"memory");\
    __asm__ __volatile__(\
	"movq	%4, %%mm0\n\t"/* a1 = A1*/\
	"movq	%5, %%mm2\n\t"/* a1 = A5*/\
	"movq	%%mm0, %%mm1\n\t"\
	"movq	%%mm2, %%mm3\n\t"\
	"pfadd	%%mm4, %%mm0\n\t"/*A1 = a1 + u*/\
	"pfsub	%%mm5, %%mm2\n\t"/*A5 = a1 - v*/\
	"movq	%%mm0, %0\n\t"\
	"pfsub	%%mm4, %%mm1\n\t"/*A9 = a1 - u*/\
	"movq	%%mm2, %2\n\t"\
	"pfadd	%%mm5, %%mm3\n\t"/*A9 = a1 + v*/\
	"movq	%%mm1, %1\n\t"\
	"movq	%%mm3, %3"\
	:"=m"(A1), "=m"(A9), "=m"(A5), "=m"(A13)\
	:"m"(A1), "m"(A5)\
	:"memory");\
}

#endif
