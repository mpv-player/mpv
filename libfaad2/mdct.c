/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: mdct.c,v 1.2 2003/10/03 22:22:27 alex Exp $
**/

/*
 * Fast (I)MDCT Implementation using (I)FFT ((Inverse) Fast Fourier Transform)
 * and consists of three steps: pre-(I)FFT complex multiplication, complex
 * (I)FFT, post-(I)FFT complex multiplication,
 * 
 * As described in:
 *  P. Duhamel, Y. Mahieux, and J.P. Petit, "A Fast Algorithm for the
 *  Implementation of Filter Banks Based on 'Time Domain Aliasing
 *  Cancellation’," IEEE Proc. on ICASSP‘91, 1991, pp. 2209-2212.
 *
 *
 * As of April 6th 2002 completely rewritten.
 * This (I)MDCT can now be used for any data size n, where n is divisible by 8.
 *
 */

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#ifdef _WIN32_WCE
#define assert(x)
#else
#include <assert.h>
#endif

#include "cfft.h"
#include "mdct.h"

/* const_tab[]:
    0: sqrt(2 / N)
    1: cos(2 * PI / N)
    2: sin(2 * PI / N)
    3: cos(2 * PI * (1/8) / N)
    4: sin(2 * PI * (1/8) / N)
 */
#ifdef FIXED_POINT
real_t const_tab[][5] =
{
    {    /* 2048 */
        COEF_CONST(1),
        FRAC_CONST(0.99999529380957619),
        FRAC_CONST(0.0030679567629659761),
        FRAC_CONST(0.99999992646571789),
        FRAC_CONST(0.00038349518757139556)
    }, { /* 1920 */
        COEF_CONST(/* sqrt(1024/960) */ 1.0327955589886444),
        FRAC_CONST(0.99999464540169647),
        FRAC_CONST(0.0032724865065266251),
        FRAC_CONST(0.99999991633432805),
        FRAC_CONST(0.00040906153202803459)
    }, { /* 1024 */
        COEF_CONST(1),
        FRAC_CONST(0.99998117528260111),
        FRAC_CONST(0.0061358846491544753),
        FRAC_CONST(0.99999970586288223),
        FRAC_CONST(0.00076699031874270449)
    }, { /* 960 */
        COEF_CONST(/* sqrt(512/480) */ 1.0327955589886444),
        FRAC_CONST(0.99997858166412923),
        FRAC_CONST(0.0065449379673518581),
        FRAC_CONST(0.99999966533732598),
        FRAC_CONST(0.00081812299560725323)
    }, { /* 256 */
        COEF_CONST(1),
        FRAC_CONST(0.99969881869620425),
        FRAC_CONST(0.024541228522912288),
        FRAC_CONST(0.99999529380957619),
        FRAC_CONST(0.0030679567629659761)
    }, {  /* 240 */
        COEF_CONST(/* sqrt(256/240) */ 1.0327955589886444),
        FRAC_CONST(0.99965732497555726),
        FRAC_CONST(0.026176948307873149),
        FRAC_CONST(0.99999464540169647),
        FRAC_CONST(0.0032724865065266251)
    }
#ifdef SSR_DEC
    ,{   /* 512 */
        COEF_CONST(1),
        FRAC_CONST(0.9999247018391445),
        FRAC_CONST(0.012271538285719925),
        FRAC_CONST(0.99999882345170188),
        FRAC_CONST(0.0015339801862847655)
    }, { /* 64 */
        COEF_CONST(1),
        FRAC_CONST(0.99518472667219693),
        FRAC_CONST(0.098017140329560604),
        FRAC_CONST(0.9999247018391445),
        FRAC_CONST(0.012271538285719925)
    }
#endif
};
#endif

#ifdef FIXED_POINT
static uint8_t map_N_to_idx(uint16_t N)
{
    /* gives an index into const_tab above */
    /* for normal AAC deocding (eg. no scalable profile) only */
    /* index 0 and 4 will be used */
    switch(N)
    {
    case 2048: return 0;
    case 1920: return 1;
    case 1024: return 2;
    case 960:  return 3;
    case 256:  return 4;
    case 240:  return 5;
#ifdef SSR_DEC
    case 512:  return 6;
    case 64:   return 7;
#endif
    }
    return 0;
}
#endif

