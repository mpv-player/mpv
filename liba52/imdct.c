/*
 * imdct.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * The ifft algorithms in this file have been largely inspired by Dan
 * Bernstein's work, djbfft, available at http://cr.yp.to/djbfft.html
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * Modified for use with MPlayer, changes contained in liba52_changes.diff.
 * detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
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
 * AltiVec optimizations from Romain Dolbeau (romain@dolbeau.org)
 */

#include "config.h"

#include <math.h>
#include <stdio.h>
#ifdef LIBA52_DJBFFT
#include <fftc4.h>
#endif
#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795029
#endif
#include <inttypes.h>

#include "a52.h"
#include "a52_internal.h"
#include "mm_accel.h"
#include "mangle.h"

void (*a52_imdct_512) (sample_t * data, sample_t * delay, sample_t bias);

#ifdef RUNTIME_CPUDETECT
#undef HAVE_AMD3DNOWEXT
#define HAVE_AMD3DNOWEXT 0
#endif

typedef struct complex_s {
    sample_t real;
    sample_t imag;
} complex_t;

static const int pm128[128] attribute_used __attribute__((aligned(16))) =
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

static uint8_t attribute_used bit_reverse_512[] = {
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

static uint8_t fftorder[] = {
      0,128, 64,192, 32,160,224, 96, 16,144, 80,208,240,112, 48,176,
      8,136, 72,200, 40,168,232,104,248,120, 56,184, 24,152,216, 88,
      4,132, 68,196, 36,164,228,100, 20,148, 84,212,244,116, 52,180,
    252,124, 60,188, 28,156,220, 92, 12,140, 76,204,236,108, 44,172,
      2,130, 66,194, 34,162,226, 98, 18,146, 82,210,242,114, 50,178,
     10,138, 74,202, 42,170,234,106,250,122, 58,186, 26,154,218, 90,
    254,126, 62,190, 30,158,222, 94, 14,142, 78,206,238,110, 46,174,
      6,134, 70,198, 38,166,230,102,246,118, 54,182, 22,150,214, 86
};

static complex_t __attribute__((aligned(16))) buf[128];

/* Twiddle factor LUT */
static complex_t __attribute__((aligned(16))) w_1[1];
static complex_t __attribute__((aligned(16))) w_2[2];
static complex_t __attribute__((aligned(16))) w_4[4];
static complex_t __attribute__((aligned(16))) w_8[8];
static complex_t __attribute__((aligned(16))) w_16[16];
static complex_t __attribute__((aligned(16))) w_32[32];
static complex_t __attribute__((aligned(16))) w_64[64];
static complex_t __attribute__((aligned(16))) * w[7] = {w_1, w_2, w_4, w_8, w_16, w_32, w_64};

/* Twiddle factors for IMDCT */
static sample_t __attribute__((aligned(16))) xcos1[128];
static sample_t __attribute__((aligned(16))) xsin1[128];

#if ARCH_X86 || ARCH_X86_64
// NOTE: SSE needs 16byte alignment or it will segfault 
// 
static float __attribute__((aligned(16))) sseSinCos1c[256];
static float __attribute__((aligned(16))) sseSinCos1d[256];
static float attribute_used __attribute__((aligned(16))) ps111_1[4]={1,1,1,-1};
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
#endif

/* Root values for IFFT */
static sample_t roots16[3];
static sample_t roots32[7];
static sample_t roots64[15];
static sample_t roots128[31];

/* Twiddle factors for IMDCT */
static complex_t pre1[128];
static complex_t post1[64];
static complex_t pre2[64];
static complex_t post2[32];

static sample_t a52_imdct_window[256];

static void (* ifft128) (complex_t * buf);
static void (* ifft64) (complex_t * buf);

static inline void ifft2 (complex_t * buf)
{
    double r, i;

    r = buf[0].real;
    i = buf[0].imag;
    buf[0].real += buf[1].real;
    buf[0].imag += buf[1].imag;
    buf[1].real = r - buf[1].real;
    buf[1].imag = i - buf[1].imag;
}

static inline void ifft4 (complex_t * buf)
{
    double tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;

    tmp1 = buf[0].real + buf[1].real;
    tmp2 = buf[3].real + buf[2].real;
    tmp3 = buf[0].imag + buf[1].imag;
    tmp4 = buf[2].imag + buf[3].imag;
    tmp5 = buf[0].real - buf[1].real;
    tmp6 = buf[0].imag - buf[1].imag;
    tmp7 = buf[2].imag - buf[3].imag;
    tmp8 = buf[3].real - buf[2].real;

    buf[0].real = tmp1 + tmp2;
    buf[0].imag = tmp3 + tmp4;
    buf[2].real = tmp1 - tmp2;
    buf[2].imag = tmp3 - tmp4;
    buf[1].real = tmp5 + tmp7;
    buf[1].imag = tmp6 + tmp8;
    buf[3].real = tmp5 - tmp7;
    buf[3].imag = tmp6 - tmp8;
}

/* the basic split-radix ifft butterfly */

#define BUTTERFLY(a0,a1,a2,a3,wr,wi) do {	\
    tmp5 = a2.real * wr + a2.imag * wi;		\
    tmp6 = a2.imag * wr - a2.real * wi;		\
    tmp7 = a3.real * wr - a3.imag * wi;		\
    tmp8 = a3.imag * wr + a3.real * wi;		\
    tmp1 = tmp5 + tmp7;				\
    tmp2 = tmp6 + tmp8;				\
    tmp3 = tmp6 - tmp8;				\
    tmp4 = tmp7 - tmp5;				\
    a2.real = a0.real - tmp1;			\
    a2.imag = a0.imag - tmp2;			\
    a3.real = a1.real - tmp3;			\
    a3.imag = a1.imag - tmp4;			\
    a0.real += tmp1;				\
    a0.imag += tmp2;				\
    a1.real += tmp3;				\
    a1.imag += tmp4;				\
} while (0)

/* split-radix ifft butterfly, specialized for wr=1 wi=0 */

#define BUTTERFLY_ZERO(a0,a1,a2,a3) do {	\
    tmp1 = a2.real + a3.real;			\
    tmp2 = a2.imag + a3.imag;			\
    tmp3 = a2.imag - a3.imag;			\
    tmp4 = a3.real - a2.real;			\
    a2.real = a0.real - tmp1;			\
    a2.imag = a0.imag - tmp2;			\
    a3.real = a1.real - tmp3;			\
    a3.imag = a1.imag - tmp4;			\
    a0.real += tmp1;				\
    a0.imag += tmp2;				\
    a1.real += tmp3;				\
    a1.imag += tmp4;				\
} while (0)

