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

static float HSQRT2_3DNOW = 0.707106781188;

#ifdef HAVE_3DNOWEX
#define TRANS_FILL_MM6_MM7_3DNOW()\
    asm(\
	"movl	$-1, %%eax\n\t"\
	"movd	%%eax, %%mm7\n\t"\
	"negl	%%eax\n\t"\
	"movd	%%eax, %%mm6\n\t"\
	"punpckldq %%mm6, %%mm7\n\t" /* -1.0 | 1.0 */\
	"pi2fd	%%mm7, %%mm7\n\t"\
	"pswapd	%%mm7, %%mm6\n\t"/* 1.0 | -1.0 */\
	:::"eax","memory");
#else
#define TRANS_FILL_MM6_MM7_3DNOW()\
    asm(\
	"movl	$-1, %%eax\n\t"\
	"movd	%%eax, %%mm7\n\t"\
	"negl	%%eax\n\t"\
	"movd	%%eax, %%mm6\n\t"\
	"punpckldq %%mm6, %%mm7\n\t" /* -1.0 | 1.0 */\
	"punpckldq %%mm7, %%mm6\n\t" /* 1.0 | -1.0 */\
	"pi2fd	%%mm7, %%mm7\n\t"\
	"pi2fd	%%mm6, %%mm6\n\t"\
	:::"eax","memory");
#endif

#ifdef HAVE_3DNOWEX
#define PSWAP_MM(mm_base,mm_hlp) "pswapd	"##mm_base","##mm_base"\n\t"
#else
#define PSWAP_MM(mm_base,mm_hlp)\
	"movq	"##mm_base","##mm_hlp"\n\t"\
	"psrlq $32, "##mm_base"\n\t"\
	"punpckldq "##mm_hlp","##mm_base"\n\t"
#endif

#define TRANSZERO_3DNOW(A0,A4,A8,A12) \
{ \
    asm volatile("femms":::"memory");\
    TRANS_FILL_MM6_MM7_3DNOW()\
    asm(\
	"movq	%4, %%mm0\n\t" /* mm0 = wTB[0]*/\
	"movq	%5, %%mm1\n\t" /* mm1 = wTB[k*2]*/ \
	"movq	%%mm0, %%mm5\n\t"/*u.re = wTB[0].re + wTB[k*2].re;*/\
	"pfadd	%%mm1, %%mm5\n\t"/*u.im = wTB[0].im + wTB[k*2].im; mm5 = u*/\
	"pfmul  %%mm6, %%mm0\n\t"/*mm0 = wTB[0].re | -wTB[0].im */\
	"pfmul	%%mm7, %%mm1\n\t"/*mm1 = -wTB[k*2].re | wTB[k*2].im */\
	"pfadd	%%mm1, %%mm0\n\t"/*v.im = wTB[0].re - wTB[k*2].re;*/\
	"movq	%%mm0, %%mm4\n\t"/*v.re =-wTB[0].im + wTB[k*2].im;*/\
	PSWAP_MM("%%mm4","%%mm2")/* mm4 = v*/\
	"movq	%6, %%mm0\n\t" /* a1 = A0;*/\
	"movq	%7, %%mm2\n\t" /* a1 = A4;*/\
	"movq	%%mm0, %%mm1\n\t"\
	"movq	%%mm2, %%mm3\n\t"\
	"pfadd	%%mm5, %%mm0\n\t" /*A0 = a1 + u;*/\
	"pfadd	%%mm4, %%mm2\n\t" /*A12 = a1 + v;*/\
	"pfsub	%%mm5, %%mm1\n\t" /*A1 = a1 - u;*/\
	"pfsub	%%mm4, %%mm3\n\t" /*A4  = a1 - v;*/\
	"movq	%%mm0, %0\n\t"\
	"movq	%%mm2, %3\n\t"\
	"movq	%%mm1, %1\n\t"\
	"movq	%%mm3, %2"\
	:"=m"(A0), "=m"(A8), "=m"(A4), "=m"(A12)\
	:"m"(wTB[0]), "m"(wTB[k*2]), "0"(A0), "2"(A4)\
	:"memory");\
    asm volatile("femms":::"memory");\
}

#define TRANSHALF_16_3DNOW(A2,A6,A10,A14)\
{\
    asm volatile("femms":::"memory");\
    TRANS_FILL_MM6_MM7_3DNOW()\
    asm(\
	"movq	%4, %%mm0\n\t"/*u.re = wTB[2].im + wTB[2].re;*/\
	"movq	%%mm0, %%mm1\n\t"\
	"pfmul	%%mm7, %%mm1\n\t"\
	"pfacc	%%mm1, %%mm0\n\t"/*u.im = wTB[2].im - wTB[2].re; mm0 = u*/\
	"movq	%5, %%mm1\n\t"  /*a.re = wTB[6].im - wTB[6].re; */\
	"movq	%%mm1, %%mm2\n\t"\
	"pfmul	%%mm7, %%mm1\n\t"\
	"pfacc	%%mm2, %%mm1\n\t"/*a.im = wTB[6].im + wTB[6].re;  mm1 = a*/\
	"movq	%%mm1, %%mm2\n\t"\
	"pfmul	%%mm7, %%mm2\n\t"/*v.im = u.re - a.re;*/\
	"movq	%%mm0, %%mm3\n\t"/*v.re = u.im + a.im;*/\
	"pfadd	%%mm2, %%mm3\n\t"\
	PSWAP_MM("%%mm3","%%mm2")/*mm3 = v*/\
	"pfmul	%%mm6, %%mm1\n\t"/*u.re = u.re + a.re;*/\
	"pfadd	%%mm1, %%mm0\n\t"/*u.im = u.im - a.im; mm0 = u*/\
	"movd	%8, %%mm2\n\t"\
	"punpckldq %8, %%mm2\n\t"\
	"pfmul	%%mm2, %%mm3\n\t" /* v *= HSQRT2_3DNOW; */\
	"pfmul	%%mm2, %%mm0\n\t" /* u *= HSQRT2_3DNOW; */\
	"movq	%6, %%mm1\n\t" /* a1 = A2;*/\
	"movq	%%mm1, %%mm2\n\t"\
	"pfadd	%%mm0, %%mm1\n\t" /*A2 = a1 + u;*/\
	"pfsub	%%mm0, %%mm2\n\t" /*A2 = a1 - u;*/\
	"movq	%%mm1, %0\n\t"\
	"movq	%%mm2, %1\n\t"\
	"movq	%7, %%mm1\n\t" /* a1 = A6;*/\
	"movq	%%mm1, %%mm2\n\t"\
	"movq	%%mm3, %%mm4\n\t"\
	"pfmul	%%mm6, %%mm4\n\t"/*A6.re  = a1.re + v.re;*/\
	"pfadd	%%mm4, %%mm1\n\t"/*A6.im  = a1.im - v.im;*/\
	"pfmul	%%mm7, %%mm3\n\t"/*A14.re = a1.re - v.re;*/\
	"pfadd	%%mm3, %%mm2\n\t"/*A14.im = a1.im + v.im;*/\
	"movq	%%mm1, %2\n\t"\
	"movq	%%mm2, %3"\
	:"=m"(A2), "=m"(A10), "=m"(A6), "=m"(A14)\
	:"m"(wTB[2]), "m"(wTB[6]), "0"(A2), "2"(A6), "m"(HSQRT2_3DNOW)\
	:"memory");\
    asm volatile("femms":::"memory");\
}

#endif