mdct_info *faad_mdct_init(uint16_t N)
{
    uint16_t k;
#ifdef FIXED_POINT
    uint16_t N_idx;
    real_t cangle, sangle, c, s, cold;
#endif
	real_t scale;

    mdct_info *mdct = (mdct_info*)faad_malloc(sizeof(mdct_info));

    assert(N % 8 == 0);

    mdct->N = N;
    mdct->sincos = (complex_t*)faad_malloc(N/4*sizeof(complex_t));

#ifdef FIXED_POINT
    N_idx = map_N_to_idx(N);

    scale = const_tab[N_idx][0];
    cangle = const_tab[N_idx][1];
    sangle = const_tab[N_idx][2];
    c = const_tab[N_idx][3];
    s = const_tab[N_idx][4];
#else
    scale = (real_t)sqrt(2.0 / (real_t)N);
#endif

    /* (co)sine table build using recurrence relations */
    /* this can also be done using static table lookup or */
    /* some form of interpolation */
    for (k = 0; k < N/4; k++)
    {
#ifdef FIXED_POINT
        RE(mdct->sincos[k]) = c; //MUL_C_C(c,scale);
        IM(mdct->sincos[k]) = s; //MUL_C_C(s,scale);

        cold = c;
        c = MUL_F(c,cangle) - MUL_F(s,sangle);
        s = MUL_F(s,cangle) + MUL_F(cold,sangle);
#else
        /* no recurrence, just sines */
        RE(mdct->sincos[k]) = scale*(real_t)(cos(2.0*M_PI*(k+1./8.) / (real_t)N));
        IM(mdct->sincos[k]) = scale*(real_t)(sin(2.0*M_PI*(k+1./8.) / (real_t)N));
#endif
    }

    /* initialise fft */
    mdct->cfft = cffti(N/4);

#ifdef PROFILE
    mdct->cycles = 0;
    mdct->fft_cycles = 0;
#endif

    return mdct;
}

void faad_mdct_end(mdct_info *mdct)
{
    if (mdct != NULL)
    {
#ifdef PROFILE
        printf("MDCT[%.4d]:         %I64d cycles\n", mdct->N, mdct->cycles);
        printf("CFFT[%.4d]:         %I64d cycles\n", mdct->N/4, mdct->fft_cycles);
#endif

        cfftu(mdct->cfft);

        if (mdct->sincos) faad_free(mdct->sincos);

        faad_free(mdct);
    }
}

