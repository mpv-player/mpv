/*
 * imdct.c
 * Copyright (C) 2000-2001 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * a52dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * a52dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * SSE optimizations from Michael Niedermayer (michaelni@gmx.at)
 * 3DNOW optimizations from Nick Kurshev <nickols_k@mail.ru>
 *   michael did port them from libac3 (untested, perhaps totally broken)
 */

#include "config.h"

#include <math.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795029
#endif
#include <inttypes.h>

#include "a52.h"
#include "a52_internal.h"
#include "mm_accel.h"

#ifdef RUNTIME_CPUDETECT
#undef HAVE_3DNOWEX
#endif

#define USE_AC3_C

void (* imdct_256) (sample_t data[], sample_t delay[], sample_t bias);
void (* imdct_512) (sample_t data[], sample_t delay[], sample_t bias);

typedef struct complex_s {
    sample_t real;
    sample_t imag;
} complex_t;

static void fft_128p(complex_t *a);
static void fft_128p_3dnow(complex_t *a);

static const int pm128[128] __attribute__((aligned(16))) =
{
	0, 16, 32, 48, 64, 80,  96, 112,  8, 40, 72, 104, 24, 56,  88, 120,
	4, 20, 36, 52, 68, 84, 100, 116, 12, 28, 44,  60, 76, 92, 108, 124,
	2, 18, 34, 50, 66, 82,  98, 114, 10, 42, 74, 106, 26, 58,  90, 122,
	6, 22, 38, 54, 70, 86, 102, 118, 14, 46, 78, 110, 30, 62,  94, 126,
	1, 17, 33, 49, 65, 81,  97, 113,  9, 41, 73, 105, 25, 57,  89, 121,
	5, 21, 37, 53, 69, 85, 101, 117, 13, 29, 45,  61, 77, 93, 109, 125,
	3, 19, 35, 51, 67, 83,  99, 115, 11, 43, 75, 107, 27, 59,  91, 123,
	7, 23, 39, 55, 71, 87, 103, 119, 15, 31, 47,  63, 79, 95, 111, 127
}; 

/* 128 point bit-reverse LUT */
static uint8_t bit_reverse_512[] = {
	0x00, 0x40, 0x20, 0x60, 0x10, 0x50, 0x30, 0x70, 
	0x08, 0x48, 0x28, 0x68, 0x18, 0x58, 0x38, 0x78, 
	0x04, 0x44, 0x24, 0x64, 0x14, 0x54, 0x34, 0x74, 
	0x0c, 0x4c, 0x2c, 0x6c, 0x1c, 0x5c, 0x3c, 0x7c, 
	0x02, 0x42, 0x22, 0x62, 0x12, 0x52, 0x32, 0x72, 
	0x0a, 0x4a, 0x2a, 0x6a, 0x1a, 0x5a, 0x3a, 0x7a, 
	0x06, 0x46, 0x26, 0x66, 0x16, 0x56, 0x36, 0x76, 
	0x0e, 0x4e, 0x2e, 0x6e, 0x1e, 0x5e, 0x3e, 0x7e, 
	0x01, 0x41, 0x21, 0x61, 0x11, 0x51, 0x31, 0x71, 
	0x09, 0x49, 0x29, 0x69, 0x19, 0x59, 0x39, 0x79, 
	0x05, 0x45, 0x25, 0x65, 0x15, 0x55, 0x35, 0x75, 
	0x0d, 0x4d, 0x2d, 0x6d, 0x1d, 0x5d, 0x3d, 0x7d, 
	0x03, 0x43, 0x23, 0x63, 0x13, 0x53, 0x33, 0x73, 
	0x0b, 0x4b, 0x2b, 0x6b, 0x1b, 0x5b, 0x3b, 0x7b, 
	0x07, 0x47, 0x27, 0x67, 0x17, 0x57, 0x37, 0x77, 
	0x0f, 0x4f, 0x2f, 0x6f, 0x1f, 0x5f, 0x3f, 0x7f};

static uint8_t bit_reverse_256[] = {
	0x00, 0x20, 0x10, 0x30, 0x08, 0x28, 0x18, 0x38, 
	0x04, 0x24, 0x14, 0x34, 0x0c, 0x2c, 0x1c, 0x3c, 
	0x02, 0x22, 0x12, 0x32, 0x0a, 0x2a, 0x1a, 0x3a, 
	0x06, 0x26, 0x16, 0x36, 0x0e, 0x2e, 0x1e, 0x3e, 
	0x01, 0x21, 0x11, 0x31, 0x09, 0x29, 0x19, 0x39, 
	0x05, 0x25, 0x15, 0x35, 0x0d, 0x2d, 0x1d, 0x3d, 
	0x03, 0x23, 0x13, 0x33, 0x0b, 0x2b, 0x1b, 0x3b, 
	0x07, 0x27, 0x17, 0x37, 0x0f, 0x2f, 0x1f, 0x3f};

#ifdef ARCH_X86
// NOTE: SSE needs 16byte alignment or it will segfault 
// 
static complex_t __attribute__((aligned(16))) buf[128];
static float __attribute__((aligned(16))) sseSinCos1c[256];
static float __attribute__((aligned(16))) sseSinCos1d[256];
static float __attribute__((aligned(16))) ps111_1[4]={1,1,1,-1};
//static float __attribute__((aligned(16))) sseW0[4];
static float __attribute__((aligned(16))) sseW1[8];
static float __attribute__((aligned(16))) sseW2[16];
static float __attribute__((aligned(16))) sseW3[32];
static float __attribute__((aligned(16))) sseW4[64];
static float __attribute__((aligned(16))) sseW5[128];
static float __attribute__((aligned(16))) sseW6[256];
static float __attribute__((aligned(16))) *sseW[7]=
	{NULL /*sseW0*/,sseW1,sseW2,sseW3,sseW4,sseW5,sseW6};
static float __attribute__((aligned(16))) sseWindow[512];
#else
static complex_t buf[128];
#endif

/* Twiddle factor LUT */
static complex_t w_1[1];
static complex_t w_2[2];
static complex_t w_4[4];
static complex_t w_8[8];
static complex_t w_16[16];
static complex_t w_32[32];
static complex_t w_64[64];
static complex_t * w[7] = {w_1, w_2, w_4, w_8, w_16, w_32, w_64};

/* Twiddle factors for IMDCT */
static sample_t xcos1[128];
static sample_t xsin1[128];
static sample_t xcos2[64];
static sample_t xsin2[64];

/* Windowing function for Modified DCT - Thank you acroread */
sample_t imdct_window[] = {
	0.00014, 0.00024, 0.00037, 0.00051, 0.00067, 0.00086, 0.00107, 0.00130,
	0.00157, 0.00187, 0.00220, 0.00256, 0.00297, 0.00341, 0.00390, 0.00443,
	0.00501, 0.00564, 0.00632, 0.00706, 0.00785, 0.00871, 0.00962, 0.01061,
	0.01166, 0.01279, 0.01399, 0.01526, 0.01662, 0.01806, 0.01959, 0.02121,
	0.02292, 0.02472, 0.02662, 0.02863, 0.03073, 0.03294, 0.03527, 0.03770,
	0.04025, 0.04292, 0.04571, 0.04862, 0.05165, 0.05481, 0.05810, 0.06153,
	0.06508, 0.06878, 0.07261, 0.07658, 0.08069, 0.08495, 0.08935, 0.09389,
	0.09859, 0.10343, 0.10842, 0.11356, 0.11885, 0.12429, 0.12988, 0.13563,
	0.14152, 0.14757, 0.15376, 0.16011, 0.16661, 0.17325, 0.18005, 0.18699,
	0.19407, 0.20130, 0.20867, 0.21618, 0.22382, 0.23161, 0.23952, 0.24757,
	0.25574, 0.26404, 0.27246, 0.28100, 0.28965, 0.29841, 0.30729, 0.31626,
	0.32533, 0.33450, 0.34376, 0.35311, 0.36253, 0.37204, 0.38161, 0.39126,
	0.40096, 0.41072, 0.42054, 0.43040, 0.44030, 0.45023, 0.46020, 0.47019,
	0.48020, 0.49022, 0.50025, 0.51028, 0.52031, 0.53033, 0.54033, 0.55031,
	0.56026, 0.57019, 0.58007, 0.58991, 0.59970, 0.60944, 0.61912, 0.62873,
	0.63827, 0.64774, 0.65713, 0.66643, 0.67564, 0.68476, 0.69377, 0.70269,
	0.71150, 0.72019, 0.72877, 0.73723, 0.74557, 0.75378, 0.76186, 0.76981,
	0.77762, 0.78530, 0.79283, 0.80022, 0.80747, 0.81457, 0.82151, 0.82831,
	0.83496, 0.84145, 0.84779, 0.85398, 0.86001, 0.86588, 0.87160, 0.87716,
	0.88257, 0.88782, 0.89291, 0.89785, 0.90264, 0.90728, 0.91176, 0.91610,
	0.92028, 0.92432, 0.92822, 0.93197, 0.93558, 0.93906, 0.94240, 0.94560,
	0.94867, 0.95162, 0.95444, 0.95713, 0.95971, 0.96217, 0.96451, 0.96674,
	0.96887, 0.97089, 0.97281, 0.97463, 0.97635, 0.97799, 0.97953, 0.98099,
	0.98236, 0.98366, 0.98488, 0.98602, 0.98710, 0.98811, 0.98905, 0.98994,
	0.99076, 0.99153, 0.99225, 0.99291, 0.99353, 0.99411, 0.99464, 0.99513,
	0.99558, 0.99600, 0.99639, 0.99674, 0.99706, 0.99736, 0.99763, 0.99788,
	0.99811, 0.99831, 0.99850, 0.99867, 0.99882, 0.99895, 0.99908, 0.99919,
	0.99929, 0.99938, 0.99946, 0.99953, 0.99959, 0.99965, 0.99969, 0.99974,
	0.99978, 0.99981, 0.99984, 0.99986, 0.99988, 0.99990, 0.99992, 0.99993,
	0.99994, 0.99995, 0.99996, 0.99997, 0.99998, 0.99998, 0.99998, 0.99999,
	0.99999, 0.99999, 0.99999, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000,
	1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000 };


