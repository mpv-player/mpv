/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
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
** $Id: mdct.c,v 1.26 2003/07/29 08:20:12 menno Exp $
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
#ifndef FIXED_POINT
#ifdef _MSC_VER
#pragma warning(disable:4305)
#pragma warning(disable:4244)
#endif
real_t const_tab[][5] =
{
    { COEF_CONST(0.0312500000), COEF_CONST(0.9999952938), COEF_CONST(0.0030679568),
        COEF_CONST(0.9999999265), COEF_CONST(0.0003834952) }, /* 2048 */
    { COEF_CONST(0.0322748612), COEF_CONST(0.9999946356), COEF_CONST(0.0032724866),
        COEF_CONST(0.9999999404), COEF_CONST(0.0004090615) }, /* 1920 */
    { COEF_CONST(0.0441941738), COEF_CONST(0.9999811649), COEF_CONST(0.0061358847),
        COEF_CONST(0.9999997020), COEF_CONST(0.0007669903) }, /* 1024 */
    { COEF_CONST(0.0456435465), COEF_CONST(0.9999786019), COEF_CONST(0.0065449383),
        COEF_CONST(0.9999996424), COEF_CONST(0.0008181230) }, /* 960 */
    { COEF_CONST(0.0883883476), COEF_CONST(0.9996988177), COEF_CONST(0.0245412290),
        COEF_CONST(0.9999952912), COEF_CONST(0.0030679568) }, /* 256 */
    { COEF_CONST(0.0912870929), COEF_CONST(0.9996573329), COEF_CONST(0.0261769500),
        COEF_CONST(0.9999946356), COEF_CONST(0.0032724866) }  /* 240 */
#ifdef SSR_DEC
   ,{ COEF_CONST(0.062500000), COEF_CONST(0.999924702), COEF_CONST(0.012271538),
        COEF_CONST(0.999998823), COEF_CONST(0.00153398) }, /* 512 */
    { COEF_CONST(0.176776695), COEF_CONST(0.995184727), COEF_CONST(0.09801714),
        COEF_CONST(0.999924702), COEF_CONST(0.012271538) }  /* 64 */
#endif
};
#else
real_t const_tab[][5] =
{
    { COEF_CONST(1), COEF_CONST(0.9999952938), COEF_CONST(0.0030679568),
        COEF_CONST(0.9999999265), COEF_CONST(0.0003834952) }, /* 2048 */
    { COEF_CONST(/* sqrt(1024/960) */ 1.03279556), COEF_CONST(0.9999946356), COEF_CONST(0.0032724866),
        COEF_CONST(0), COEF_CONST(0.0004090615) }, /* 1920 */
    { COEF_CONST(1), COEF_CONST(0.9999811649), COEF_CONST(0.0061358847),
        COEF_CONST(0.9999997020), COEF_CONST(0.0007669903) }, /* 1024 */
    { COEF_CONST(/* sqrt(512/480) */ 1.03279556), COEF_CONST(0.9999786019), COEF_CONST(0.0065449383),
        COEF_CONST(0.9999996424), COEF_CONST(0.0008181230) }, /* 960 */
    { COEF_CONST(1), COEF_CONST(0.9996988177), COEF_CONST(0.0245412290),
        COEF_CONST(0.9999952912), COEF_CONST(0.0030679568) }, /* 256 */
    { COEF_CONST(/* sqrt(256/240) */ 1.03279556), COEF_CONST(0.9996573329), COEF_CONST(0.0261769500),
        COEF_CONST(0.9999946356), COEF_CONST(0.0032724866) }  /* 240 */
#ifdef SSR_DEC
   ,{ COEF_CONST(0), COEF_CONST(0.999924702), COEF_CONST(0.012271538),
        COEF_CONST(0.999998823), COEF_CONST(0.00153398) }, /* 512 */
    { COEF_CONST(0), COEF_CONST(0.995184727), COEF_CONST(0.09801714),
        COEF_CONST(0.999924702), COEF_CONST(0.012271538) }  /* 64 */
#endif
};
#endif