void faad_imdct(mdct_info *mdct, real_t *X_in, real_t *X_out)
{
    uint16_t k;

    complex_t x;
    ALIGN complex_t Z1[512];
    complex_t *sincos = mdct->sincos;

    uint16_t N  = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;

#ifdef PROFILE
    int64_t count1, count2 = faad_get_ts();
#endif

    /* pre-IFFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
        ComplexMult(&IM(Z1[k]), &RE(Z1[k]),
            X_in[2*k], X_in[N2 - 1 - 2*k], RE(sincos[k]), IM(sincos[k]));
    }

#ifdef PROFILE
    count1 = faad_get_ts();
#endif

    /* complex IFFT, any non-scaling FFT can be used here */
    cfftb(mdct->cfft, Z1);

#ifdef PROFILE
    count1 = faad_get_ts() - count1;
#endif

    /* post-IFFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
        RE(x) = RE(Z1[k]);
        IM(x) = IM(Z1[k]);
        ComplexMult(&IM(Z1[k]), &RE(Z1[k]),
            IM(x), RE(x), RE(sincos[k]), IM(sincos[k]));
    }

    /* reordering */
    for (k = 0; k < N8; k+=2)
    {
        X_out[              2*k] =  IM(Z1[N8 +     k]);
        X_out[          2 + 2*k] =  IM(Z1[N8 + 1 + k]);

        X_out[          1 + 2*k] = -RE(Z1[N8 - 1 - k]);
        X_out[          3 + 2*k] = -RE(Z1[N8 - 2 - k]);

        X_out[N4 +          2*k] =  RE(Z1[         k]);
        X_out[N4 +    + 2 + 2*k] =  RE(Z1[     1 + k]);

        X_out[N4 +      1 + 2*k] = -IM(Z1[N4 - 1 - k]);
        X_out[N4 +      3 + 2*k] = -IM(Z1[N4 - 2 - k]);

        X_out[N2 +          2*k] =  RE(Z1[N8 +     k]);
        X_out[N2 +    + 2 + 2*k] =  RE(Z1[N8 + 1 + k]);

        X_out[N2 +      1 + 2*k] = -IM(Z1[N8 - 1 - k]);
        X_out[N2 +      3 + 2*k] = -IM(Z1[N8 - 2 - k]);

        X_out[N2 + N4 +     2*k] = -IM(Z1[         k]);
        X_out[N2 + N4 + 2 + 2*k] = -IM(Z1[     1 + k]);

        X_out[N2 + N4 + 1 + 2*k] =  RE(Z1[N4 - 1 - k]);
        X_out[N2 + N4 + 3 + 2*k] =  RE(Z1[N4 - 2 - k]);
    }

#ifdef PROFILE
    count2 = faad_get_ts() - count2;
    mdct->fft_cycles += count1;
    mdct->cycles += (count2 - count1);
#endif
}