static inline void swap_cmplx(complex_t *a, complex_t *b)
{
    complex_t tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}



static inline complex_t cmplx_mult(complex_t a, complex_t b)
{
    complex_t ret;

    ret.real = a.real * b.real - a.imag * b.imag;
    ret.imag = a.real * b.imag + a.imag * b.real;

    return ret;
}

void
imdct_do_512(sample_t data[],sample_t delay[], sample_t bias)
{
    int i,k;
    int p,q;
    int m;
    int two_m;
    int two_m_plus_one;

    sample_t tmp_a_i;
    sample_t tmp_a_r;
    sample_t tmp_b_i;
    sample_t tmp_b_r;

    sample_t *data_ptr;
    sample_t *delay_ptr;
    sample_t *window_ptr;
	
    /* 512 IMDCT with source and dest data in 'data' */
	
    /* Pre IFFT complex multiply plus IFFT cmplx conjugate & reordering*/
    for( i=0; i < 128; i++) {
	/* z[i] = (X[256-2*i-1] + j * X[2*i]) * (xcos1[i] + j * xsin1[i]) ; */ 
#ifdef USE_AC3_C
	int j= pm128[i];
#else
	int j= bit_reverse_512[i];
#endif	
	buf[i].real =         (data[256-2*j-1] * xcos1[j])  -  (data[2*j]       * xsin1[j]);
	buf[i].imag = -1.0 * ((data[2*j]       * xcos1[j])  +  (data[256-2*j-1] * xsin1[j]));
    }

    /* FFT Merge */
/* unoptimized variant
    for (m=1; m < 7; m++) {
	if(m)
	    two_m = (1 << m);
	else
	    two_m = 1;

	two_m_plus_one = (1 << (m+1));

	for(i = 0; i < 128; i += two_m_plus_one) {
	    for(k = 0; k < two_m; k++) {
		p = k + i;
		q = p + two_m;
		tmp_a_r = buf[p].real;
		tmp_a_i = buf[p].imag;
		tmp_b_r = buf[q].real * w[m][k].real - buf[q].imag * w[m][k].imag;
		tmp_b_i = buf[q].imag * w[m][k].real + buf[q].real * w[m][k].imag;
		buf[p].real = tmp_a_r + tmp_b_r;
		buf[p].imag =  tmp_a_i + tmp_b_i;
		buf[q].real = tmp_a_r - tmp_b_r;
		buf[q].imag =  tmp_a_i - tmp_b_i;
	    }
	}
    }
*/
#ifdef USE_AC3_C
	fft_128p (&buf[0]);
#else

    /* 1. iteration */
    for(i = 0; i < 128; i += 2) {
	tmp_a_r = buf[i].real;
	tmp_a_i = buf[i].imag;
	tmp_b_r = buf[i+1].real;
	tmp_b_i = buf[i+1].imag;
	buf[i].real = tmp_a_r + tmp_b_r;
	buf[i].imag =  tmp_a_i + tmp_b_i;
	buf[i+1].real = tmp_a_r - tmp_b_r;
	buf[i+1].imag =  tmp_a_i - tmp_b_i;
    }
        
    /* 2. iteration */
	// Note w[1]={{1,0}, {0,-1}}
    for(i = 0; i < 128; i += 4) {
	tmp_a_r = buf[i].real;
	tmp_a_i = buf[i].imag;
	tmp_b_r = buf[i+2].real;
	tmp_b_i = buf[i+2].imag;
	buf[i].real = tmp_a_r + tmp_b_r;
	buf[i].imag =  tmp_a_i + tmp_b_i;
	buf[i+2].real = tmp_a_r - tmp_b_r;
	buf[i+2].imag =  tmp_a_i - tmp_b_i;
	tmp_a_r = buf[i+1].real;
	tmp_a_i = buf[i+1].imag;
	tmp_b_r = buf[i+3].imag;
	tmp_b_i = buf[i+3].real;
	buf[i+1].real = tmp_a_r + tmp_b_r;
	buf[i+1].imag =  tmp_a_i - tmp_b_i;
	buf[i+3].real = tmp_a_r - tmp_b_r;
	buf[i+3].imag =  tmp_a_i + tmp_b_i;
    }

    /* 3. iteration */
    for(i = 0; i < 128; i += 8) {
		tmp_a_r = buf[i].real;
		tmp_a_i = buf[i].imag;
		tmp_b_r = buf[i+4].real;
		tmp_b_i = buf[i+4].imag;
		buf[i].real = tmp_a_r + tmp_b_r;
		buf[i].imag =  tmp_a_i + tmp_b_i;
		buf[i+4].real = tmp_a_r - tmp_b_r;
		buf[i+4].imag =  tmp_a_i - tmp_b_i;
		tmp_a_r = buf[1+i].real;
		tmp_a_i = buf[1+i].imag;
		tmp_b_r = (buf[i+5].real + buf[i+5].imag) * w[2][1].real;
		tmp_b_i = (buf[i+5].imag - buf[i+5].real) * w[2][1].real;
		buf[1+i].real = tmp_a_r + tmp_b_r;
		buf[1+i].imag =  tmp_a_i + tmp_b_i;
		buf[i+5].real = tmp_a_r - tmp_b_r;
		buf[i+5].imag =  tmp_a_i - tmp_b_i;
		tmp_a_r = buf[i+2].real;
		tmp_a_i = buf[i+2].imag;
		tmp_b_r = buf[i+6].imag;
		tmp_b_i = - buf[i+6].real;
		buf[i+2].real = tmp_a_r + tmp_b_r;
		buf[i+2].imag =  tmp_a_i + tmp_b_i;
		buf[i+6].real = tmp_a_r - tmp_b_r;
		buf[i+6].imag =  tmp_a_i - tmp_b_i;
		tmp_a_r = buf[i+3].real;
		tmp_a_i = buf[i+3].imag;
		tmp_b_r = (buf[i+7].real - buf[i+7].imag) * w[2][3].imag;
		tmp_b_i = (buf[i+7].imag + buf[i+7].real) * w[2][3].imag;
		buf[i+3].real = tmp_a_r + tmp_b_r;
		buf[i+3].imag =  tmp_a_i + tmp_b_i;
		buf[i+7].real = tmp_a_r - tmp_b_r;
		buf[i+7].imag =  tmp_a_i - tmp_b_i;
     }
    
    /* 4-7. iterations */
    for (m=3; m < 7; m++) {
        two_m = (1 << m);

	two_m_plus_one = two_m<<1;

	for(i = 0; i < 128; i += two_m_plus_one) {
	    for(k = 0; k < two_m; k++) {
		int p = k + i;
		int q = p + two_m;
		tmp_a_r = buf[p].real;
		tmp_a_i = buf[p].imag;
		tmp_b_r = buf[q].real * w[m][k].real - buf[q].imag * w[m][k].imag;
		tmp_b_i = buf[q].imag * w[m][k].real + buf[q].real * w[m][k].imag;
		buf[p].real = tmp_a_r + tmp_b_r;
		buf[p].imag =  tmp_a_i + tmp_b_i;
		buf[q].real = tmp_a_r - tmp_b_r;
		buf[q].imag =  tmp_a_i - tmp_b_i;
	    }
	}
    }
#endif    
    /* Post IFFT complex multiply  plus IFFT complex conjugate*/
    for( i=0; i < 128; i++) {
	/* y[n] = z[n] * (xcos1[n] + j * xsin1[n]) ; */
	tmp_a_r =        buf[i].real;
	tmp_a_i = -1.0 * buf[i].imag;
	buf[i].real =(tmp_a_r * xcos1[i])  -  (tmp_a_i  * xsin1[i]);
	buf[i].imag =(tmp_a_r * xsin1[i])  +  (tmp_a_i  * xcos1[i]);
    }
	
    data_ptr = data;
    delay_ptr = delay;
    window_ptr = imdct_window;

    /* Window and convert to real valued signal */
    for(i=0; i< 64; i++) { 
	*data_ptr++   = -buf[64+i].imag   * *window_ptr++ + *delay_ptr++ + bias; 
	*data_ptr++   =  buf[64-i-1].real * *window_ptr++ + *delay_ptr++ + bias; 
    }
    
    for(i=0; i< 64; i++) { 
	*data_ptr++  = -buf[i].real       * *window_ptr++ + *delay_ptr++ + bias; 
	*data_ptr++  =  buf[128-i-1].imag * *window_ptr++ + *delay_ptr++ + bias; 
    }
    
    /* The trailing edge of the window goes into the delay line */
    delay_ptr = delay;

    for(i=0; i< 64; i++) { 
	*delay_ptr++  = -buf[64+i].real   * *--window_ptr; 
	*delay_ptr++  =  buf[64-i-1].imag * *--window_ptr; 
    }
    
    for(i=0; i<64; i++) {
	*delay_ptr++  =  buf[i].imag       * *--window_ptr; 
	*delay_ptr++  = -buf[128-i-1].real * *--window_ptr; 
    }
}

#ifdef ARCH_X86
#include "srfftp_3dnow.h"

const i_cmplx_t x_plus_minus_3dnow __attribute__ ((aligned (8))) = { 0x00000000UL, 0x80000000UL }; 
const i_cmplx_t x_minus_plus_3dnow __attribute__ ((aligned (8))) = { 0x80000000UL, 0x00000000UL }; 
const complex_t HSQRT2_3DNOW __attribute__ ((aligned (8))) = { 0.707106781188, 0.707106781188 };