/* split-radix ifft butterfly, specialized for wr=wi */

#define BUTTERFLY_HALF(a0,a1,a2,a3,w) do {	\
    tmp5 = (a2.real + a2.imag) * w;		\
    tmp6 = (a2.imag - a2.real) * w;		\
    tmp7 = (a3.real - a3.imag) * w;		\
    tmp8 = (a3.imag + a3.real) * w;		\
    tmp1 = tmp5 + tmp7;				\
    tmp2 = tmp6 + tmp8;				\
    tmp3 = tmp6 - tmp8;				\
    tmp4 = tmp7 - tmp5;				\
    a2.real = a0.real - tmp1;			\
    a2.imag = a0.imag - tmp2;			\
    a3.real = a1.real - tmp3;			\
    a3.imag = a1.imag - tmp4;			\
    a0.real += tmp1;				\
    a0.imag += tmp2;				\
    a1.real += tmp3;				\
    a1.imag += tmp4;				\
} while (0)

static inline void ifft8 (complex_t * buf)
{
    double tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;

    ifft4 (buf);
    ifft2 (buf + 4);
    ifft2 (buf + 6);
    BUTTERFLY_ZERO (buf[0], buf[2], buf[4], buf[6]);
    BUTTERFLY_HALF (buf[1], buf[3], buf[5], buf[7], roots16[1]);
}

static void ifft_pass (complex_t * buf, sample_t * weight, int n)
{
    complex_t * buf1;
    complex_t * buf2;
    complex_t * buf3;
    double tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
    int i;

    buf++;
    buf1 = buf + n;
    buf2 = buf + 2 * n;
    buf3 = buf + 3 * n;

    BUTTERFLY_ZERO (buf[-1], buf1[-1], buf2[-1], buf3[-1]);

    i = n - 1;

    do {
	BUTTERFLY (buf[0], buf1[0], buf2[0], buf3[0], weight[n], weight[2*i]);
	buf++;
	buf1++;
	buf2++;
	buf3++;
	weight++;
    } while (--i);
}

static void ifft16 (complex_t * buf)
{
    ifft8 (buf);
    ifft4 (buf + 8);
    ifft4 (buf + 12);
    ifft_pass (buf, roots16 - 4, 4);
}

static void ifft32 (complex_t * buf)
{
    ifft16 (buf);
    ifft8 (buf + 16);
    ifft8 (buf + 24);
    ifft_pass (buf, roots32 - 8, 8);
}

static void ifft64_c (complex_t * buf)
{
    ifft32 (buf);
    ifft16 (buf + 32);
    ifft16 (buf + 48);
    ifft_pass (buf, roots64 - 16, 16);
}

static void ifft128_c (complex_t * buf)
{
    ifft32 (buf);
    ifft16 (buf + 32);
    ifft16 (buf + 48);
    ifft_pass (buf, roots64 - 16, 16);

    ifft32 (buf + 64);
    ifft32 (buf + 96);
    ifft_pass (buf, roots128 - 32, 32);
}

void imdct_do_512 (sample_t * data, sample_t * delay, sample_t bias)
{
    int i, k;
    sample_t t_r, t_i, a_r, a_i, b_r, b_i, w_1, w_2;
    const sample_t * window = a52_imdct_window;
    complex_t buf[128];
	
    for (i = 0; i < 128; i++) {
	k = fftorder[i];
	t_r = pre1[i].real;
	t_i = pre1[i].imag;

	buf[i].real = t_i * data[255-k] + t_r * data[k];
	buf[i].imag = t_r * data[255-k] - t_i * data[k];
    }

    ifft128 (buf);

    /* Post IFFT complex multiply plus IFFT complex conjugate*/
    /* Window and convert to real valued signal */
    for (i = 0; i < 64; i++) {
	/* y[n] = z[n] * (xcos1[n] + j * xsin1[n]) ; */
	t_r = post1[i].real;
	t_i = post1[i].imag;

	a_r = t_r * buf[i].real     + t_i * buf[i].imag;
	a_i = t_i * buf[i].real     - t_r * buf[i].imag;
	b_r = t_i * buf[127-i].real + t_r * buf[127-i].imag;
	b_i = t_r * buf[127-i].real - t_i * buf[127-i].imag;

	w_1 = window[2*i];
	w_2 = window[255-2*i];
	data[2*i]     = delay[2*i] * w_2 - a_r * w_1 + bias;
	data[255-2*i] = delay[2*i] * w_1 + a_r * w_2 + bias;
	delay[2*i] = a_i;

	w_1 = window[2*i+1];
	w_2 = window[254-2*i];
	data[2*i+1]   = delay[2*i+1] * w_2 + b_r * w_1 + bias;
	data[254-2*i] = delay[2*i+1] * w_1 - b_r * w_2 + bias;
	delay[2*i+1] = b_i;
    }
}

#if HAVE_ALTIVEC

#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif

// used to build registers permutation vectors (vcprm)
// the 's' are for words in the _s_econd vector
#define WORD_0 0x00,0x01,0x02,0x03
#define WORD_1 0x04,0x05,0x06,0x07
#define WORD_2 0x08,0x09,0x0a,0x0b
#define WORD_3 0x0c,0x0d,0x0e,0x0f
#define WORD_s0 0x10,0x11,0x12,0x13
#define WORD_s1 0x14,0x15,0x16,0x17
#define WORD_s2 0x18,0x19,0x1a,0x1b
#define WORD_s3 0x1c,0x1d,0x1e,0x1f

#define vcprm(a,b,c,d) (const vector unsigned char){WORD_ ## a, WORD_ ## b, WORD_ ## c, WORD_ ## d}
#define vcii(a,b,c,d) (const vector float){FLOAT_ ## a, FLOAT_ ## b, FLOAT_ ## c, FLOAT_ ## d}

#define FOUROF(a) {a,a,a,a}

// vcprmle is used to keep the same index as in the SSE version.
// it's the same as vcprm, with the index inversed
// ('le' is Little Endian)
#define vcprmle(a,b,c,d) vcprm(d,c,b,a)

// used to build inverse/identity vectors (vcii)
// n is _n_egative, p is _p_ositive
#define FLOAT_n -1.
#define FLOAT_p 1.