#ifdef USE_SSE
void faad_imdct_sse(mdct_info *mdct, real_t *X_in, real_t *X_out)
{
    uint16_t k;

    ALIGN complex_t Z1[512];
    complex_t *sincos = mdct->sincos;

    uint16_t N  = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;

#ifdef PROFILE
    int64_t count1, count2 = faad_get_ts();
#endif

    /* pre-IFFT complex multiplication */
    for (k = 0; k < N4; k+=4)
    {
        __m128 m12, m13, m14, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11;
        __m128 n12, n13, n14, n0, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11;
        n12 = _mm_load_ps(&X_in[N2 - 2*k - 8]);
        m12 = _mm_load_ps(&X_in[N2 - 2*k - 4]);
        m13 = _mm_load_ps(&X_in[2*k]);
        n13 = _mm_load_ps(&X_in[2*k + 4]);
        m1 = _mm_load_ps(&RE(sincos[k]));
        n1 = _mm_load_ps(&RE(sincos[k+2]));

        m0 = _mm_shuffle_ps(m12, m13, _MM_SHUFFLE(2,0,1,3));
        m2 = _mm_shuffle_ps(m1, m1, _MM_SHUFFLE(2,3,0,1));
        m14 = _mm_shuffle_ps(m0, m0, _MM_SHUFFLE(3,1,2,0));
        n0 = _mm_shuffle_ps(n12, n13, _MM_SHUFFLE(2,0,1,3));
        n2 = _mm_shuffle_ps(n1, n1, _MM_SHUFFLE(2,3,0,1));
        n14 = _mm_shuffle_ps(n0, n0, _MM_SHUFFLE(3,1,2,0));

        m3 = _mm_mul_ps(m14, m1);
        n3 = _mm_mul_ps(n14, n1);
        m4 = _mm_mul_ps(m14, m2);
        n4 = _mm_mul_ps(n14, n2);

        m5 = _mm_shuffle_ps(m3, m4, _MM_SHUFFLE(2,0,2,0));
        n5 = _mm_shuffle_ps(n3, n4, _MM_SHUFFLE(2,0,2,0));
        m6 = _mm_shuffle_ps(m3, m4, _MM_SHUFFLE(3,1,3,1));
        n6 = _mm_shuffle_ps(n3, n4, _MM_SHUFFLE(3,1,3,1));

        m7 = _mm_add_ps(m5, m6);
        n7 = _mm_add_ps(n5, n6);
        m8 = _mm_sub_ps(m5, m6);
        n8 = _mm_sub_ps(n5, n6);

        m9 = _mm_shuffle_ps(m7, m7, _MM_SHUFFLE(3,2,3,2));
        n9 = _mm_shuffle_ps(n7, n7, _MM_SHUFFLE(3,2,3,2));
        m10 = _mm_shuffle_ps(m8, m8, _MM_SHUFFLE(1,0,1,0));
        n10 = _mm_shuffle_ps(n8, n8, _MM_SHUFFLE(1,0,1,0));

        m11 = _mm_unpacklo_ps(m10, m9);
        n11 = _mm_unpacklo_ps(n10, n9);

        _mm_store_ps(&RE(Z1[k]), m11);
        _mm_store_ps(&RE(Z1[k+2]), n11);
    }

#ifdef PROFILE
    count1 = faad_get_ts();
#endif

    /* complex IFFT, any non-scaling FFT can be used here */
    cfftb_sse(mdct->cfft, Z1);

#ifdef PROFILE
    count1 = faad_get_ts() - count1;
#endif

    /* post-IFFT complex multiplication */
    for (k = 0; k < N4; k+=4)
    {
        __m128 m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11;
        __m128 n0, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11;
        m0 = _mm_load_ps(&RE(Z1[k]));
        n0 = _mm_load_ps(&RE(Z1[k+2]));
        m1 = _mm_load_ps(&RE(sincos[k]));
        n1 = _mm_load_ps(&RE(sincos[k+2]));

        m2 = _mm_shuffle_ps(m1, m1, _MM_SHUFFLE(2,3,0,1));
        n2 = _mm_shuffle_ps(n1, n1, _MM_SHUFFLE(2,3,0,1));

        m3 = _mm_mul_ps(m0, m1);
        n3 = _mm_mul_ps(n0, n1);
        m4 = _mm_mul_ps(m0, m2);
        n4 = _mm_mul_ps(n0, n2);

        m5 = _mm_shuffle_ps(m3, m4, _MM_SHUFFLE(2,0,2,0));
        n5 = _mm_shuffle_ps(n3, n4, _MM_SHUFFLE(2,0,2,0));
        m6 = _mm_shuffle_ps(m3, m4, _MM_SHUFFLE(3,1,3,1));
        n6 = _mm_shuffle_ps(n3, n4, _MM_SHUFFLE(3,1,3,1));

        m7 = _mm_add_ps(m5, m6);
        n7 = _mm_add_ps(n5, n6);
        m8 = _mm_sub_ps(m5, m6);
        n8 = _mm_sub_ps(n5, n6);

        m9 = _mm_shuffle_ps(m7, m7, _MM_SHUFFLE(3,2,3,2));
        n9 = _mm_shuffle_ps(n7, n7, _MM_SHUFFLE(3,2,3,2));
        m10 = _mm_shuffle_ps(m8, m8, _MM_SHUFFLE(1,0,1,0));
        n10 = _mm_shuffle_ps(n8, n8, _MM_SHUFFLE(1,0,1,0));

        m11 = _mm_unpacklo_ps(m10, m9);
        n11 = _mm_unpacklo_ps(n10, n9);

        _mm_store_ps(&RE(Z1[k]), m11);
        _mm_store_ps(&RE(Z1[k+2]), n11);
    }

    /* reordering */
    for (k = 0; k < N8; k+=2)
    {
        __m128 m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m13;
        __m128 n4, n5, n6, n7, n8, n9;
        __m128 neg1 = _mm_set_ps(-1.0,  1.0, -1.0,  1.0);
        __m128 neg2 = _mm_set_ps(-1.0, -1.0, -1.0, -1.0);

        m0 = _mm_load_ps(&RE(Z1[k]));
        m1 = _mm_load_ps(&RE(Z1[N8 - 2 - k]));
        m2 = _mm_load_ps(&RE(Z1[N8 + k]));
        m3 = _mm_load_ps(&RE(Z1[N4 - 2 - k]));

        m10 = _mm_mul_ps(m0, neg1);
        m11 = _mm_mul_ps(m1, neg2);
        m13 = _mm_mul_ps(m3, neg1);

        m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(3,1,2,0));
        n4 = _mm_shuffle_ps(m10, m10, _MM_SHUFFLE(3,1,2,0));
        m4 = _mm_shuffle_ps(m11, m11, _MM_SHUFFLE(3,1,2,0));
        n5 = _mm_shuffle_ps(m13, m13, _MM_SHUFFLE(3,1,2,0));

        m6 = _mm_shuffle_ps(m4, m5, _MM_SHUFFLE(3,2,1,0));
        n6 = _mm_shuffle_ps(n4, n5, _MM_SHUFFLE(3,2,1,0));
        m7 = _mm_shuffle_ps(m5, m4, _MM_SHUFFLE(3,2,1,0));
        n7 = _mm_shuffle_ps(n5, n4, _MM_SHUFFLE(3,2,1,0));

        m8 = _mm_shuffle_ps(m6, m6, _MM_SHUFFLE(0,3,1,2));
        n8 = _mm_shuffle_ps(n6, n6, _MM_SHUFFLE(2,1,3,0));
        m9 = _mm_shuffle_ps(m7, m7, _MM_SHUFFLE(2,1,3,0));
        n9 = _mm_shuffle_ps(n7, n7, _MM_SHUFFLE(0,3,1,2));

        _mm_store_ps(&X_out[2*k], m8);
        _mm_store_ps(&X_out[N4 + 2*k], n8);
        _mm_store_ps(&X_out[N2 + 2*k], m9);
        _mm_store_ps(&X_out[N2 + N4 + 2*k], n9);
    }