void
imdct_do_512_3dnow(sample_t data[],sample_t delay[], sample_t bias)
{
    int i,k;
    int p,q;
    int m;
    int two_m;
    int two_m_plus_one;

    sample_t tmp_a_i;
    sample_t tmp_a_r;
    sample_t tmp_b_i;
    sample_t tmp_b_r;

    sample_t *data_ptr;
    sample_t *delay_ptr;
    sample_t *window_ptr;
	
    /* 512 IMDCT with source and dest data in 'data' */
	
    /* Pre IFFT complex multiply plus IFFT cmplx conjugate & reordering*/
#if 1
      __asm__ __volatile__ (
	"movq %0, %%mm7\n\t"
	::"m"(x_plus_minus_3dnow)
	:"memory");
	for( i=0; i < 128; i++) {
		int j = pm128[i];
	__asm__ __volatile__ (
		"movd	%1, %%mm0\n\t"
		"movd	%3, %%mm1\n\t"
		"punpckldq %2, %%mm0\n\t" /* mm0 = data[256-2*j-1] | data[2*j]*/
		"punpckldq %4, %%mm1\n\t" /* mm1 = xcos[j] | xsin[j] */
		"movq	%%mm0, %%mm2\n\t"
		"pfmul	%%mm1, %%mm0\n\t"
#ifdef HAVE_3DNOWEX
		"pswapd	%%mm1, %%mm1\n\t"
#else
		"movq %%mm1, %%mm5\n\t"
		"psrlq $32, %%mm1\n\t"
		"punpckldq %%mm5, %%mm1\n\t"
#endif
		"pfmul	%%mm1, %%mm2\n\t"
#ifdef HAVE_3DNOWEX
		"pfpnacc %%mm2, %%mm0\n\t"
#else
		"pxor	%%mm7, %%mm0\n\t"
		"pfacc	%%mm2, %%mm0\n\t"
#endif
		"pxor	%%mm7, %%mm0\n\t"
		"movq	%%mm0, %0"
		:"=m"(buf[i])
		:"m"(data[256-2*j-1]), "m"(data[2*j]), "m"(xcos1[j]), "m"(xsin1[j])
		:"memory"
	);
/*		buf[i].re = (data[256-2*j-1] * xcos1[j] - data[2*j] * xsin1[j]);
		buf[i].im = (data[256-2*j-1] * xsin1[j] + data[2*j] * xcos1[j])*(-1.0);*/
	}
#else
  __asm__ __volatile__ ("femms":::"memory");
    for( i=0; i < 128; i++) {
	/* z[i] = (X[256-2*i-1] + j * X[2*i]) * (xcos1[i] + j * xsin1[i]) ; */ 
	int j= pm128[i];
	buf[i].real =         (data[256-2*j-1] * xcos1[j])  -  (data[2*j]       * xsin1[j]);
	buf[i].imag = -1.0 * ((data[2*j]       * xcos1[j])  +  (data[256-2*j-1] * xsin1[j]));
    }
#endif

    /* FFT Merge */
/* unoptimized variant
    for (m=1; m < 7; m++) {
	if(m)
	    two_m = (1 << m);
	else
	    two_m = 1;

	two_m_plus_one = (1 << (m+1));

	for(i = 0; i < 128; i += two_m_plus_one) {
	    for(k = 0; k < two_m; k++) {
		p = k + i;
		q = p + two_m;
		tmp_a_r = buf[p].real;
		tmp_a_i = buf[p].imag;
		tmp_b_r = buf[q].real * w[m][k].real - buf[q].imag * w[m][k].imag;
		tmp_b_i = buf[q].imag * w[m][k].real + buf[q].real * w[m][k].imag;
		buf[p].real = tmp_a_r + tmp_b_r;
		buf[p].imag =  tmp_a_i + tmp_b_i;
		buf[q].real = tmp_a_r - tmp_b_r;
		buf[q].imag =  tmp_a_i - tmp_b_i;
	    }
	}
    }
*/

    fft_128p_3dnow (&buf[0]);
//    asm volatile ("femms \n\t":::"memory");
    
    /* Post IFFT complex multiply  plus IFFT complex conjugate*/
#if 1  
  __asm__ __volatile__ (
	"movq %0, %%mm7\n\t"
	"movq %1, %%mm6\n\t"
	::"m"(x_plus_minus_3dnow),
	"m"(x_minus_plus_3dnow)
	:"eax","memory");
	for (i=0; i < 128; i++) {
	    __asm__ __volatile__ (
		"movq %1, %%mm0\n\t" /* ac3_buf[i].re | ac3_buf[i].im */
		"movq %%mm0, %%mm1\n\t" /* ac3_buf[i].re | ac3_buf[i].im */
#ifndef HAVE_3DNOWEX
		"movq %%mm1, %%mm2\n\t"
		"psrlq $32, %%mm1\n\t"
		"punpckldq %%mm2, %%mm1\n\t"
#else			 
		"pswapd %%mm1, %%mm1\n\t" /* ac3_buf[i].re | ac3_buf[i].im */
#endif			 
		"movd %3, %%mm3\n\t" /* ac3_xsin[i] */
		"punpckldq %2, %%mm3\n\t" /* ac3_xsin[i] | ac3_xcos[i] */
		"pfmul %%mm3, %%mm0\n\t"
		"pfmul %%mm3, %%mm1\n\t"
#ifndef HAVE_3DNOWEX
		"pxor  %%mm7, %%mm0\n\t"
		"pfacc %%mm1, %%mm0\n\t"
		"movd %%mm0, 4%0\n\t"
		"psrlq $32, %%mm0\n\t"
		"movd %%mm0, %0\n\t"
#else
		"pfpnacc %%mm1, %%mm0\n\t" /* mm0 = mm0[0] - mm0[1] | mm1[0] + mm1[1] */
		"pswapd %%mm0, %%mm0\n\t"
		"movq %%mm0, %0"
#endif
		:"=m"(buf[i])
		:"m"(buf[i]),"m"(xcos1[i]),"m"(xsin1[i])
		:"memory");
/*		ac3_buf[i].re =(tmp_a_r * ac3_xcos1[i])  +  (tmp_a_i  * ac3_xsin1[i]);
		ac3_buf[i].im =(tmp_a_r * ac3_xsin1[i])  -  (tmp_a_i  * ac3_xcos1[i]);*/
	}
#else    
  __asm__ __volatile__ ("femms":::"memory");
    for( i=0; i < 128; i++) {
	/* y[n] = z[n] * (xcos1[n] + j * xsin1[n]) ; */
	tmp_a_r =        buf[i].real;
	tmp_a_i = -1.0 * buf[i].imag;
	buf[i].real =(tmp_a_r * xcos1[i])  -  (tmp_a_i  * xsin1[i]);
	buf[i].imag =(tmp_a_r * xsin1[i])  +  (tmp_a_i  * xcos1[i]);
    }
#endif
	
    data_ptr = data;
    delay_ptr = delay;
    window_ptr = imdct_window;

    /* Window and convert to real valued signal */
#if 1
	asm volatile (
		"movd (%0), %%mm3	\n\t"
		"punpckldq %%mm3, %%mm3	\n\t"
	:: "r" (&bias)
	);
	for (i=0; i< 64; i++) {
/* merge two loops in one to enable working of 2 decoders */
	__asm__ __volatile__ (
		"movd	516(%1), %%mm0\n\t"
		"movd	(%1), %%mm1\n\t" /**data_ptr++=-buf[64+i].im**window_ptr+++*delay_ptr++;*/
		"punpckldq (%2), %%mm0\n\t"/*data_ptr[128]=-buf[i].re*window_ptr[128]+delay_ptr[128];*/
		"punpckldq 516(%2), %%mm1\n\t"
		"pfmul	(%3), %%mm0\n\t"/**data_ptr++=buf[64-i-1].re**window_ptr+++*delay_ptr++;*/
		"pfmul	512(%3), %%mm1\n\t"
		"pxor	%%mm6, %%mm0\n\t"/*data_ptr[128]=buf[128-i-1].im*window_ptr[128]+delay_ptr[128];*/
		"pxor	%%mm6, %%mm1\n\t"
		"pfadd	(%4), %%mm0\n\t"
		"pfadd	512(%4), %%mm1\n\t"
		"pfadd %%mm3, %%mm0\n\t"
		"pfadd %%mm3, %%mm1\n\t"
		"movq	%%mm0, (%0)\n\t"
		"movq	%%mm1, 512(%0)"
		:"=r"(data_ptr)
		:"r"(&buf[i].real), "r"(&buf[64-i-1].real), "r"(window_ptr), "r"(delay_ptr), "0"(data_ptr)
		:"memory");
		data_ptr += 2;
		window_ptr += 2;
		delay_ptr += 2;
	}
	window_ptr += 128;
#else    
  __asm__ __volatile__ ("femms":::"memory");
    for(i=0; i< 64; i++) { 
	*data_ptr++   = -buf[64+i].imag   * *window_ptr++ + *delay_ptr++ + bias; 
	*data_ptr++   =  buf[64-i-1].real * *window_ptr++ + *delay_ptr++ + bias; 
    }
    
    for(i=0; i< 64; i++) { 
	*data_ptr++  = -buf[i].real       * *window_ptr++ + *delay_ptr++ + bias; 
	*data_ptr++  =  buf[128-i-1].imag * *window_ptr++ + *delay_ptr++ + bias; 
    }
#endif

    /* The trailing edge of the window goes into the delay line */
    delay_ptr = delay;
#if 1
	for(i=0; i< 64; i++) {
/* merge two loops in one to enable working of 2 decoders */
	    window_ptr -=2;
	    __asm__ __volatile__(
		"movd	508(%1), %%mm0\n\t"
		"movd	(%1), %%mm1\n\t"
		"punpckldq (%2), %%mm0\n\t"
		"punpckldq 508(%2), %%mm1\n\t"
#ifdef HAVE_3DNOWEX
		"pswapd	(%3), %%mm3\n\t"
		"pswapd	-512(%3), %%mm4\n\t"
#else
		"movq	(%3), %%mm3\n\t"/**delay_ptr++=-buf[64+i].re**--window_ptr;*/
		"movq	-512(%3), %%mm4\n\t"
		"psrlq	$32, %%mm3\n\t"/*delay_ptr[128]=buf[i].im**window_ptr[-512];*/
		"psrlq	$32, %%mm4\n\t"/**delay_ptr++=buf[64-i-1].im**--window_ptr;*/
		"punpckldq (%3), %%mm3\n\t"/*delay_ptr[128]=-buf[128-i-1].re**window_ptr[-512];*/
		"punpckldq -512(%3), %%mm4\n\t"
#endif
		"pfmul	%%mm3, %%mm0\n\t"
		"pfmul	%%mm4, %%mm1\n\t"
		"pxor	%%mm6, %%mm0\n\t"
		"pxor	%%mm7, %%mm1\n\t"
		"movq	%%mm0, (%0)\n\t"
		"movq	%%mm1, 512(%0)"
		:"=r"(delay_ptr)
		:"r"(&buf[i].imag), "r"(&buf[64-i-1].imag), "r"(window_ptr), "0"(delay_ptr)
		:"memory");
		delay_ptr += 2;
	}
  __asm__ __volatile__ ("femms":::"memory");
#else    
  __asm__ __volatile__ ("femms":::"memory");
    for(i=0; i< 64; i++) { 
	*delay_ptr++  = -buf[64+i].real   * *--window_ptr; 
	*delay_ptr++  =  buf[64-i-1].imag * *--window_ptr; 
    }
    
    for(i=0; i<64; i++) {
	*delay_ptr++  =  buf[i].imag       * *--window_ptr; 
	*delay_ptr++  = -buf[128-i-1].real * *--window_ptr; 
    }
#endif    
}