void
imdct_do_512_altivec(sample_t data[],sample_t delay[], sample_t bias)
{
  int i;
  int k;
  int p,q;
  int m;
  long two_m;
  long two_m_plus_one;

  sample_t tmp_b_i;
  sample_t tmp_b_r;
  sample_t tmp_a_i;
  sample_t tmp_a_r;

  sample_t *data_ptr;
  sample_t *delay_ptr;
  sample_t *window_ptr;
	
  /* 512 IMDCT with source and dest data in 'data' */
	
  /* Pre IFFT complex multiply plus IFFT cmplx conjugate & reordering*/
  for( i=0; i < 128; i++) {
    /* z[i] = (X[256-2*i-1] + j * X[2*i]) * (xcos1[i] + j * xsin1[i]) ; */ 
    int j= bit_reverse_512[i];
    buf[i].real =         (data[256-2*j-1] * xcos1[j])  -  (data[2*j]       * xsin1[j]);
    buf[i].imag = -1.0 * ((data[2*j]       * xcos1[j])  +  (data[256-2*j-1] * xsin1[j]));
  }
  
  /* 1. iteration */
  for(i = 0; i < 128; i += 2) {
#if 0
    tmp_a_r = buf[i].real;
    tmp_a_i = buf[i].imag;
    tmp_b_r = buf[i+1].real;
    tmp_b_i = buf[i+1].imag;
    buf[i].real = tmp_a_r + tmp_b_r;
    buf[i].imag =  tmp_a_i + tmp_b_i;
    buf[i+1].real = tmp_a_r - tmp_b_r;
    buf[i+1].imag =  tmp_a_i - tmp_b_i;
#else
    vector float temp, bufv; 

    bufv = vec_ld(i << 3, (float*)buf);
    temp = vec_perm(bufv, bufv, vcprm(2,3,0,1));
    bufv = vec_madd(bufv, vcii(p,p,n,n), temp);
    vec_st(bufv, i << 3, (float*)buf);
#endif
  }
        
  /* 2. iteration */
  // Note w[1]={{1,0}, {0,-1}}
  for(i = 0; i < 128; i += 4) {
#if 0
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
    /* WARNING: im <-> re here ! */
    tmp_b_r = buf[i+3].imag;
    tmp_b_i = buf[i+3].real;
    buf[i+1].real = tmp_a_r + tmp_b_r;
    buf[i+1].imag =  tmp_a_i - tmp_b_i;
    buf[i+3].real = tmp_a_r - tmp_b_r;
    buf[i+3].imag =  tmp_a_i + tmp_b_i;
#else
    vector float buf01, buf23, temp1, temp2;
	
    buf01 = vec_ld((i + 0) << 3, (float*)buf);
    buf23 = vec_ld((i + 2) << 3, (float*)buf);
    buf23 = vec_perm(buf23,buf23,vcprm(0,1,3,2));

    temp1 = vec_madd(buf23, vcii(p,p,p,n), buf01);
    temp2 = vec_madd(buf23, vcii(n,n,n,p), buf01);

    vec_st(temp1, (i + 0) << 3, (float*)buf);
    vec_st(temp2, (i + 2) << 3, (float*)buf);
#endif
  }

  /* 3. iteration */
  for(i = 0; i < 128; i += 8) {
#if 0
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
    /* WARNING re <-> im & sign */
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
#else
    vector float buf01, buf23, buf45, buf67;

    buf01 = vec_ld((i + 0) << 3, (float*)buf);
    buf23 = vec_ld((i + 2) << 3, (float*)buf);

    tmp_b_r = (buf[i+5].real + buf[i+5].imag) * w[2][1].real;
    tmp_b_i = (buf[i+5].imag - buf[i+5].real) * w[2][1].real;
    buf[i+5].real = tmp_b_r;
    buf[i+5].imag = tmp_b_i;
    tmp_b_r = (buf[i+7].real - buf[i+7].imag) * w[2][3].imag;
    tmp_b_i = (buf[i+7].imag + buf[i+7].real) * w[2][3].imag;
    buf[i+7].real = tmp_b_r;
    buf[i+7].imag = tmp_b_i;

    buf23 = vec_ld((i + 2) << 3, (float*)buf);
    buf45 = vec_ld((i + 4) << 3, (float*)buf);
    buf67 = vec_ld((i + 6) << 3, (float*)buf);
    buf67 = vec_perm(buf67, buf67, vcprm(1,0,2,3));
	
    vec_st(vec_add(buf01, buf45), (i + 0) << 3, (float*)buf);
    vec_st(vec_madd(buf67, vcii(p,n,p,p), buf23), (i + 2) << 3, (float*)buf);
    vec_st(vec_sub(buf01, buf45), (i + 4) << 3, (float*)buf);
    vec_st(vec_nmsub(buf67, vcii(p,n,p,p), buf23), (i + 6) << 3, (float*)buf);
#endif
  }
    
  /* 4-7. iterations */
  for (m=3; m < 7; m++) {
    two_m = (1 << m);

    two_m_plus_one = two_m<<1;

    for(i = 0; i < 128; i += two_m_plus_one) {
      for(k = 0; k < two_m; k+=2) {
#if 0
        int p = k + i;
        int q = p + two_m;
        tmp_a_r = buf[p].real;
        tmp_a_i = buf[p].imag;
        tmp_b_r =
          buf[q].real * w[m][k].real -
          buf[q].imag * w[m][k].imag;
        tmp_b_i =
          buf[q].imag * w[m][k].real +
          buf[q].real * w[m][k].imag;
        buf[p].real = tmp_a_r + tmp_b_r;
        buf[p].imag =  tmp_a_i + tmp_b_i;
        buf[q].real = tmp_a_r - tmp_b_r;
        buf[q].imag =  tmp_a_i - tmp_b_i;

        tmp_a_r = buf[(p + 1)].real;
        tmp_a_i = buf[(p + 1)].imag;
        tmp_b_r =
          buf[(q + 1)].real * w[m][(k + 1)].real -
          buf[(q + 1)].imag * w[m][(k + 1)].imag;
        tmp_b_i =
          buf[(q + 1)].imag * w[m][(k + 1)].real +
          buf[(q + 1)].real * w[m][(k + 1)].imag;
        buf[(p + 1)].real = tmp_a_r + tmp_b_r;
        buf[(p + 1)].imag =  tmp_a_i + tmp_b_i;
        buf[(q + 1)].real = tmp_a_r - tmp_b_r;
        buf[(q + 1)].imag =  tmp_a_i - tmp_b_i;
#else
        int p = k + i;
        int q = p + two_m;
        vector float vecp, vecq, vecw, temp1, temp2, temp3, temp4;
        const vector float vczero = (const vector float)FOUROF(0.);
        // first compute buf[q] and buf[q+1]
        vecq = vec_ld(q << 3, (float*)buf);
        vecw = vec_ld(0, (float*)&(w[m][k]));
        temp1 = vec_madd(vecq, vecw, vczero);
        temp2 = vec_perm(vecq, vecq, vcprm(1,0,3,2));
        temp2 = vec_madd(temp2, vecw, vczero);
        temp3 = vec_perm(temp1, temp2, vcprm(0,s0,2,s2));
        temp4 = vec_perm(temp1, temp2, vcprm(1,s1,3,s3));
        vecq = vec_madd(temp4, vcii(n,p,n,p), temp3);
        // then butterfly with buf[p] and buf[p+1]
        vecp = vec_ld(p << 3, (float*)buf);
        
        temp1 = vec_add(vecp, vecq);
        temp2 = vec_sub(vecp, vecq);
                
        vec_st(temp1, p << 3, (float*)buf);
        vec_st(temp2, q << 3, (float*)buf);
#endif
      }
    }
  }

  /* Post IFFT complex multiply  plus IFFT complex conjugate*/
  for( i=0; i < 128; i+=4) {
    /* y[n] = z[n] * (xcos1[n] + j * xsin1[n]) ; */
#if 0
    tmp_a_r =        buf[(i + 0)].real;
    tmp_a_i = -1.0 * buf[(i + 0)].imag;
    buf[(i + 0)].real =
      (tmp_a_r * xcos1[(i + 0)])  -  (tmp_a_i  * xsin1[(i + 0)]);
    buf[(i + 0)].imag =
      (tmp_a_r * xsin1[(i + 0)])  +  (tmp_a_i  * xcos1[(i + 0)]);

    tmp_a_r =        buf[(i + 1)].real;
    tmp_a_i = -1.0 * buf[(i + 1)].imag;
    buf[(i + 1)].real =
      (tmp_a_r * xcos1[(i + 1)])  -  (tmp_a_i  * xsin1[(i + 1)]);
    buf[(i + 1)].imag =
      (tmp_a_r * xsin1[(i + 1)])  +  (tmp_a_i  * xcos1[(i + 1)]);

    tmp_a_r =        buf[(i + 2)].real;
    tmp_a_i = -1.0 * buf[(i + 2)].imag;
    buf[(i + 2)].real =
      (tmp_a_r * xcos1[(i + 2)])  -  (tmp_a_i  * xsin1[(i + 2)]);
    buf[(i + 2)].imag =
      (tmp_a_r * xsin1[(i + 2)])  +  (tmp_a_i  * xcos1[(i + 2)]);

    tmp_a_r =        buf[(i + 3)].real;
    tmp_a_i = -1.0 * buf[(i + 3)].imag;
    buf[(i + 3)].real =
      (tmp_a_r * xcos1[(i + 3)])  -  (tmp_a_i  * xsin1[(i + 3)]);
    buf[(i + 3)].imag =
      (tmp_a_r * xsin1[(i + 3)])  +  (tmp_a_i  * xcos1[(i + 3)]);
#else
    vector float bufv_0, bufv_2, cosv, sinv, temp1, temp2;
    vector float temp0022, temp1133, tempCS01;
    const vector float vczero = (const vector float)FOUROF(0.);

    bufv_0 = vec_ld((i + 0) << 3, (float*)buf);
    bufv_2 = vec_ld((i + 2) << 3, (float*)buf);

    cosv = vec_ld(i << 2, xcos1);
    sinv = vec_ld(i << 2, xsin1);

    temp0022 = vec_perm(bufv_0, bufv_0, vcprm(0,0,2,2));
    temp1133 = vec_perm(bufv_0, bufv_0, vcprm(1,1,3,3));
    tempCS01 = vec_perm(cosv, sinv, vcprm(0,s0,1,s1));
    temp1 = vec_madd(temp0022, tempCS01, vczero);
    tempCS01 = vec_perm(cosv, sinv, vcprm(s0,0,s1,1));
    temp2 = vec_madd(temp1133, tempCS01, vczero);
    bufv_0 = vec_madd(temp2, vcii(p,n,p,n), temp1);
    
    vec_st(bufv_0, (i + 0) << 3, (float*)buf);

    /* idem with bufv_2 and high-order cosv/sinv */

    temp0022 = vec_perm(bufv_2, bufv_2, vcprm(0,0,2,2));
    temp1133 = vec_perm(bufv_2, bufv_2, vcprm(1,1,3,3));
    tempCS01 = vec_perm(cosv, sinv, vcprm(2,s2,3,s3));
    temp1 = vec_madd(temp0022, tempCS01, vczero);
    tempCS01 = vec_perm(cosv, sinv, vcprm(s2,2,s3,3));
    temp2 = vec_madd(temp1133, tempCS01, vczero);
    bufv_2 = vec_madd(temp2, vcii(p,n,p,n), temp1);

    vec_st(bufv_2, (i + 2) << 3, (float*)buf);
    
#endif
  }
  
  data_ptr = data;
  delay_ptr = delay;
  window_ptr = a52_imdct_window;

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
#endif


// Stuff below this line is borrowed from libac3
#include "srfftp.h"
#if ARCH_X86 || ARCH_X86_64
#undef HAVE_AMD3DNOW
#define HAVE_AMD3DNOW 1
#include "srfftp_3dnow.h"

const i_cmplx_t x_plus_minus_3dnow __attribute__ ((aligned (8))) = {{ 0x00000000UL, 0x80000000UL }}; 
const i_cmplx_t x_minus_plus_3dnow __attribute__ ((aligned (8))) = {{ 0x80000000UL, 0x00000000UL }}; 
const complex_t HSQRT2_3DNOW __attribute__ ((aligned (8))) = { 0.707106781188, 0.707106781188 };

#undef HAVE_AMD3DNOWEXT
#define HAVE_AMD3DNOWEXT 0
#include "imdct_3dnow.h"
#undef HAVE_AMD3DNOWEXT
#define HAVE_AMD3DNOWEXT 1
#include "imdct_3dnow.h"

void
imdct_do_512_sse(sample_t data[],sample_t delay[], sample_t bias)
{
/*	int i,k;
    int p,q;*/
    int m;
    long two_m;
    long two_m_plus_one;
    long two_m_plus_one_shl3;
    complex_t *buf_offset;

/*  sample_t tmp_a_i;
    sample_t tmp_a_r;
    sample_t tmp_b_i;
    sample_t tmp_b_r;*/

    sample_t *data_ptr;
    sample_t *delay_ptr;
    sample_t *window_ptr;
	
    /* 512 IMDCT with source and dest data in 'data' */
    /* see the c version (dct_do_512()), its allmost identical, just in C */ 

    /* Pre IFFT complex multiply plus IFFT cmplx conjugate */
    /* Bit reversed shuffling */
	__asm__ volatile(
		"xor %%"REG_S", %%"REG_S"		\n\t"
		"lea "MANGLE(bit_reverse_512)", %%"REG_a"\n\t"
		"mov $1008, %%"REG_D"			\n\t"
		"push %%"REG_BP"			\n\t" //use ebp without telling gcc
		ASMALIGN(4)
		"1:					\n\t"
		"movlps (%0, %%"REG_S"), %%xmm0	\n\t" // XXXI
		"movhps 8(%0, %%"REG_D"), %%xmm0	\n\t" // RXXI
		"movlps 8(%0, %%"REG_S"), %%xmm1	\n\t" // XXXi
		"movhps (%0, %%"REG_D"), %%xmm1	\n\t" // rXXi
		"shufps $0x33, %%xmm1, %%xmm0		\n\t" // irIR
		"movaps "MANGLE(sseSinCos1c)"(%%"REG_S"), %%xmm2\n\t"
		"mulps %%xmm0, %%xmm2			\n\t"
		"shufps $0xB1, %%xmm0, %%xmm0		\n\t" // riRI
		"mulps "MANGLE(sseSinCos1d)"(%%"REG_S"), %%xmm0\n\t"
		"subps %%xmm0, %%xmm2			\n\t"
		"movzb (%%"REG_a"), %%"REG_d"		\n\t"
		"movzb 1(%%"REG_a"), %%"REG_BP"		\n\t"
		"movlps %%xmm2, (%1, %%"REG_d", 8)	\n\t"
		"movhps %%xmm2, (%1, %%"REG_BP", 8)	\n\t"
		"add $16, %%"REG_S"			\n\t"
		"add $2, %%"REG_a"			\n\t" // avoid complex addressing for P4 crap
		"sub $16, %%"REG_D"			\n\t"
		"jnc 1b				 	\n\t"
		"pop %%"REG_BP"				\n\t"//no we didnt touch ebp *g*
		:: "b" (data), "c" (buf)
		: "%"REG_S, "%"REG_D, "%"REG_a, "%"REG_d
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
	__asm__ volatile(
		"xorps %%xmm1, %%xmm1	\n\t"
		"xorps %%xmm2, %%xmm2	\n\t"
		"mov %0, %%"REG_S"	\n\t"
		ASMALIGN(4)
		"1:			\n\t"
		"movlps (%%"REG_S"), %%xmm0\n\t" //buf[p]
		"movlps 8(%%"REG_S"), %%xmm1\n\t" //buf[q]
		"movhps (%%"REG_S"), %%xmm0\n\t" //buf[p]
		"movhps 8(%%"REG_S"), %%xmm2\n\t" //buf[q]
		"addps %%xmm1, %%xmm0	\n\t"
		"subps %%xmm2, %%xmm0	\n\t"
		"movaps %%xmm0, (%%"REG_S")\n\t"
		"add $16, %%"REG_S"	\n\t"
		"cmp %1, %%"REG_S"	\n\t"
		" jb 1b			\n\t"
		:: "g" (buf), "r" (buf + 128)
		: "%"REG_S
	);
        
    /* 2. iteration */
	// Note w[1]={{1,0}, {0,-1}}
	__asm__ volatile(
		"movaps "MANGLE(ps111_1)", %%xmm7\n\t" // 1,1,1,-1
		"mov %0, %%"REG_S"		\n\t"
		ASMALIGN(4)
		"1:				\n\t"
		"movaps 16(%%"REG_S"), %%xmm2	\n\t" //r2,i2,r3,i3
		"shufps $0xB4, %%xmm2, %%xmm2	\n\t" //r2,i2,i3,r3
		"mulps %%xmm7, %%xmm2		\n\t" //r2,i2,i3,-r3
		"movaps (%%"REG_S"), %%xmm0	\n\t" //r0,i0,r1,i1
		"movaps (%%"REG_S"), %%xmm1	\n\t" //r0,i0,r1,i1
		"addps %%xmm2, %%xmm0		\n\t"
		"subps %%xmm2, %%xmm1		\n\t"
		"movaps %%xmm0, (%%"REG_S")	\n\t"
		"movaps %%xmm1, 16(%%"REG_S")	\n\t"
		"add $32, %%"REG_S"	\n\t"
		"cmp %1, %%"REG_S"	\n\t"
		" jb 1b			\n\t"
		:: "g" (buf), "r" (buf + 128)
		: "%"REG_S
	);

    /* 3. iteration */
/*
 Note sseW2+0={1,1,sqrt(2),sqrt(2))
 Note sseW2+16={0,0,sqrt(2),-sqrt(2))
 Note sseW2+32={0,0,-sqrt(2),-sqrt(2))
 Note sseW2+48={1,-1,sqrt(2),-sqrt(2))
*/
	__asm__ volatile(
		"movaps 48+"MANGLE(sseW2)", %%xmm6\n\t" 
		"movaps 16+"MANGLE(sseW2)", %%xmm7\n\t" 
		"xorps %%xmm5, %%xmm5		\n\t"
		"xorps %%xmm2, %%xmm2		\n\t"
		"mov %0, %%"REG_S"		\n\t"
		ASMALIGN(4)
		"1:				\n\t"
		"movaps 32(%%"REG_S"), %%xmm2	\n\t" //r4,i4,r5,i5
		"movaps 48(%%"REG_S"), %%xmm3	\n\t" //r6,i6,r7,i7
		"movaps "MANGLE(sseW2)", %%xmm4	\n\t" //r4,i4,r5,i5
		"movaps 32+"MANGLE(sseW2)", %%xmm5\n\t" //r6,i6,r7,i7
		"mulps %%xmm2, %%xmm4		\n\t"
		"mulps %%xmm3, %%xmm5		\n\t"
		"shufps $0xB1, %%xmm2, %%xmm2	\n\t" //i4,r4,i5,r5
		"shufps $0xB1, %%xmm3, %%xmm3	\n\t" //i6,r6,i7,r7
		"mulps %%xmm6, %%xmm3		\n\t"
		"mulps %%xmm7, %%xmm2		\n\t"
		"movaps (%%"REG_S"), %%xmm0	\n\t" //r0,i0,r1,i1
		"movaps 16(%%"REG_S"), %%xmm1	\n\t" //r2,i2,r3,i3
		"addps %%xmm4, %%xmm2		\n\t"
		"addps %%xmm5, %%xmm3		\n\t"
		"movaps %%xmm2, %%xmm4		\n\t"
		"movaps %%xmm3, %%xmm5		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"addps %%xmm1, %%xmm3		\n\t"
		"subps %%xmm4, %%xmm0		\n\t"
		"subps %%xmm5, %%xmm1		\n\t"
		"movaps %%xmm2, (%%"REG_S")	\n\t" 
		"movaps %%xmm3, 16(%%"REG_S")	\n\t" 
		"movaps %%xmm0, 32(%%"REG_S")	\n\t" 
		"movaps %%xmm1, 48(%%"REG_S")	\n\t" 
		"add $64, %%"REG_S"	\n\t"
		"cmp %1, %%"REG_S"	\n\t"
		" jb 1b			\n\t"
		:: "g" (buf), "r" (buf + 128)
		: "%"REG_S
	);

    /* 4-7. iterations */
    for (m=3; m < 7; m++) {
	two_m = (1 << m);
	two_m_plus_one = two_m<<1;
	two_m_plus_one_shl3 = (two_m_plus_one<<3);
	buf_offset = buf+128;
	__asm__ volatile(
		"mov %0, %%"REG_S"			\n\t"
		ASMALIGN(4)
		"1:					\n\t"
		"xor %%"REG_D", %%"REG_D"		\n\t" // k
		"lea (%%"REG_S", %3), %%"REG_d"		\n\t"
		"2:					\n\t"
		"movaps (%%"REG_d", %%"REG_D"), %%xmm1	\n\t"
		"movaps (%4, %%"REG_D", 2), %%xmm2	\n\t"
		"mulps %%xmm1, %%xmm2			\n\t"
		"shufps $0xB1, %%xmm1, %%xmm1		\n\t"
		"mulps 16(%4, %%"REG_D", 2), %%xmm1	\n\t"
		"movaps (%%"REG_S", %%"REG_D"), %%xmm0	\n\t"
		"addps %%xmm2, %%xmm1			\n\t"
		"movaps %%xmm1, %%xmm2			\n\t"
		"addps %%xmm0, %%xmm1			\n\t"
		"subps %%xmm2, %%xmm0			\n\t"
		"movaps %%xmm1, (%%"REG_S", %%"REG_D")	\n\t"
		"movaps %%xmm0, (%%"REG_d", %%"REG_D")	\n\t"
		"add $16, %%"REG_D"			\n\t"
		"cmp %3, %%"REG_D"			\n\t" //FIXME (opt) count against 0 
		"jb 2b					\n\t"
		"add %2, %%"REG_S"			\n\t"
		"cmp %1, %%"REG_S"			\n\t"
		" jb 1b					\n\t"
		:: "g" (buf), "m" (buf_offset), "m" (two_m_plus_one_shl3), "r" (two_m<<3),
		   "r" (sseW[m])
		: "%"REG_S, "%"REG_D, "%"REG_d
	);
    }

    /* Post IFFT complex multiply  plus IFFT complex conjugate*/
	__asm__ volatile(
		"mov $-1024, %%"REG_S"			\n\t"
		ASMALIGN(4)
		"1:					\n\t"
		"movaps (%0, %%"REG_S"), %%xmm0		\n\t"
		"movaps (%0, %%"REG_S"), %%xmm1		\n\t"
		"shufps $0xB1, %%xmm0, %%xmm0		\n\t"
		"mulps 1024+"MANGLE(sseSinCos1c)"(%%"REG_S"), %%xmm1\n\t"
		"mulps 1024+"MANGLE(sseSinCos1d)"(%%"REG_S"), %%xmm0\n\t"
		"addps %%xmm1, %%xmm0			\n\t"
		"movaps %%xmm0, (%0, %%"REG_S")		\n\t"
		"add $16, %%"REG_S"			\n\t"
		" jnz 1b				\n\t"
		:: "r" (buf+128)
		: "%"REG_S
	);   

	
    data_ptr = data;
    delay_ptr = delay;
    window_ptr = a52_imdct_window;

    /* Window and convert to real valued signal */
	__asm__ volatile(
		"xor %%"REG_D", %%"REG_D"		\n\t"  // 0
		"xor %%"REG_S", %%"REG_S"		\n\t"  // 0
		"movss %3, %%xmm2			\n\t"  // bias
		"shufps $0x00, %%xmm2, %%xmm2		\n\t"  // bias, bias, ...
		ASMALIGN(4)
		"1:					\n\t"
		"movlps (%0, %%"REG_S"), %%xmm0		\n\t" // ? ? A ?
		"movlps 8(%0, %%"REG_S"), %%xmm1	\n\t" // ? ? C ?
		"movhps -16(%0, %%"REG_D"), %%xmm1	\n\t" // ? D C ?
		"movhps -8(%0, %%"REG_D"), %%xmm0	\n\t" // ? B A ?
		"shufps $0x99, %%xmm1, %%xmm0		\n\t" // D C B A
		"mulps "MANGLE(sseWindow)"(%%"REG_S"), %%xmm0\n\t"
		"addps (%2, %%"REG_S"), %%xmm0		\n\t"
		"addps %%xmm2, %%xmm0			\n\t"
		"movaps %%xmm0, (%1, %%"REG_S")		\n\t"
		"add  $16, %%"REG_S"			\n\t"
		"sub  $16, %%"REG_D"			\n\t"
		"cmp  $512, %%"REG_S"			\n\t" 
		" jb 1b					\n\t"
		:: "r" (buf+64), "r" (data_ptr), "r" (delay_ptr), "m" (bias)
		: "%"REG_S, "%"REG_D
	);
	data_ptr+=128;
	delay_ptr+=128;
//	window_ptr+=128;
	
	__asm__ volatile(
		"mov $1024, %%"REG_D"			\n\t"  // 512
		"xor %%"REG_S", %%"REG_S"		\n\t"  // 0
		"movss %3, %%xmm2			\n\t"  // bias
		"shufps $0x00, %%xmm2, %%xmm2		\n\t"  // bias, bias, ...
		ASMALIGN(4)
		"1:					\n\t"
		"movlps (%0, %%"REG_S"), %%xmm0		\n\t" // ? ? ? A
		"movlps 8(%0, %%"REG_S"), %%xmm1	\n\t" // ? ? ? C
		"movhps -16(%0, %%"REG_D"), %%xmm1	\n\t" // D ? ? C
		"movhps -8(%0, %%"REG_D"), %%xmm0	\n\t" // B ? ? A
		"shufps $0xCC, %%xmm1, %%xmm0		\n\t" // D C B A
		"mulps 512+"MANGLE(sseWindow)"(%%"REG_S"), %%xmm0\n\t"
		"addps (%2, %%"REG_S"), %%xmm0		\n\t"
		"addps %%xmm2, %%xmm0			\n\t"
		"movaps %%xmm0, (%1, %%"REG_S")		\n\t"
		"add $16, %%"REG_S"			\n\t"
		"sub $16, %%"REG_D"			\n\t"
		"cmp $512, %%"REG_S"			\n\t" 
		" jb 1b					\n\t"
		:: "r" (buf), "r" (data_ptr), "r" (delay_ptr), "m" (bias)
		: "%"REG_S, "%"REG_D
	);
	data_ptr+=128;
//	window_ptr+=128;

    /* The trailing edge of the window goes into the delay line */
    delay_ptr = delay;

	__asm__ volatile(
		"xor %%"REG_D", %%"REG_D"		\n\t"  // 0
		"xor %%"REG_S", %%"REG_S"		\n\t"  // 0
		ASMALIGN(4)
		"1:					\n\t"
		"movlps (%0, %%"REG_S"), %%xmm0		\n\t" // ? ? ? A
		"movlps 8(%0, %%"REG_S"), %%xmm1	\n\t" // ? ? ? C
		"movhps -16(%0, %%"REG_D"), %%xmm1	\n\t" // D ? ? C 
		"movhps -8(%0, %%"REG_D"), %%xmm0	\n\t" // B ? ? A 
		"shufps $0xCC, %%xmm1, %%xmm0		\n\t" // D C B A
		"mulps 1024+"MANGLE(sseWindow)"(%%"REG_S"), %%xmm0\n\t"
		"movaps %%xmm0, (%1, %%"REG_S")		\n\t"
		"add $16, %%"REG_S"			\n\t"
		"sub $16, %%"REG_D"			\n\t"
		"cmp $512, %%"REG_S"			\n\t" 
		" jb 1b					\n\t"
		:: "r" (buf+64), "r" (delay_ptr)
		: "%"REG_S, "%"REG_D
	);
	delay_ptr+=128;
//	window_ptr-=128;
	
	__asm__ volatile(
		"mov $1024, %%"REG_D"			\n\t"  // 1024
		"xor %%"REG_S", %%"REG_S"		\n\t"  // 0
		ASMALIGN(4)
		"1:					\n\t"
		"movlps (%0, %%"REG_S"), %%xmm0	\n\t" // ? ? A ?
		"movlps 8(%0, %%"REG_S"), %%xmm1	\n\t" // ? ? C ?
		"movhps -16(%0, %%"REG_D"), %%xmm1	\n\t" // ? D C ? 
		"movhps -8(%0, %%"REG_D"), %%xmm0	\n\t" // ? B A ? 
		"shufps $0x99, %%xmm1, %%xmm0		\n\t" // D C B A
		"mulps 1536+"MANGLE(sseWindow)"(%%"REG_S"), %%xmm0\n\t"
		"movaps %%xmm0, (%1, %%"REG_S")		\n\t"
		"add $16, %%"REG_S"			\n\t"
		"sub $16, %%"REG_D"			\n\t"
		"cmp $512, %%"REG_S"			\n\t" 
		" jb 1b					\n\t"
		:: "r" (buf), "r" (delay_ptr)
		: "%"REG_S, "%"REG_D
	);
}
#endif // ARCH_X86 || ARCH_X86_64

void a52_imdct_256(sample_t * data, sample_t * delay, sample_t bias)
{
    int i, k;
    sample_t t_r, t_i, a_r, a_i, b_r, b_i, c_r, c_i, d_r, d_i, w_1, w_2;
    const sample_t * window = a52_imdct_window;
    complex_t buf1[64], buf2[64];

    /* Pre IFFT complex multiply plus IFFT cmplx conjugate */
    for (i = 0; i < 64; i++) {
	k = fftorder[i];
	t_r = pre2[i].real;
	t_i = pre2[i].imag;

	buf1[i].real = t_i * data[254-k] + t_r * data[k];
	buf1[i].imag = t_r * data[254-k] - t_i * data[k];

	buf2[i].real = t_i * data[255-k] + t_r * data[k+1];
	buf2[i].imag = t_r * data[255-k] - t_i * data[k+1];
    }

    ifft64 (buf1);
    ifft64 (buf2);

    /* Post IFFT complex multiply */
    /* Window and convert to real valued signal */
    for (i = 0; i < 32; i++) {
	/* y1[n] = z1[n] * (xcos2[n] + j * xs in2[n]) ; */ 
	t_r = post2[i].real;
	t_i = post2[i].imag;

	a_r = t_r * buf1[i].real    + t_i * buf1[i].imag;
	a_i = t_i * buf1[i].real    - t_r * buf1[i].imag;
	b_r = t_i * buf1[63-i].real + t_r * buf1[63-i].imag;
	b_i = t_r * buf1[63-i].real - t_i * buf1[63-i].imag;

	c_r = t_r * buf2[i].real    + t_i * buf2[i].imag;
	c_i = t_i * buf2[i].real    - t_r * buf2[i].imag;
	d_r = t_i * buf2[63-i].real + t_r * buf2[63-i].imag;
	d_i = t_r * buf2[63-i].real - t_i * buf2[63-i].imag;

	w_1 = window[2*i];
	w_2 = window[255-2*i];
	data[2*i]     = delay[2*i] * w_2 - a_r * w_1 + bias;
	data[255-2*i] = delay[2*i] * w_1 + a_r * w_2 + bias;
	delay[2*i] = c_i;

	w_1 = window[128+2*i];
	w_2 = window[127-2*i];
	data[128+2*i] = delay[127-2*i] * w_2 + a_i * w_1 + bias;
	data[127-2*i] = delay[127-2*i] * w_1 - a_i * w_2 + bias;
	delay[127-2*i] = c_r;

	w_1 = window[2*i+1];
	w_2 = window[254-2*i];
	data[2*i+1]   = delay[2*i+1] * w_2 - b_i * w_1 + bias;
	data[254-2*i] = delay[2*i+1] * w_1 + b_i * w_2 + bias;
	delay[2*i+1] = d_r;

	w_1 = window[129+2*i];
	w_2 = window[126-2*i];
	data[129+2*i] = delay[126-2*i] * w_2 + b_r * w_1 + bias;
	data[126-2*i] = delay[126-2*i] * w_1 - b_r * w_2 + bias;
	delay[126-2*i] = d_i;
    }
}

static double besselI0 (double x)
{
    double bessel = 1;
    int i = 100;

    do
	bessel = bessel * x / (i * i) + 1;
    while (--i);
    return bessel;
}

void a52_imdct_init (uint32_t mm_accel)
{
    int i, j, k;
    double sum;

    /* compute imdct window - kaiser-bessel derived window, alpha = 5.0 */
    sum = 0;
    for (i = 0; i < 256; i++) {
	sum += besselI0 (i * (256 - i) * (5 * M_PI / 256) * (5 * M_PI / 256));
	a52_imdct_window[i] = sum;
    }
    sum++;
    for (i = 0; i < 256; i++)
	a52_imdct_window[i] = sqrt (a52_imdct_window[i] / sum);

    for (i = 0; i < 3; i++)
	roots16[i] = cos ((M_PI / 8) * (i + 1));

    for (i = 0; i < 7; i++)
	roots32[i] = cos ((M_PI / 16) * (i + 1));

    for (i = 0; i < 15; i++)
	roots64[i] = cos ((M_PI / 32) * (i + 1));

    for (i = 0; i < 31; i++)
	roots128[i] = cos ((M_PI / 64) * (i + 1));

    for (i = 0; i < 64; i++) {
	k = fftorder[i] / 2 + 64;
	pre1[i].real = cos ((M_PI / 256) * (k - 0.25));
	pre1[i].imag = sin ((M_PI / 256) * (k - 0.25));
    }

    for (i = 64; i < 128; i++) {
	k = fftorder[i] / 2 + 64;
	pre1[i].real = -cos ((M_PI / 256) * (k - 0.25));
	pre1[i].imag = -sin ((M_PI / 256) * (k - 0.25));
    }

    for (i = 0; i < 64; i++) {
	post1[i].real = cos ((M_PI / 256) * (i + 0.5));
	post1[i].imag = sin ((M_PI / 256) * (i + 0.5));
    }

    for (i = 0; i < 64; i++) {
	k = fftorder[i] / 4;
	pre2[i].real = cos ((M_PI / 128) * (k - 0.25));
	pre2[i].imag = sin ((M_PI / 128) * (k - 0.25));
    }

    for (i = 0; i < 32; i++) {
	post2[i].real = cos ((M_PI / 128) * (i + 0.5));
	post2[i].imag = sin ((M_PI / 128) * (i + 0.5));
    }
    for (i = 0; i < 128; i++) {
	xcos1[i] = -cos ((M_PI / 2048) * (8 * i + 1));
	xsin1[i] = -sin ((M_PI / 2048) * (8 * i + 1));
    }
    for (i = 0; i < 7; i++) {
	j = 1 << i;
	for (k = 0; k < j; k++) {
	    w[i][k].real = cos (-M_PI * k / j);
	    w[i][k].imag = sin (-M_PI * k / j);
	}
    }
#if ARCH_X86 || ARCH_X86_64
	for (i = 0; i < 128; i++) {
	    sseSinCos1c[2*i+0]= xcos1[i];
	    sseSinCos1c[2*i+1]= -xcos1[i];
	    sseSinCos1d[2*i+0]= xsin1[i];
	    sseSinCos1d[2*i+1]= xsin1[i];	
	}
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
		sseWindow[2*i+0]= -a52_imdct_window[2*i+0];
		sseWindow[2*i+1]=  a52_imdct_window[2*i+1];	
	}
	
	for(i=0; i<64; i++)
	{
		sseWindow[256 + 2*i+0]= -a52_imdct_window[254 - 2*i+1];
		sseWindow[256 + 2*i+1]=  a52_imdct_window[254 - 2*i+0];
		sseWindow[384 + 2*i+0]=  a52_imdct_window[126 - 2*i+1];
		sseWindow[384 + 2*i+1]= -a52_imdct_window[126 - 2*i+0];
	}
#endif
	a52_imdct_512 = imdct_do_512;
	ifft128 = ifft128_c;
	ifft64 = ifft64_c;

#if ARCH_X86 || ARCH_X86_64
	if(mm_accel & MM_ACCEL_X86_SSE)
	{
	  fprintf (stderr, "Using SSE optimized IMDCT transform\n");
	  a52_imdct_512 = imdct_do_512_sse;
	}
	else
	if(mm_accel & MM_ACCEL_X86_3DNOWEXT)
	{
	  fprintf (stderr, "Using 3DNowEx optimized IMDCT transform\n");
	  a52_imdct_512 = imdct_do_512_3dnowex;
	}
	else
	if(mm_accel & MM_ACCEL_X86_3DNOW)
	{
	  fprintf (stderr, "Using 3DNow optimized IMDCT transform\n");
	  a52_imdct_512 = imdct_do_512_3dnow;
	}
	else
#endif // ARCH_X86 || ARCH_X86_64
#if HAVE_ALTIVEC
        if (mm_accel & MM_ACCEL_PPC_ALTIVEC)
	{
	  fprintf(stderr, "Using AltiVec optimized IMDCT transform\n");
          a52_imdct_512 = imdct_do_512_altivec;
	}
	else
#endif

#ifdef LIBA52_DJBFFT
    if (mm_accel & MM_ACCEL_DJBFFT) {
	fprintf (stderr, "Using djbfft for IMDCT transform\n");
	ifft128 = (void (*) (complex_t *)) fftc4_un128;
	ifft64 = (void (*) (complex_t *)) fftc4_un64;
    } else
#endif
    {
	fprintf (stderr, "No accelerated IMDCT transform found\n");
    }
}