#ifdef PROFILE
    count2 = faad_get_ts() - count2;
    mdct->fft_cycles += count1;
    mdct->cycles += (count2 - count1);
#endif
}
#endif

#ifdef LTP_DEC
void faad_mdct(mdct_info *mdct, real_t *X_in, real_t *X_out)
{
    uint16_t k;

    complex_t x;
    ALIGN complex_t Z1[512];
    complex_t *sincos = mdct->sincos;

    uint16_t N  = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;

#ifndef FIXED_POINT
	real_t scale = REAL_CONST(N);
#else
	real_t scale = REAL_CONST(4.0/N);
#endif

    /* pre-FFT complex multiplication */
    for (k = 0; k < N8; k++)
    {
        uint16_t n = k << 1;
        RE(x) = X_in[N - N4 - 1 - n] + X_in[N - N4 +     n];
        IM(x) = X_in[    N4 +     n] - X_in[    N4 - 1 - n];

        ComplexMult(&RE(Z1[k]), &IM(Z1[k]),
            RE(x), IM(x), RE(sincos[k]), IM(sincos[k]));

        RE(Z1[k]) = MUL_R(RE(Z1[k]), scale);
        IM(Z1[k]) = MUL_R(IM(Z1[k]), scale);

        RE(x) =  X_in[N2 - 1 - n] - X_in[        n];
        IM(x) =  X_in[N2 +     n] + X_in[N - 1 - n];

        ComplexMult(&RE(Z1[k + N8]), &IM(Z1[k + N8]),
            RE(x), IM(x), RE(sincos[k + N8]), IM(sincos[k + N8]));

        RE(Z1[k + N8]) = MUL_R(RE(Z1[k + N8]), scale);
        IM(Z1[k + N8]) = MUL_R(IM(Z1[k + N8]), scale);
    }

    /* complex FFT, any non-scaling FFT can be used here  */
    cfftf(mdct->cfft, Z1);

    /* post-FFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
        uint16_t n = k << 1;
        ComplexMult(&RE(x), &IM(x),
            RE(Z1[k]), IM(Z1[k]), RE(sincos[k]), IM(sincos[k]));

        X_out[         n] = -RE(x);
        X_out[N2 - 1 - n] =  IM(x);
        X_out[N2 +     n] = -IM(x);
        X_out[N  - 1 - n] =  RE(x);
    }
}
#endif