void
imdct_do_512_sse(sample_t data[],sample_t delay[], sample_t bias)
{
    int i,k;
    int p,q;
    int m;
    int two_m;
    int two_m_plus_one;

    sample_t tmp_a_i;
    sample_t tmp_a_r;
    sample_t tmp_b_i;
    sample_t tmp_b_r;

    sample_t *data_ptr;
    sample_t *delay_ptr;
    sample_t *window_ptr;
	
    /* 512 IMDCT with source and dest data in 'data' */
    /* see the c version (dct_do_512()), its allmost identical, just in C */ 

    /* Pre IFFT complex multiply plus IFFT cmplx conjugate */
    /* Bit reversed shuffling */
	asm volatile(
		"xorl %%esi, %%esi			\n\t"
		"leal bit_reverse_512, %%eax		\n\t"
		"movl $1008, %%edi			\n\t"
		"pushl %%ebp				\n\t" //use ebp without telling gcc
		".balign 16				\n\t"
		"1:					\n\t"
		"movlps (%0, %%esi), %%xmm0		\n\t" // XXXI
		"movhps 8(%0, %%edi), %%xmm0		\n\t" // RXXI
		"movlps 8(%0, %%esi), %%xmm1		\n\t" // XXXi
		"movhps (%0, %%edi), %%xmm1		\n\t" // rXXi
		"shufps $0x33, %%xmm1, %%xmm0		\n\t" // irIR
		"movaps sseSinCos1c(%%esi), %%xmm2	\n\t"
		"mulps %%xmm0, %%xmm2			\n\t"
		"shufps $0xB1, %%xmm0, %%xmm0		\n\t" // riRI
		"mulps sseSinCos1d(%%esi), %%xmm0	\n\t"
		"subps %%xmm0, %%xmm2			\n\t"
		"movzbl (%%eax), %%edx			\n\t"
		"movzbl 1(%%eax), %%ebp			\n\t"
		"movlps %%xmm2, (%1, %%edx,8)		\n\t"
		"movhps %%xmm2, (%1, %%ebp,8)		\n\t"
		"addl $16, %%esi			\n\t"
		"addl $2, %%eax				\n\t" // avoid complex addressing for P4 crap
		"subl $16, %%edi			\n\t"
		" jnc 1b				\n\t"
		"popl %%ebp				\n\t"//no we didnt touch ebp *g*
		:: "b" (data), "c" (buf)
		: "%esi", "%edi", "%eax", "%edx"
	);


    /* FFT Merge */
/* unoptimized variant
    for (m=1; m < 7; m++) {
	if(m)
	    two_m = (1 << m);
	else
	    two_m = 1;

	two_m_plus_one = (1 << (m+1));

	for(i = 0; i < 128; i += two_m_plus_one) {
	    for(k = 0; k < two_m; k++) {
		p = k + i;
		q = p + two_m;
		tmp_a_r = buf[p].real;
		tmp_a_i = buf[p].imag;
		tmp_b_r = buf[q].real * w[m][k].real - buf[q].imag * w[m][k].imag;
		tmp_b_i = buf[q].imag * w[m][k].real + buf[q].real * w[m][k].imag;
		buf[p].real = tmp_a_r + tmp_b_r;
		buf[p].imag =  tmp_a_i + tmp_b_i;
		buf[q].real = tmp_a_r - tmp_b_r;
		buf[q].imag =  tmp_a_i - tmp_b_i;
	    }
	}
    }
*/
    
    /* 1. iteration */
	// Note w[0][0]={1,0}
	asm volatile(
		"xorps %%xmm1, %%xmm1	\n\t"
		"xorps %%xmm2, %%xmm2	\n\t"
		"movl %0, %%esi		\n\t"
		".balign 16				\n\t"
		"1:			\n\t"
		"movlps (%%esi), %%xmm0	\n\t" //buf[p]
		"movlps 8(%%esi), %%xmm1\n\t" //buf[q]
		"movhps (%%esi), %%xmm0	\n\t" //buf[p]
		"movhps 8(%%esi), %%xmm2\n\t" //buf[q]
		"addps %%xmm1, %%xmm0	\n\t"
		"subps %%xmm2, %%xmm0	\n\t"
		"movaps %%xmm0, (%%esi)	\n\t"
		"addl $16, %%esi	\n\t"
		"cmpl %1, %%esi		\n\t"
		" jb 1b			\n\t"
		:: "g" (buf), "r" (buf + 128)
		: "%esi"
	);
        
    /* 2. iteration */
	// Note w[1]={{1,0}, {0,-1}}
	asm volatile(
		"movaps ps111_1, %%xmm7		\n\t" // 1,1,1,-1
		"movl %0, %%esi			\n\t"
		".balign 16				\n\t"
		"1:				\n\t"
		"movaps 16(%%esi), %%xmm2	\n\t" //r2,i2,r3,i3
		"shufps $0xB4, %%xmm2, %%xmm2	\n\t" //r2,i2,i3,r3
		"mulps %%xmm7, %%xmm2		\n\t" //r2,i2,i3,-r3
		"movaps (%%esi), %%xmm0		\n\t" //r0,i0,r1,i1
		"movaps (%%esi), %%xmm1		\n\t" //r0,i0,r1,i1
		"addps %%xmm2, %%xmm0		\n\t"
		"subps %%xmm2, %%xmm1		\n\t"
		"movaps %%xmm0, (%%esi)		\n\t"
		"movaps %%xmm1, 16(%%esi)	\n\t"
		"addl $32, %%esi	\n\t"
		"cmpl %1, %%esi		\n\t"
		" jb 1b			\n\t"
		:: "g" (buf), "r" (buf + 128)
		: "%esi"
	);

    /* 3. iteration */
/*
 Note sseW2+0={1,1,sqrt(2),sqrt(2))
 Note sseW2+16={0,0,sqrt(2),-sqrt(2))
 Note sseW2+32={0,0,-sqrt(2),-sqrt(2))
 Note sseW2+48={1,-1,sqrt(2),-sqrt(2))
*/
	asm volatile(
		"movaps 48+sseW2, %%xmm6	\n\t" 
		"movaps 16+sseW2, %%xmm7	\n\t" 
		"xorps %%xmm5, %%xmm5		\n\t"
		"xorps %%xmm2, %%xmm2		\n\t"
		"movl %0, %%esi			\n\t"
		".balign 16			\n\t"
		"1:				\n\t"
		"movaps 32(%%esi), %%xmm2	\n\t" //r4,i4,r5,i5
		"movaps 48(%%esi), %%xmm3	\n\t" //r6,i6,r7,i7
		"movaps sseW2, %%xmm4		\n\t" //r4,i4,r5,i5
		"movaps 32+sseW2, %%xmm5	\n\t" //r6,i6,r7,i7
		"mulps %%xmm2, %%xmm4		\n\t"
		"mulps %%xmm3, %%xmm5		\n\t"
		"shufps $0xB1, %%xmm2, %%xmm2	\n\t" //i4,r4,i5,r5
		"shufps $0xB1, %%xmm3, %%xmm3	\n\t" //i6,r6,i7,r7
		"mulps %%xmm6, %%xmm3		\n\t"
		"mulps %%xmm7, %%xmm2		\n\t"
		"movaps (%%esi), %%xmm0		\n\t" //r0,i0,r1,i1
		"movaps 16(%%esi), %%xmm1	\n\t" //r2,i2,r3,i3
		"addps %%xmm4, %%xmm2		\n\t"
		"addps %%xmm5, %%xmm3		\n\t"
		"movaps %%xmm2, %%xmm4		\n\t"
		"movaps %%xmm3, %%xmm5		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"addps %%xmm1, %%xmm3		\n\t"
		"subps %%xmm4, %%xmm0		\n\t"
		"subps %%xmm5, %%xmm1		\n\t"
		"movaps %%xmm2, (%%esi)		\n\t" 
		"movaps %%xmm3, 16(%%esi)	\n\t" 
		"movaps %%xmm0, 32(%%esi)	\n\t" 
		"movaps %%xmm1, 48(%%esi)	\n\t" 
		"addl $64, %%esi	\n\t"
		"cmpl %1, %%esi		\n\t"
		" jb 1b			\n\t"
		:: "g" (buf), "r" (buf + 128)
		: "%esi"
	);

    /* 4-7. iterations */
    for (m=3; m < 7; m++) {
	two_m = (1 << m);
	two_m_plus_one = two_m<<1;
	asm volatile(
		"movl %0, %%esi				\n\t"
		".balign 16				\n\t"
		"1:					\n\t"
		"xorl %%edi, %%edi			\n\t" // k
		"leal (%%esi, %3), %%edx		\n\t"
		"2:					\n\t"
		"movaps (%%edx, %%edi), %%xmm1		\n\t"
		"movaps (%4, %%edi, 2), %%xmm2		\n\t"
		"mulps %%xmm1, %%xmm2			\n\t"
		"shufps $0xB1, %%xmm1, %%xmm1		\n\t"
		"mulps 16(%4, %%edi, 2), %%xmm1		\n\t"
		"movaps (%%esi, %%edi), %%xmm0		\n\t"
		"addps %%xmm2, %%xmm1			\n\t"
		"movaps %%xmm1, %%xmm2			\n\t"
		"addps %%xmm0, %%xmm1			\n\t"
		"subps %%xmm2, %%xmm0			\n\t"
		"movaps %%xmm1, (%%esi, %%edi)		\n\t"
		"movaps %%xmm0, (%%edx, %%edi)		\n\t"
		"addl $16, %%edi			\n\t"
		"cmpl %3, %%edi				\n\t" //FIXME (opt) count against 0 
		" jb 2b					\n\t"
		"addl %2, %%esi				\n\t"
		"cmpl %1, %%esi				\n\t"
		" jb 1b					\n\t"
		:: "g" (buf), "m" (buf+128), "m" (two_m_plus_one<<3), "r" (two_m<<3),
		   "r" (sseW[m])
		: "%esi", "%edi", "%edx"
	);
    }

    /* Post IFFT complex multiply  plus IFFT complex conjugate*/
	asm volatile(
		"movl $-1024, %%esi				\n\t"
		".balign 16				\n\t"
		"1:					\n\t"
		"movaps (%0, %%esi), %%xmm0		\n\t"
		"movaps (%0, %%esi), %%xmm1		\n\t"
		"shufps $0xB1, %%xmm0, %%xmm0		\n\t"
		"mulps 1024+sseSinCos1c(%%esi), %%xmm1	\n\t"
		"mulps 1024+sseSinCos1d(%%esi), %%xmm0	\n\t"
		"addps %%xmm1, %%xmm0			\n\t"
		"movaps %%xmm0, (%0, %%esi)		\n\t"
		"addl $16, %%esi			\n\t"
		" jnz 1b				\n\t"
		:: "r" (buf+128)
		: "%esi"
	);   

	
    data_ptr = data;
    delay_ptr = delay;
    window_ptr = imdct_window;

    /* Window and convert to real valued signal */
	asm volatile(
		"xorl %%edi, %%edi			\n\t"  // 0
		"xorl %%esi, %%esi			\n\t"  // 0
		"movss %3, %%xmm2			\n\t"  // bias
		"shufps $0x00, %%xmm2, %%xmm2		\n\t"  // bias, bias, ...
		".balign 16				\n\t"
		"1:					\n\t"
		"movlps (%0, %%esi), %%xmm0		\n\t" // ? ? A ?
		"movlps 8(%0, %%esi), %%xmm1		\n\t" // ? ? C ?
		"movhps -16(%0, %%edi), %%xmm1		\n\t" // ? D C ?
		"movhps -8(%0, %%edi), %%xmm0		\n\t" // ? B A ?
		"shufps $0x99, %%xmm1, %%xmm0		\n\t" // D C B A
		"mulps sseWindow(%%esi), %%xmm0		\n\t"
		"addps (%2, %%esi), %%xmm0		\n\t"
		"addps %%xmm2, %%xmm0			\n\t"
		"movaps %%xmm0, (%1, %%esi)		\n\t"
		"addl $16, %%esi			\n\t"
		"subl $16, %%edi			\n\t"
		"cmpl $512, %%esi			\n\t" 
		" jb 1b					\n\t"
		:: "r" (buf+64), "r" (data_ptr), "r" (delay_ptr), "m" (bias)
		: "%esi", "%edi"
	);
	data_ptr+=128;
	delay_ptr+=128;
//	window_ptr+=128;
	
	asm volatile(
		"movl $1024, %%edi			\n\t"  // 512
		"xorl %%esi, %%esi			\n\t"  // 0
		"movss %3, %%xmm2			\n\t"  // bias
		"shufps $0x00, %%xmm2, %%xmm2		\n\t"  // bias, bias, ...
		".balign 16				\n\t"
		"1:					\n\t"
		"movlps (%0, %%esi), %%xmm0		\n\t" // ? ? ? A
		"movlps 8(%0, %%esi), %%xmm1		\n\t" // ? ? ? C
		"movhps -16(%0, %%edi), %%xmm1		\n\t" // D ? ? C
		"movhps -8(%0, %%edi), %%xmm0		\n\t" // B ? ? A
		"shufps $0xCC, %%xmm1, %%xmm0		\n\t" // D C B A
		"mulps 512+sseWindow(%%esi), %%xmm0		\n\t"
		"addps (%2, %%esi), %%xmm0		\n\t"
		"addps %%xmm2, %%xmm0			\n\t"
		"movaps %%xmm0, (%1, %%esi)		\n\t"
		"addl $16, %%esi			\n\t"
		"subl $16, %%edi			\n\t"
		"cmpl $512, %%esi			\n\t" 
		" jb 1b					\n\t"
		:: "r" (buf), "r" (data_ptr), "r" (delay_ptr), "m" (bias)
		: "%esi", "%edi"
	);
	data_ptr+=128;
//	window_ptr+=128;

    /* The trailing edge of the window goes into the delay line */
    delay_ptr = delay;

	asm volatile(
		"xorl %%edi, %%edi			\n\t"  // 0
		"xorl %%esi, %%esi			\n\t"  // 0
		".balign 16				\n\t"
		"1:					\n\t"
		"movlps (%0, %%esi), %%xmm0		\n\t" // ? ? ? A
		"movlps 8(%0, %%esi), %%xmm1		\n\t" // ? ? ? C
		"movhps -16(%0, %%edi), %%xmm1		\n\t" // D ? ? C 
		"movhps -8(%0, %%edi), %%xmm0		\n\t" // B ? ? A 
		"shufps $0xCC, %%xmm1, %%xmm0		\n\t" // D C B A
		"mulps 1024+sseWindow(%%esi), %%xmm0	\n\t"
		"movaps %%xmm0, (%1, %%esi)		\n\t"
		"addl $16, %%esi			\n\t"
		"subl $16, %%edi			\n\t"
		"cmpl $512, %%esi			\n\t" 
		" jb 1b					\n\t"
		:: "r" (buf+64), "r" (delay_ptr)
		: "%esi", "%edi"
	);
	delay_ptr+=128;
//	window_ptr-=128;
	
	asm volatile(
		"movl $1024, %%edi			\n\t"  // 1024
		"xorl %%esi, %%esi			\n\t"  // 0
		".balign 16				\n\t"
		"1:					\n\t"
		"movlps (%0, %%esi), %%xmm0		\n\t" // ? ? A ?
		"movlps 8(%0, %%esi), %%xmm1		\n\t" // ? ? C ?
		"movhps -16(%0, %%edi), %%xmm1		\n\t" // ? D C ? 
		"movhps -8(%0, %%edi), %%xmm0		\n\t" // ? B A ? 
		"shufps $0x99, %%xmm1, %%xmm0		\n\t" // D C B A
		"mulps 1536+sseWindow(%%esi), %%xmm0	\n\t"
		"movaps %%xmm0, (%1, %%esi)		\n\t"
		"addl $16, %%esi			\n\t"
		"subl $16, %%edi			\n\t"
		"cmpl $512, %%esi			\n\t" 
		" jb 1b					\n\t"
		:: "r" (buf), "r" (delay_ptr)
		: "%esi", "%edi"
	);
}
#endif //arch_x86