uint8_t map_N_to_idx(uint16_t N)
{
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

mdct_info *faad_mdct_init(uint16_t N)
{
    uint16_t k, N_idx;
    real_t cangle, sangle, c, s, cold;
	real_t scale;

    mdct_info *mdct = (mdct_info*)malloc(sizeof(mdct_info));

    assert(N % 8 == 0);

    mdct->N = N;
    mdct->sincos = (complex_t*)malloc(N/4*sizeof(complex_t));
    mdct->Z1 = (complex_t*)malloc(N/4*sizeof(complex_t));

    N_idx = map_N_to_idx(N);

    scale = const_tab[N_idx][0];
    cangle = const_tab[N_idx][1];
    sangle = const_tab[N_idx][2];
    c = const_tab[N_idx][3];
    s = const_tab[N_idx][4];

    for (k = 0; k < N/4; k++)
    {
        RE(mdct->sincos[k]) = -1*MUL_C_C(c,scale);
        IM(mdct->sincos[k]) = -1*MUL_C_C(s,scale);

        cold = c;
        c = MUL_C_C(c,cangle) - MUL_C_C(s,sangle);
        s = MUL_C_C(s,cangle) + MUL_C_C(cold,sangle);
    }

    /* initialise fft */
    mdct->cfft = cffti(N/4);

    return mdct;
}

void faad_mdct_end(mdct_info *mdct)
{
    if (mdct != NULL)
    {
        cfftu(mdct->cfft);

        if (mdct->Z1) free(mdct->Z1);
        if (mdct->sincos) free(mdct->sincos);

        free(mdct);
    }
}

void faad_imdct(mdct_info *mdct, real_t *X_in, real_t *X_out)
{
    uint16_t k;

    complex_t x;
    complex_t *Z1 = mdct->Z1;
    complex_t *sincos = mdct->sincos;

    uint16_t N  = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;

    /* pre-IFFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
        uint16_t n = k << 1;
        RE(x) = X_in[         n];
        IM(x) = X_in[N2 - 1 - n];
        RE(Z1[k]) = MUL_R_C(IM(x), RE(sincos[k])) - MUL_R_C(RE(x), IM(sincos[k]));
        IM(Z1[k]) = MUL_R_C(RE(x), RE(sincos[k])) + MUL_R_C(IM(x), IM(sincos[k]));
    }

    /* complex IFFT */
    cfftb(mdct->cfft, Z1);

    /* post-IFFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
        uint16_t n = k << 1;
        RE(x) = RE(Z1[k]);
        IM(x) = IM(Z1[k]);

        RE(Z1[k]) = MUL_R_C(RE(x), RE(sincos[k])) - MUL_R_C(IM(x), IM(sincos[k]));
        IM(Z1[k]) = MUL_R_C(IM(x), RE(sincos[k])) + MUL_R_C(RE(x), IM(sincos[k]));
    }

    /* reordering */
    for (k = 0; k < N8; k++)
    {
        uint16_t n = k << 1;
        X_out[              n] =  IM(Z1[N8 +     k]);
        X_out[          1 + n] = -RE(Z1[N8 - 1 - k]);
        X_out[N4 +          n] =  RE(Z1[         k]);
        X_out[N4 +      1 + n] = -IM(Z1[N4 - 1 - k]);
        X_out[N2 +          n] =  RE(Z1[N8 +     k]);
        X_out[N2 +      1 + n] = -IM(Z1[N8 - 1 - k]);
        X_out[N2 + N4 +     n] = -IM(Z1[         k]);
        X_out[N2 + N4 + 1 + n] =  RE(Z1[N4 - 1 - k]);
    }
}

#ifdef LTP_DEC
void faad_mdct(mdct_info *mdct, real_t *X_in, real_t *X_out)
{
    uint16_t k;

    complex_t x;
    complex_t *Z1 = mdct->Z1;
    complex_t *sincos = mdct->sincos;

    uint16_t N  = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;

	real_t scale = REAL_CONST(N);

    /* pre-FFT complex multiplication */
    for (k = 0; k < N8; k++)
    {
        uint16_t n = k << 1;
        RE(x) = X_in[N - N4 - 1 - n] + X_in[N - N4 +     n];
        IM(x) = X_in[    N4 +     n] - X_in[    N4 - 1 - n];

        RE(Z1[k]) = -MUL_R_C(RE(x), RE(sincos[k])) - MUL_R_C(IM(x), IM(sincos[k]));
        IM(Z1[k]) = -MUL_R_C(IM(x), RE(sincos[k])) + MUL_R_C(RE(x), IM(sincos[k]));

        RE(x) =  X_in[N2 - 1 - n] - X_in[        n];
        IM(x) =  X_in[N2 +     n] + X_in[N - 1 - n];

        RE(Z1[k + N8]) = -MUL_R_C(RE(x), RE(sincos[k + N8])) - MUL_R_C(IM(x), IM(sincos[k + N8]));
        IM(Z1[k + N8]) = -MUL_R_C(IM(x), RE(sincos[k + N8])) + MUL_R_C(RE(x), IM(sincos[k + N8]));
    }

    /* complex FFT */
    cfftf(mdct->cfft, Z1);

    /* post-FFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
        uint16_t n = k << 1;
        RE(x) = MUL(MUL_R_C(RE(Z1[k]), RE(sincos[k])) + MUL_R_C(IM(Z1[k]), IM(sincos[k])), scale);
        IM(x) = MUL(MUL_R_C(IM(Z1[k]), RE(sincos[k])) - MUL_R_C(RE(Z1[k]), IM(sincos[k])), scale);

        X_out[         n] =  RE(x);
        X_out[N2 - 1 - n] = -IM(x);
        X_out[N2 +     n] =  IM(x);
        X_out[N  - 1 - n] = -RE(x);
    }
}
#endif