void
imdct_do_256(sample_t data[],sample_t delay[],sample_t bias)
{
    int i,k;
    int p,q;
    int m;
    int two_m;
    int two_m_plus_one;

    sample_t tmp_a_i;
    sample_t tmp_a_r;
    sample_t tmp_b_i;
    sample_t tmp_b_r;

    sample_t *data_ptr;
    sample_t *delay_ptr;
    sample_t *window_ptr;

    complex_t *buf_1, *buf_2;

    buf_1 = &buf[0];
    buf_2 = &buf[64];

    /* Pre IFFT complex multiply plus IFFT cmplx conjugate */
    for(k=0; k<64; k++) { 
	/* X1[k] = X[2*k]  */
	/* X2[k] = X[2*k+1]     */

	p = 2 * (128-2*k-1);
	q = 2 * (2 * k);

	/* Z1[k] = (X1[128-2*k-1] + j * X1[2*k]) * (xcos2[k] + j * xsin2[k]); */ 
	buf_1[k].real =         data[p] * xcos2[k] - data[q] * xsin2[k];
	buf_1[k].imag = -1.0f * (data[q] * xcos2[k] + data[p] * xsin2[k]); 
	/* Z2[k] = (X2[128-2*k-1] + j * X2[2*k]) * (xcos2[k] + j * xsin2[k]); */ 
	buf_2[k].real =          data[p + 1] * xcos2[k] - data[q + 1] * xsin2[k];
	buf_2[k].imag = -1.0f * ( data[q + 1] * xcos2[k] + data[p + 1] * xsin2[k]); 
    }

    /* IFFT Bit reversed shuffling */
    for(i=0; i<64; i++) { 
	k = bit_reverse_256[i];
	if (k < i) {
	    swap_cmplx(&buf_1[i],&buf_1[k]);
	    swap_cmplx(&buf_2[i],&buf_2[k]);
	}
    }

    /* FFT Merge */
    for (m=0; m < 6; m++) {
	two_m = (1 << m);
	two_m_plus_one = (1 << (m+1));

	/* FIXME */
	if(m)
	    two_m = (1 << m);
	else
	    two_m = 1;

	for(k = 0; k < two_m; k++) {
	    for(i = 0; i < 64; i += two_m_plus_one) {
		p = k + i;
		q = p + two_m;
		/* Do block 1 */
		tmp_a_r = buf_1[p].real;
		tmp_a_i = buf_1[p].imag;
		tmp_b_r = buf_1[q].real * w[m][k].real - buf_1[q].imag * w[m][k].imag;
		tmp_b_i = buf_1[q].imag * w[m][k].real + buf_1[q].real * w[m][k].imag;
		buf_1[p].real = tmp_a_r + tmp_b_r;
		buf_1[p].imag =  tmp_a_i + tmp_b_i;
		buf_1[q].real = tmp_a_r - tmp_b_r;
		buf_1[q].imag =  tmp_a_i - tmp_b_i;

		/* Do block 2 */
		tmp_a_r = buf_2[p].real;
		tmp_a_i = buf_2[p].imag;
		tmp_b_r = buf_2[q].real * w[m][k].real - buf_2[q].imag * w[m][k].imag;
		tmp_b_i = buf_2[q].imag * w[m][k].real + buf_2[q].real * w[m][k].imag;
		buf_2[p].real = tmp_a_r + tmp_b_r;
		buf_2[p].imag =  tmp_a_i + tmp_b_i;
		buf_2[q].real = tmp_a_r - tmp_b_r;
		buf_2[q].imag =  tmp_a_i - tmp_b_i;
	    }
	}
    }

    /* Post IFFT complex multiply */
    for( i=0; i < 64; i++) {
	/* y1[n] = z1[n] * (xcos2[n] + j * xs in2[n]) ; */ 
	tmp_a_r =  buf_1[i].real;
	tmp_a_i = -buf_1[i].imag;
	buf_1[i].real =(tmp_a_r * xcos2[i])  -  (tmp_a_i  * xsin2[i]);
	buf_1[i].imag =(tmp_a_r * xsin2[i])  +  (tmp_a_i  * xcos2[i]);
	/* y2[n] = z2[n] * (xcos2[n] + j * xsin2[n]) ; */ 
	tmp_a_r =  buf_2[i].real;
	tmp_a_i = -buf_2[i].imag;
	buf_2[i].real =(tmp_a_r * xcos2[i])  -  (tmp_a_i  * xsin2[i]);
	buf_2[i].imag =(tmp_a_r * xsin2[i])  +  (tmp_a_i  * xcos2[i]);
    }
	
    data_ptr = data;
    delay_ptr = delay;
    window_ptr = imdct_window;

    /* Window and convert to real valued signal */
    for(i=0; i< 64; i++) { 
	*data_ptr++  = -buf_1[i].imag      * *window_ptr++ + *delay_ptr++ + bias;
	*data_ptr++  =  buf_1[64-i-1].real * *window_ptr++ + *delay_ptr++ + bias;
    }

    for(i=0; i< 64; i++) {
	*data_ptr++  = -buf_1[i].real      * *window_ptr++ + *delay_ptr++ + bias;
	*data_ptr++  =  buf_1[64-i-1].imag * *window_ptr++ + *delay_ptr++ + bias;
    }
	
    delay_ptr = delay;

    for(i=0; i< 64; i++) {
	*delay_ptr++ = -buf_2[i].real      * *--window_ptr;
	*delay_ptr++ =  buf_2[64-i-1].imag * *--window_ptr;
    }

    for(i=0; i< 64; i++) {
	*delay_ptr++ =  buf_2[i].imag      * *--window_ptr;
	*delay_ptr++ = -buf_2[64-i-1].real * *--window_ptr;
    }
}

void imdct_init (uint32_t mm_accel)
{
#ifdef LIBA52_MLIB
    if (mm_accel & MM_ACCEL_MLIB) {
        fprintf (stderr, "Using mlib for IMDCT transform\n");
	imdct_512 = imdct_do_512_mlib;
	imdct_256 = imdct_do_256_mlib;
    } else
#endif
    {
	int i, j, k;

	if(mm_accel & MM_ACCEL_X86_SSE) 	fprintf (stderr, "Using SSE optimized IMDCT transform\n");
	else if(mm_accel & MM_ACCEL_X86_3DNOW) 	fprintf (stderr, "Using 3DNow optimized IMDCT transform\n");
	else		    			fprintf (stderr, "No accelerated IMDCT transform found\n");

	/* Twiddle factors to turn IFFT into IMDCT */
	for (i = 0; i < 128; i++) {
	    xcos1[i] = -cos ((M_PI / 2048) * (8 * i + 1));
	    xsin1[i] = -sin ((M_PI / 2048) * (8 * i + 1));
	}
#ifdef ARCH_X86
	for (i = 0; i < 128; i++) {
	    sseSinCos1c[2*i+0]= xcos1[i];
	    sseSinCos1c[2*i+1]= -xcos1[i];
	    sseSinCos1d[2*i+0]= xsin1[i];
	    sseSinCos1d[2*i+1]= xsin1[i];	
	}
#endif

	/* More twiddle factors to turn IFFT into IMDCT */
	for (i = 0; i < 64; i++) {
	    xcos2[i] = -cos ((M_PI / 1024) * (8 * i + 1));
	    xsin2[i] = -sin ((M_PI / 1024) * (8 * i + 1));
	}

	for (i = 0; i < 7; i++) {
	    j = 1 << i;
	    for (k = 0; k < j; k++) {
		w[i][k].real = cos (-M_PI * k / j);
		w[i][k].imag = sin (-M_PI * k / j);
	    }
	}
#ifdef ARCH_X86
	for (i = 1; i < 7; i++) {
	    j = 1 << i;
	    for (k = 0; k < j; k+=2) {
	    
	    	sseW[i][4*k + 0] = w[i][k+0].real;
	    	sseW[i][4*k + 1] = w[i][k+0].real;
	    	sseW[i][4*k + 2] = w[i][k+1].real;
	    	sseW[i][4*k + 3] = w[i][k+1].real;

	    	sseW[i][4*k + 4] = -w[i][k+0].imag;
	    	sseW[i][4*k + 5] = w[i][k+0].imag;
	    	sseW[i][4*k + 6] = -w[i][k+1].imag;
	    	sseW[i][4*k + 7] = w[i][k+1].imag;	    
	    	
	//we multiply more or less uninitalized numbers so we need to use exactly 0.0
		if(k==0)
		{
//			sseW[i][4*k + 0]= sseW[i][4*k + 1]= 1.0;
			sseW[i][4*k + 4]= sseW[i][4*k + 5]= 0.0;
		}
		
		if(2*k == j)
		{
			sseW[i][4*k + 0]= sseW[i][4*k + 1]= 0.0;
//			sseW[i][4*k + 4]= -(sseW[i][4*k + 5]= -1.0);
		}
	    }
	}

	for(i=0; i<128; i++)
	{
		sseWindow[2*i+0]= -imdct_window[2*i+0];
		sseWindow[2*i+1]=  imdct_window[2*i+1];	
	}
	
	for(i=0; i<64; i++)
	{
		sseWindow[256 + 2*i+0]= -imdct_window[254 - 2*i+1];
		sseWindow[256 + 2*i+1]=  imdct_window[254 - 2*i+0];
		sseWindow[384 + 2*i+0]=  imdct_window[126 - 2*i+1];
		sseWindow[384 + 2*i+1]= -imdct_window[126 - 2*i+0];
	}
#endif // arch_x86

	imdct_512 = imdct_do_512;
#ifdef ARCH_X86
	if(mm_accel & MM_ACCEL_X86_SSE)		imdct_512 = imdct_do_512_sse;
	else if(mm_accel & MM_ACCEL_X86_3DNOW)	imdct_512 = imdct_do_512_3dnow;
#endif // arch_x86
	imdct_256 = imdct_do_256;
    }
}

// Stuff below this line is borrowed from libac3
#include "srfftp.h"

#ifdef ARCH_X86

static void fft_4_3dnow(complex_t *x)
{
  /* delta_p = 1 here */
  /* x[k] = sum_{i=0..3} x[i] * w^{i*k}, w=e^{-2*pi/4} 
   */
  __asm__ __volatile__(
	"movq	24(%1), %%mm3\n\t"
	"movq	8(%1), %%mm1\n\t"
	"pxor	%2, %%mm3\n\t" /* mm3.re | -mm3.im */
	"pxor   %3, %%mm1\n\t" /* -mm1.re | mm1.im */
	"pfadd	%%mm1, %%mm3\n\t" /* vi.im = x[3].re - x[1].re; */
	"movq	%%mm3, %%mm4\n\t" /* vi.re =-x[3].im + x[1].im; mm4 = vi */
#ifdef HAVE_3DNOWEX
	"pswapd %%mm4, %%mm4\n\t"
#else
	"movq   %%mm4, %%mm5\n\t"
	"psrlq	$32, %%mm4\n\t"
	"punpckldq %%mm5, %%mm4\n\t"
#endif
	"movq	(%1), %%mm5\n\t" /* yb.re = x[0].re - x[2].re; */
	"movq	(%1), %%mm6\n\t" /* yt.re = x[0].re + x[2].re; */
	"movq	24(%1), %%mm7\n\t" /* u.re  = x[3].re + x[1].re; */
	"pfsub	16(%1), %%mm5\n\t" /* yb.im = x[0].im - x[2].im; mm5 = yb */
	"pfadd	16(%1), %%mm6\n\t" /* yt.im = x[0].im + x[2].im; mm6 = yt */
	"pfadd	8(%1), %%mm7\n\t" /* u.im  = x[3].im + x[1].im; mm7 = u */

	"movq	%%mm6, %%mm0\n\t" /* x[0].re = yt.re + u.re; */
	"movq	%%mm5, %%mm1\n\t" /* x[1].re = yb.re + vi.re; */
	"pfadd	%%mm7, %%mm0\n\t" /*x[0].im = yt.im + u.im; */
	"pfadd	%%mm4, %%mm1\n\t" /* x[1].im = yb.im + vi.im; */
	"movq	%%mm0, (%0)\n\t"
	"movq	%%mm1, 8(%0)\n\t"

	"pfsub	%%mm7, %%mm6\n\t" /* x[2].re = yt.re - u.re; */
	"pfsub	%%mm4, %%mm5\n\t" /* x[3].re = yb.re - vi.re; */
	"movq	%%mm6, 16(%0)\n\t" /* x[2].im = yt.im - u.im; */
	"movq	%%mm5, 24(%0)" /* x[3].im = yb.im - vi.im; */
	:"=r"(x)
	:"0"(x),
	 "m"(x_plus_minus_3dnow),
	 "m"(x_minus_plus_3dnow)
	:"memory");
}

static void fft_8_3dnow(complex_t *x)
{
  /* delta_p = diag{1, sqrt(i)} here */
  /* x[k] = sum_{i=0..7} x[i] * w^{i*k}, w=e^{-2*pi/8} 
   */
  complex_t wT1, wB1, wB2;
  
  __asm__ __volatile__(
	"movq	8(%2), %%mm0\n\t"
	"movq	24(%2), %%mm1\n\t"
	"movq	%%mm0, %0\n\t"  /* wT1 = x[1]; */
	"movq	%%mm1, %1\n\t" /* wB1 = x[3]; */
	:"=m"(wT1), "=m"(wB1)
	:"r"(x)
	:"memory");

  __asm__ __volatile__(
	"movq	16(%0), %%mm2\n\t"
	"movq	32(%0), %%mm3\n\t"
	"movq	%%mm2, 8(%0)\n\t"  /* x[1] = x[2]; */
	"movq	48(%0), %%mm4\n\t"
	"movq	%%mm3, 16(%0)\n\t" /* x[2] = x[4]; */
	"movq	%%mm4, 24(%0)\n\t" /* x[3] = x[6]; */
	:"=r"(x)
	:"0"(x)
	:"memory");

  fft_4_3dnow(&x[0]);
  
  /* x[0] x[4] x[2] x[6] */
  
  __asm__ __volatile__(
      "movq	40(%1), %%mm0\n\t"
      "movq	%%mm0,	%%mm3\n\t"
      "movq	56(%1),	%%mm1\n\t"
      "pfadd	%%mm1,	%%mm0\n\t"
      "pfsub	%%mm1,	%%mm3\n\t"
      "movq	(%2),	%%mm2\n\t"
      "pfadd	%%mm2,	%%mm0\n\t"
      "pfadd	%%mm2,	%%mm3\n\t"
      "movq	(%3),	%%mm1\n\t"
      "pfadd	%%mm1,	%%mm0\n\t"
      "pfsub	%%mm1,	%%mm3\n\t"
      "movq	(%1),	%%mm1\n\t"
      "movq	16(%1),	%%mm4\n\t"
      "movq	%%mm1,	%%mm2\n\t"
#ifdef HAVE_3DNOWEX
      "pswapd	%%mm3,	%%mm3\n\t"
#else
      "movq	%%mm3,	%%mm6\n\t"
      "psrlq	$32,	%%mm3\n\t"
      "punpckldq %%mm6,	%%mm3\n\t"
#endif
      "pfadd	%%mm0,	%%mm1\n\t"
      "movq	%%mm4,	%%mm5\n\t"
      "pfsub	%%mm0,	%%mm2\n\t"
      "pfadd	%%mm3,	%%mm4\n\t"
      "movq	%%mm1,	(%0)\n\t"
      "pfsub	%%mm3,	%%mm5\n\t"
      "movq	%%mm2,	32(%0)\n\t"
      "movd	%%mm4,	16(%0)\n\t"
      "movd	%%mm5,	48(%0)\n\t"
      "psrlq	$32, %%mm4\n\t"
      "psrlq	$32, %%mm5\n\t"
      "movd	%%mm4,	52(%0)\n\t"
      "movd	%%mm5,	20(%0)"
      :"=r"(x)
      :"0"(x), "r"(&wT1), "r"(&wB1)
      :"memory");
  
  /* x[1] x[5] */
  __asm__ __volatile__ (
	"movq	%6,	%%mm6\n\t"
	"movq	%5,	%%mm7\n\t"
	"movq	%1,	%%mm0\n\t"
	"movq	%2,	%%mm1\n\t"
	"movq	56(%3),	%%mm3\n\t"
	"pfsub	40(%3),	%%mm0\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%%mm1,	%%mm1\n\t"
#else
	"movq	%%mm1,	%%mm2\n\t"
	"psrlq	$32,	%%mm1\n\t"
	"punpckldq %%mm2,%%mm1\n\t"
#endif
	"pxor	%%mm7,	%%mm1\n\t"
	"pfadd	%%mm1,	%%mm0\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%%mm3,	%%mm3\n\t"
#else
	"movq	%%mm3,	%%mm2\n\t"
	"psrlq	$32,	%%mm3\n\t"
	"punpckldq %%mm2,%%mm3\n\t"
#endif
	"pxor	%%mm6,	%%mm3\n\t"
	"pfadd	%%mm3,	%%mm0\n\t"
	"movq	%%mm0,	%%mm1\n\t"
	"pxor	%%mm6,	%%mm1\n\t"
	"pfacc	%%mm1,	%%mm0\n\t"
	"pfmul	%4,	%%mm0\n\t"
	
	"movq	40(%3),	%%mm5\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%%mm5,	%%mm5\n\t"
#else
	"movq	%%mm5,	%%mm1\n\t"
	"psrlq	$32,	%%mm5\n\t"
	"punpckldq %%mm1,%%mm5\n\t"
#endif
	"movq	%%mm5,	%0\n\t"
	
	"movq	8(%3),	%%mm1\n\t"
	"movq	%%mm1,	%%mm2\n\t"
	"pfsub	%%mm0,	%%mm1\n\t"
	"pfadd	%%mm0,	%%mm2\n\t"
	"movq	%%mm1,	40(%3)\n\t"
	"movq	%%mm2,	8(%3)\n\t"
	:"=m"(wB2)
	:"m"(wT1), "m"(wB1), "r"(x), "m"(HSQRT2_3DNOW), 
	 "m"(x_plus_minus_3dnow), "m"(x_minus_plus_3dnow)
	:"memory");


  /* x[3] x[7] */
  __asm__ __volatile__(
	"movq	%1,	%%mm0\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%3,	%%mm1\n\t"
#else
	"movq	%3,	%%mm1\n\t"
	"psrlq	$32,	%%mm1\n\t"
	"punpckldq %3,	%%mm1\n\t"
#endif
	"pxor	%%mm6,	%%mm1\n\t"	
	"pfadd	%%mm1,	%%mm0\n\t"
	"movq	%2,	%%mm2\n\t"
	"movq	56(%4),	%%mm3\n\t"
	"pxor	%%mm7,	%%mm3\n\t"
	"pfadd	%%mm3,	%%mm2\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%%mm2,	%%mm2\n\t"
#else
	"movq	%%mm2,	%%mm5\n\t"
	"psrlq	$32,	%%mm2\n\t"
	"punpckldq %%mm5,%%mm2\n\t"
#endif
	"movq	24(%4),	%%mm3\n\t"
	"pfsub	%%mm2,	%%mm0\n\t"
	"movq	%%mm3,	%%mm4\n\t"
	"movq	%%mm0,	%%mm1\n\t"
	"pxor	%%mm6,	%%mm0\n\t"
	"pfacc	%%mm1,	%%mm0\n\t"
	"pfmul	%5,	%%mm0\n\t"
	"movq	%%mm0,	%%mm1\n\t"
	"pxor	%%mm6,	%%mm1\n\t"
	"pxor	%%mm7,	%%mm0\n\t"
	"pfadd	%%mm1,	%%mm3\n\t"
	"pfadd	%%mm0,	%%mm4\n\t"
	"movq	%%mm4,	24(%0)\n\t"
	"movq	%%mm3,	56(%0)\n\t"
	:"=r"(x)
	:"m"(wT1), "m"(wB2), "m"(wB1), "0"(x), "m"(HSQRT2_3DNOW)
	:"memory");
}

static void fft_asmb_3dnow(int k, complex_t *x, complex_t *wTB,
		     const complex_t *d, const complex_t *d_3)
{
  register complex_t  *x2k, *x3k, *x4k, *wB;

  TRANS_FILL_MM6_MM7_3DNOW();
  x2k = x + 2 * k;
  x3k = x2k + 2 * k;
  x4k = x3k + 2 * k;
  wB = wTB + 2 * k;
  
  TRANSZERO_3DNOW(x[0],x2k[0],x3k[0],x4k[0]);
  TRANS_3DNOW(x[1],x2k[1],x3k[1],x4k[1],wTB[1],wB[1],d[1],d_3[1]);
  
  --k;
  for(;;) {
     TRANS_3DNOW(x[2],x2k[2],x3k[2],x4k[2],wTB[2],wB[2],d[2],d_3[2]);
     TRANS_3DNOW(x[3],x2k[3],x3k[3],x4k[3],wTB[3],wB[3],d[3],d_3[3]);
     if (!--k) break;
     x += 2;
     x2k += 2;
     x3k += 2;
     x4k += 2;
     d += 2;
     d_3 += 2;
     wTB += 2;
     wB += 2;
  }
 
}

void fft_asmb16_3dnow(complex_t *x, complex_t *wTB)
{
  int k = 2;

  TRANS_FILL_MM6_MM7_3DNOW();
  /* transform x[0], x[8], x[4], x[12] */
  TRANSZERO_3DNOW(x[0],x[4],x[8],x[12]);

  /* transform x[1], x[9], x[5], x[13] */
  TRANS_3DNOW(x[1],x[5],x[9],x[13],wTB[1],wTB[5],delta16[1],delta16_3[1]);

  /* transform x[2], x[10], x[6], x[14] */
  TRANSHALF_16_3DNOW(x[2],x[6],x[10],x[14]);

  /* transform x[3], x[11], x[7], x[15] */
  TRANS_3DNOW(x[3],x[7],x[11],x[15],wTB[3],wTB[7],delta16[3],delta16_3[3]);

} 

static void fft_128p_3dnow(complex_t *a)
{
  fft_8_3dnow(&a[0]); fft_4_3dnow(&a[8]); fft_4_3dnow(&a[12]);
  fft_asmb16_3dnow(&a[0], &a[8]);
  
  fft_8_3dnow(&a[16]), fft_8_3dnow(&a[24]);
  fft_asmb_3dnow(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

  fft_8_3dnow(&a[32]); fft_4_3dnow(&a[40]); fft_4_3dnow(&a[44]);
  fft_asmb16_3dnow(&a[32], &a[40]);

  fft_8_3dnow(&a[48]); fft_4_3dnow(&a[56]); fft_4_3dnow(&a[60]);
  fft_asmb16_3dnow(&a[48], &a[56]);

  fft_asmb_3dnow(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);

  fft_8_3dnow(&a[64]); fft_4_3dnow(&a[72]); fft_4_3dnow(&a[76]);
  /* fft_16(&a[64]); */
  fft_asmb16_3dnow(&a[64], &a[72]);

  fft_8_3dnow(&a[80]); fft_8_3dnow(&a[88]);
  
  /* fft_32(&a[64]); */
  fft_asmb_3dnow(4, &a[64], &a[80],&delta32[0], &delta32_3[0]);

  fft_8_3dnow(&a[96]); fft_4_3dnow(&a[104]), fft_4_3dnow(&a[108]);
  /* fft_16(&a[96]); */
  fft_asmb16_3dnow(&a[96], &a[104]);

  fft_8_3dnow(&a[112]), fft_8_3dnow(&a[120]);
  /* fft_32(&a[96]); */
  fft_asmb_3dnow(4, &a[96], &a[112], &delta32[0], &delta32_3[0]);
  
  /* fft_128(&a[0]); */
  fft_asmb_3dnow(16, &a[0], &a[64], &delta128[0], &delta128_3[0]);
}
#endif //ARCH_X86

static void fft_asmb(int k, complex_t *x, complex_t *wTB,
	     const complex_t *d, const complex_t *d_3)
{
  register complex_t  *x2k, *x3k, *x4k, *wB;
  register float a_r, a_i, a1_r, a1_i, u_r, u_i, v_r, v_i;

  x2k = x + 2 * k;
  x3k = x2k + 2 * k;
  x4k = x3k + 2 * k;
  wB = wTB + 2 * k;
  
  TRANSZERO(x[0],x2k[0],x3k[0],x4k[0]);
  TRANS(x[1],x2k[1],x3k[1],x4k[1],wTB[1],wB[1],d[1],d_3[1]);
  
  --k;
  for(;;) {
     TRANS(x[2],x2k[2],x3k[2],x4k[2],wTB[2],wB[2],d[2],d_3[2]);
     TRANS(x[3],x2k[3],x3k[3],x4k[3],wTB[3],wB[3],d[3],d_3[3]);
     if (!--k) break;
     x += 2;
     x2k += 2;
     x3k += 2;
     x4k += 2;
     d += 2;
     d_3 += 2;
     wTB += 2;
     wB += 2;
  }
 
}

static void fft_asmb16(complex_t *x, complex_t *wTB)
{
  register float a_r, a_i, a1_r, a1_i, u_r, u_i, v_r, v_i;
  int k = 2;

  /* transform x[0], x[8], x[4], x[12] */
  TRANSZERO(x[0],x[4],x[8],x[12]);

  /* transform x[1], x[9], x[5], x[13] */
  TRANS(x[1],x[5],x[9],x[13],wTB[1],wTB[5],delta16[1],delta16_3[1]);

  /* transform x[2], x[10], x[6], x[14] */
  TRANSHALF_16(x[2],x[6],x[10],x[14]);

  /* transform x[3], x[11], x[7], x[15] */
  TRANS(x[3],x[7],x[11],x[15],wTB[3],wTB[7],delta16[3],delta16_3[3]);

} 

static void fft_4(complex_t *x)
{
  /* delta_p = 1 here */
  /* x[k] = sum_{i=0..3} x[i] * w^{i*k}, w=e^{-2*pi/4} 
   */

  register float yt_r, yt_i, yb_r, yb_i, u_r, u_i, vi_r, vi_i;
  
  yt_r = x[0].real;
  yb_r = yt_r - x[2].real;
  yt_r += x[2].real;

  u_r = x[1].real;
  vi_i = x[3].real - u_r;
  u_r += x[3].real;
  
  u_i = x[1].imag;
  vi_r = u_i - x[3].imag;
  u_i += x[3].imag;

  yt_i = yt_r;
  yt_i += u_r;
  x[0].real = yt_i;
  yt_r -= u_r;
  x[2].real = yt_r;
  yt_i = yb_r;
  yt_i += vi_r;
  x[1].real = yt_i;
  yb_r -= vi_r;
  x[3].real = yb_r;

  yt_i = x[0].imag;
  yb_i = yt_i - x[2].imag;
  yt_i += x[2].imag;

  yt_r = yt_i;
  yt_r += u_i;
  x[0].imag = yt_r;
  yt_i -= u_i;
  x[2].imag = yt_i;
  yt_r = yb_i;
  yt_r += vi_i;
  x[1].imag = yt_r;
  yb_i -= vi_i;
  x[3].imag = yb_i;
}


static void fft_8(complex_t *x)
{
  /* delta_p = diag{1, sqrt(i)} here */
  /* x[k] = sum_{i=0..7} x[i] * w^{i*k}, w=e^{-2*pi/8} 
   */
  register float wT1_r, wT1_i, wB1_r, wB1_i, wT2_r, wT2_i, wB2_r, wB2_i;
  
  wT1_r = x[1].real;
  wT1_i = x[1].imag;
  wB1_r = x[3].real;
  wB1_i = x[3].imag;

  x[1] = x[2];
  x[2] = x[4];
  x[3] = x[6];
  fft_4(&x[0]);

  
  /* x[0] x[4] */
  wT2_r = x[5].real;
  wT2_r += x[7].real;
  wT2_r += wT1_r;
  wT2_r += wB1_r;
  wT2_i = wT2_r;
  wT2_r += x[0].real;
  wT2_i = x[0].real - wT2_i;
  x[0].real = wT2_r;
  x[4].real = wT2_i;

  wT2_i = x[5].imag;
  wT2_i += x[7].imag;
  wT2_i += wT1_i;
  wT2_i += wB1_i;
  wT2_r = wT2_i;
  wT2_r += x[0].imag;
  wT2_i = x[0].imag - wT2_i;
  x[0].imag = wT2_r;
  x[4].imag = wT2_i;
  
  /* x[2] x[6] */
  wT2_r = x[5].imag;
  wT2_r -= x[7].imag;
  wT2_r += wT1_i;
  wT2_r -= wB1_i;
  wT2_i = wT2_r;
  wT2_r += x[2].real;
  wT2_i = x[2].real - wT2_i;
  x[2].real = wT2_r;
  x[6].real = wT2_i;

  wT2_i = x[5].real;
  wT2_i -= x[7].real;
  wT2_i += wT1_r;
  wT2_i -= wB1_r;
  wT2_r = wT2_i;
  wT2_r += x[2].imag;
  wT2_i = x[2].imag - wT2_i;
  x[2].imag = wT2_i;
  x[6].imag = wT2_r;
  

  /* x[1] x[5] */
  wT2_r = wT1_r;
  wT2_r += wB1_i;
  wT2_r -= x[5].real;
  wT2_r -= x[7].imag;
  wT2_i = wT1_i;
  wT2_i -= wB1_r;
  wT2_i -= x[5].imag;
  wT2_i += x[7].real;

  wB2_r = wT2_r;
  wB2_r += wT2_i;
  wT2_i -= wT2_r;
  wB2_r *= HSQRT2;
  wT2_i *= HSQRT2;
  wT2_r = wB2_r;
  wB2_r += x[1].real;
  wT2_r =  x[1].real - wT2_r;

  wB2_i = x[5].real;
  x[1].real = wB2_r;
  x[5].real = wT2_r;

  wT2_r = wT2_i;
  wT2_r += x[1].imag;
  wT2_i = x[1].imag - wT2_i;
  wB2_r = x[5].imag;
  x[1].imag = wT2_r;
  x[5].imag = wT2_i;

  /* x[3] x[7] */
  wT1_r -= wB1_i;
  wT1_i += wB1_r;
  wB1_r = wB2_i - x[7].imag;
  wB1_i = wB2_r + x[7].real;
  wT1_r -= wB1_r;
  wT1_i -= wB1_i;
  wB1_r = wT1_r + wT1_i;
  wB1_r *= HSQRT2;
  wT1_i -= wT1_r;
  wT1_i *= HSQRT2;
  wB2_r = x[3].real;
  wB2_i = wB2_r + wT1_i;
  wB2_r -= wT1_i;
  x[3].real = wB2_i;
  x[7].real = wB2_r;
  wB2_i = x[3].imag;
  wB2_r = wB2_i + wB1_r;
  wB2_i -= wB1_r;
  x[3].imag = wB2_i;
  x[7].imag = wB2_r;
}


static void fft_128p(complex_t *a)
{
  fft_8(&a[0]); fft_4(&a[8]); fft_4(&a[12]);
  fft_asmb16(&a[0], &a[8]);
  
  fft_8(&a[16]), fft_8(&a[24]);
  fft_asmb(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

  fft_8(&a[32]); fft_4(&a[40]); fft_4(&a[44]);
  fft_asmb16(&a[32], &a[40]);

  fft_8(&a[48]); fft_4(&a[56]); fft_4(&a[60]);
  fft_asmb16(&a[48], &a[56]);

  fft_asmb(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);

  fft_8(&a[64]); fft_4(&a[72]); fft_4(&a[76]);
  /* fft_16(&a[64]); */
  fft_asmb16(&a[64], &a[72]);

  fft_8(&a[80]); fft_8(&a[88]);
  
  /* fft_32(&a[64]); */
  fft_asmb(4, &a[64], &a[80],&delta32[0], &delta32_3[0]);

  fft_8(&a[96]); fft_4(&a[104]), fft_4(&a[108]);
  /* fft_16(&a[96]); */
  fft_asmb16(&a[96], &a[104]);

  fft_8(&a[112]), fft_8(&a[120]);
  /* fft_32(&a[96]); */
  fft_asmb(4, &a[96], &a[112], &delta32[0], &delta32_3[0]);
  
  /* fft_128(&a[0]); */
  fft_asmb(16, &a[0], &a[64], &delta128[0], &delta128_3[0]);
}



