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
** Initially modified for use with MPlayer by Arpad Gereöffy on 2003/08/30
** $Id: sbr_qmf.c,v 1.3 2004/06/02 22:59:03 diego Exp $
** detailed CVS changelog at http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
**/

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC


#include <stdlib.h>
#include <string.h>
#include "sbr_dct.h"
#include "sbr_qmf.h"
#include "sbr_qmf_c.h"
#include "sbr_syntax.h"


qmfa_info *qmfa_init(uint8_t channels)
{
    qmfa_info *qmfa = (qmfa_info*)faad_malloc(sizeof(qmfa_info));
    qmfa->x = (real_t*)faad_malloc(channels * 10 * sizeof(real_t));
    memset(qmfa->x, 0, channels * 10 * sizeof(real_t));

    qmfa->channels = channels;

    return qmfa;
}

void qmfa_end(qmfa_info *qmfa)
{
    if (qmfa)
    {
        if (qmfa->x) faad_free(qmfa->x);
        faad_free(qmfa);
    }
}

void sbr_qmf_analysis_32(sbr_info *sbr, qmfa_info *qmfa, const real_t *input,
                         qmf_t X[MAX_NTSRHFG][32], uint8_t offset, uint8_t kx)
{
    ALIGN real_t u[64];
#ifndef SBR_LOW_POWER
    ALIGN real_t x[64], y[64];
#else
    ALIGN real_t y[32];
#endif
    uint16_t in = 0;
    uint8_t l;

    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        int16_t n;

        /* shift input buffer x */
        memmove(qmfa->x + 32, qmfa->x, (320-32)*sizeof(real_t));

        /* add new samples to input buffer x */
        for (n = 32 - 1; n >= 0; n--)
        {
#ifdef FIXED_POINT
            qmfa->x[n] = (input[in++]) >> 5;
#else
            qmfa->x[n] = input[in++];
#endif
        }

        /* window and summation to create array u */
        for (n = 0; n < 64; n++)
        {
            u[n] = MUL_F(qmfa->x[n], qmf_c[2*n]) +
                MUL_F(qmfa->x[n + 64], qmf_c[2*(n + 64)]) +
                MUL_F(qmfa->x[n + 128], qmf_c[2*(n + 128)]) +
                MUL_F(qmfa->x[n + 192], qmf_c[2*(n + 192)]) +
                MUL_F(qmfa->x[n + 256], qmf_c[2*(n + 256)]);
        }

        /* calculate 32 subband samples by introducing X */
#ifdef SBR_LOW_POWER
        y[0] = u[48];
        for (n = 1; n < 16; n++)
            y[n] = u[n+48] + u[48-n];
        for (n = 16; n < 32; n++)
            y[n] = -u[n-16] + u[48-n];

        DCT3_32_unscaled(u, y);

        for (n = 0; n < 32; n++)
        {
            if (n < kx)
            {
#ifdef FIXED_POINT
                QMF_RE(X[l + offset][n]) = u[n] << 1;
#else
                QMF_RE(X[l + offset][n]) = 2. * u[n];
#endif
            } else {
                QMF_RE(X[l + offset][n]) = 0;
            }
        }
#else
        x[0] = u[0];
        for (n = 0; n < 31; n++)
        {
            x[2*n+1] = u[n+1] + u[63-n];
            x[2*n+2] = u[n+1] - u[63-n];
        }
        x[63] = u[32];

        DCT4_64_kernel(y, x);

        for (n = 0; n < 32; n++)
        {
            if (n < kx)
            {
#ifdef FIXED_POINT
                QMF_RE(X[l + offset][n]) = y[n] << 1;
                QMF_IM(X[l + offset][n]) = -y[63-n] << 1;
#else
                QMF_RE(X[l + offset][n]) = 2. * y[n];
                QMF_IM(X[l + offset][n]) = -2. * y[63-n];
#endif
            } else {
                QMF_RE(X[l + offset][n]) = 0;
                QMF_IM(X[l + offset][n]) = 0;
            }
        }
#endif
    }
}

qmfs_info *qmfs_init(uint8_t channels)
{
    qmfs_info *qmfs = (qmfs_info*)faad_malloc(sizeof(qmfs_info));

#ifndef SBR_LOW_POWER
    qmfs->v[0] = (real_t*)faad_malloc(channels * 10 * sizeof(real_t));
    memset(qmfs->v[0], 0, channels * 10 * sizeof(real_t));
    qmfs->v[1] = (real_t*)faad_malloc(channels * 10 * sizeof(real_t));
    memset(qmfs->v[1], 0, channels * 10 * sizeof(real_t));
#else
    qmfs->v[0] = (real_t*)faad_malloc(channels * 20 * sizeof(real_t));
    memset(qmfs->v[0], 0, channels * 20 * sizeof(real_t));
    qmfs->v[1] = NULL;
#endif

    qmfs->v_index = 0;

    qmfs->channels = channels;

#ifdef USE_SSE
    if (cpu_has_sse())
    {
        qmfs->qmf_func = sbr_qmf_synthesis_64_sse;
    } else {
        qmfs->qmf_func = sbr_qmf_synthesis_64;
    }
#endif

    return qmfs;
}

void qmfs_end(qmfs_info *qmfs)
{
    if (qmfs)
    {
        if (qmfs->v[0]) faad_free(qmfs->v[0]);
#ifndef SBR_LOW_POWER
        if (qmfs->v[1]) faad_free(qmfs->v[1]);
#endif
        faad_free(qmfs);
    }
}

#ifdef SBR_LOW_POWER
void sbr_qmf_synthesis_64(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output)
{
    ALIGN real_t x[64];
    ALIGN real_t y[64];
    int16_t n, k, out = 0;
    uint8_t l;


    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        //real_t *v0, *v1;

        /* shift buffers */
        //memmove(qmfs->v[0] + 64, qmfs->v[0], (640-64)*sizeof(real_t));
        //memmove(qmfs->v[1] + 64, qmfs->v[1], (640-64)*sizeof(real_t));
        memmove(qmfs->v[0] + 128, qmfs->v[0], (1280-128)*sizeof(real_t));

        //v0 = qmfs->v[qmfs->v_index];
        //v1 = qmfs->v[(qmfs->v_index + 1) & 0x1];
        //qmfs->v_index = (qmfs->v_index + 1) & 0x1;

        /* calculate 128 samples */
        for (k = 0; k < 64; k++)
        {
#ifdef FIXED_POINT
            x[k] = QMF_RE(X[l][k]);
#else
            x[k] = QMF_RE(X[l][k]) / 32.;
#endif
        }

        for (n = 0; n < 32; n++)
        {
            y[2*n]   = -x[2*n];
            y[2*n+1] =  x[2*n+1];
        }

        DCT2_64_unscaled(x, x);

        for (n = 0; n < 64; n++)
        {
            qmfs->v[0][n+32] = x[n];
        }
        for (n = 0; n < 32; n++)
        {
            qmfs->v[0][31 - n] = x[n + 1];
        }
        DST2_64_unscaled(x, y);
        qmfs->v[0][96] = 0;
        for (n = 1; n < 32; n++)
        {
            qmfs->v[0][n + 96] = x[n-1];
        }

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k++)
        {
#if 1
             output[out++] = MUL_F(qmfs->v[0][k], qmf_c[k]) +
                 MUL_F(qmfs->v[0][192 + k], qmf_c[64 + k]) +
                 MUL_F(qmfs->v[0][256 + k], qmf_c[128 + k]) +
                 MUL_F(qmfs->v[0][256 + 192 + k], qmf_c[128 + 64 + k]) +
                 MUL_F(qmfs->v[0][512 + k], qmf_c[256 + k]) +
                 MUL_F(qmfs->v[0][512 + 192 + k], qmf_c[256 + 64 + k]) +
                 MUL_F(qmfs->v[0][768 + k], qmf_c[384 + k]) +
                 MUL_F(qmfs->v[0][768 + 192 + k], qmf_c[384 + 64 + k]) +
                 MUL_F(qmfs->v[0][1024 + k], qmf_c[512 + k]) +
                 MUL_F(qmfs->v[0][1024 + 192 + k], qmf_c[512 + 64 + k]);
#else
            output[out++] = MUL_F(v0[k], qmf_c[k]) +
                MUL_F(v0[64 + k], qmf_c[64 + k]) +
                MUL_F(v0[128 + k], qmf_c[128 + k]) +
                MUL_F(v0[192 + k], qmf_c[192 + k]) +
                MUL_F(v0[256 + k], qmf_c[256 + k]) +
                MUL_F(v0[320 + k], qmf_c[320 + k]) +
                MUL_F(v0[384 + k], qmf_c[384 + k]) +
                MUL_F(v0[448 + k], qmf_c[448 + k]) +
                MUL_F(v0[512 + k], qmf_c[512 + k]) +
                MUL_F(v0[576 + k], qmf_c[576 + k]);
#endif
        }
    }
}

void sbr_qmf_synthesis_64_sse(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                              real_t *output)
{
    ALIGN real_t x[64];
    ALIGN real_t y[64];
    ALIGN real_t y2[64];
    int16_t n, k, out = 0;
    uint8_t l;

    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        //real_t *v0, *v1;

        /* shift buffers */
        //memmove(qmfs->v[0] + 64, qmfs->v[0], (640-64)*sizeof(real_t));
        //memmove(qmfs->v[1] + 64, qmfs->v[1], (640-64)*sizeof(real_t));
        memmove(qmfs->v[0] + 128, qmfs->v[0], (1280-128)*sizeof(real_t));

        //v0 = qmfs->v[qmfs->v_index];
        //v1 = qmfs->v[(qmfs->v_index + 1) & 0x1];
        //qmfs->v_index = (qmfs->v_index + 1) & 0x1;

        /* calculate 128 samples */
        for (k = 0; k < 64; k++)
        {
#ifdef FIXED_POINT
            x[k] = QMF_RE(X[l][k]);
#else
            x[k] = QMF_RE(X[l][k]) / 32.;
#endif
        }

        for (n = 0; n < 32; n++)
        {
            y[2*n]   = -x[2*n];
            y[2*n+1] =  x[2*n+1];
        }

        DCT2_64_unscaled(x, x);

        for (n = 0; n < 64; n++)
        {
            qmfs->v[0][n+32] = x[n];
        }
        for (n = 0; n < 32; n++)
        {
            qmfs->v[0][31 - n] = x[n + 1];
        }

        DST2_64_unscaled(x, y);
        qmfs->v[0][96] = 0;
        for (n = 1; n < 32; n++)
        {
            qmfs->v[0][n + 96] = x[n-1];
        }

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k++)
        {
#if 1
             output[out++] = MUL_F(qmfs->v[0][k], qmf_c[k]) +
                 MUL_F(qmfs->v[0][192 + k], qmf_c[64 + k]) +
                 MUL_F(qmfs->v[0][256 + k], qmf_c[128 + k]) +
                 MUL_F(qmfs->v[0][256 + 192 + k], qmf_c[128 + 64 + k]) +
                 MUL_F(qmfs->v[0][512 + k], qmf_c[256 + k]) +
                 MUL_F(qmfs->v[0][512 + 192 + k], qmf_c[256 + 64 + k]) +
                 MUL_F(qmfs->v[0][768 + k], qmf_c[384 + k]) +
                 MUL_F(qmfs->v[0][768 + 192 + k], qmf_c[384 + 64 + k]) +
                 MUL_F(qmfs->v[0][1024 + k], qmf_c[512 + k]) +
                 MUL_F(qmfs->v[0][1024 + 192 + k], qmf_c[512 + 64 + k]);
#else
            output[out++] = MUL_F(v0[k], qmf_c[k]) +
                MUL_F(v0[64 + k], qmf_c[64 + k]) +
                MUL_F(v0[128 + k], qmf_c[128 + k]) +
                MUL_F(v0[192 + k], qmf_c[192 + k]) +
                MUL_F(v0[256 + k], qmf_c[256 + k]) +
                MUL_F(v0[320 + k], qmf_c[320 + k]) +
                MUL_F(v0[384 + k], qmf_c[384 + k]) +
                MUL_F(v0[448 + k], qmf_c[448 + k]) +
                MUL_F(v0[512 + k], qmf_c[512 + k]) +
                MUL_F(v0[576 + k], qmf_c[576 + k]);
#endif
        }
    }
}
#else
void sbr_qmf_synthesis_64(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output)
{
    ALIGN real_t x1[64], x2[64];
    real_t scale = 1.f/64.f;
    int16_t n, k, out = 0;
    uint8_t l;


    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        real_t *v0, *v1;

        /* shift buffers */
        memmove(qmfs->v[0] + 64, qmfs->v[0], (640-64)*sizeof(real_t));
        memmove(qmfs->v[1] + 64, qmfs->v[1], (640-64)*sizeof(real_t));

        v0 = qmfs->v[qmfs->v_index];
        v1 = qmfs->v[(qmfs->v_index + 1) & 0x1];
        qmfs->v_index = (qmfs->v_index + 1) & 0x1;

        /* calculate 128 samples */
        x1[0] = scale*QMF_RE(X[l][0]);
        x2[63] = scale*QMF_IM(X[l][0]);
        for (k = 0; k < 31; k++)
        {
            x1[2*k+1] = scale*(QMF_RE(X[l][2*k+1]) - QMF_RE(X[l][2*k+2]));
            x1[2*k+2] = scale*(QMF_RE(X[l][2*k+1]) + QMF_RE(X[l][2*k+2]));

            x2[61 - 2*k] = scale*(QMF_IM(X[l][2*k+2]) - QMF_IM(X[l][2*k+1]));
            x2[62 - 2*k] = scale*(QMF_IM(X[l][2*k+2]) + QMF_IM(X[l][2*k+1]));
        }
        x1[63] = scale*QMF_RE(X[l][63]);
        x2[0] = scale*QMF_IM(X[l][63]);

        DCT4_64_kernel(x1, x1);
        DCT4_64_kernel(x2, x2);

        for (n = 0; n < 32; n++)
        {
            v0[   2*n]   =  x2[2*n]   - x1[2*n];
            v1[63-2*n]   =  x2[2*n]   + x1[2*n];
            v0[   2*n+1] = -x2[2*n+1] - x1[2*n+1];
            v1[62-2*n]   = -x2[2*n+1] + x1[2*n+1];
        }

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k++)
        {
            output[out++] = MUL_F(v0[k], qmf_c[k]) +
                MUL_F(v0[64 + k], qmf_c[64 + k]) +
                MUL_F(v0[128 + k], qmf_c[128 + k]) +
                MUL_F(v0[192 + k], qmf_c[192 + k]) +
                MUL_F(v0[256 + k], qmf_c[256 + k]) +
                MUL_F(v0[320 + k], qmf_c[320 + k]) +
                MUL_F(v0[384 + k], qmf_c[384 + k]) +
                MUL_F(v0[448 + k], qmf_c[448 + k]) +
                MUL_F(v0[512 + k], qmf_c[512 + k]) +
                MUL_F(v0[576 + k], qmf_c[576 + k]);
        }
    }
}

#ifdef USE_SSE
void memmove_sse_576(real_t *out, const real_t *in)
{
    __m128 m[144];
    uint16_t i;

    for (i = 0; i < 144; i++)
    {
        m[i] = _mm_load_ps(&in[i*4]);
    }
    for (i = 0; i < 144; i++)
    {
        _mm_store_ps(&out[i*4], m[i]);
    }
}

void sbr_qmf_synthesis_64_sse(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                              real_t *output)
{
    ALIGN real_t x1[64], x2[64];
    real_t scale = 1.f/64.f;
    int16_t n, k, out = 0;
    uint8_t l;


    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        real_t *v0, *v1;

        /* shift buffers */
        memmove_sse_576(qmfs->v[0] + 64, qmfs->v[0]);
        memmove_sse_576(qmfs->v[1] + 64, qmfs->v[1]);

        v0 = qmfs->v[qmfs->v_index];
        v1 = qmfs->v[(qmfs->v_index + 1) & 0x1];
        qmfs->v_index = (qmfs->v_index + 1) & 0x1;

        /* calculate 128 samples */
        x1[0] = scale*QMF_RE(X[l][0]);
        x2[63] = scale*QMF_IM(X[l][0]);
        for (k = 0; k < 31; k++)
        {
            x1[2*k+1] = scale*(QMF_RE(X[l][2*k+1]) - QMF_RE(X[l][2*k+2]));
            x1[2*k+2] = scale*(QMF_RE(X[l][2*k+1]) + QMF_RE(X[l][2*k+2]));

            x2[61 - 2*k] = scale*(QMF_IM(X[l][2*k+2]) - QMF_IM(X[l][2*k+1]));
            x2[62 - 2*k] = scale*(QMF_IM(X[l][2*k+2]) + QMF_IM(X[l][2*k+1]));
        }
        x1[63] = scale*QMF_RE(X[l][63]);
        x2[0] = scale*QMF_IM(X[l][63]);

        DCT4_64_kernel(x1, x1);
        DCT4_64_kernel(x2, x2);

        for (n = 0; n < 32; n++)
        {
            v0[    2*n   ] =  x2[2*n]   - x1[2*n];
            v1[63- 2*n   ] =  x2[2*n]   + x1[2*n];
            v0[    2*n+1 ] = -x2[2*n+1] - x1[2*n+1];
            v1[63-(2*n+1)] = -x2[2*n+1] + x1[2*n+1];
        }

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k+=4)
        {
            __m128 m0, m1, m2, m3, m4, m5, m6, m7, m8, m9;
            __m128 c0, c1, c2, c3, c4, c5, c6, c7, c8, c9;
            __m128 s1, s2, s3, s4, s5, s6, s7, s8, s9;

            m0 = _mm_load_ps(&v0[k]);
            m1 = _mm_load_ps(&v0[k + 64]);
            m2 = _mm_load_ps(&v0[k + 128]);
            m3 = _mm_load_ps(&v0[k + 192]);
            m4 = _mm_load_ps(&v0[k + 256]);
            c0 = _mm_load_ps(&qmf_c[k]);
            c1 = _mm_load_ps(&qmf_c[k + 64]);
            c2 = _mm_load_ps(&qmf_c[k + 128]);
            c3 = _mm_load_ps(&qmf_c[k + 192]);
            c4 = _mm_load_ps(&qmf_c[k + 256]);

            m0 = _mm_mul_ps(m0, c0);
            m1 = _mm_mul_ps(m1, c1);
            m2 = _mm_mul_ps(m2, c2);
            m3 = _mm_mul_ps(m3, c3);
            m4 = _mm_mul_ps(m4, c4);

            s1 = _mm_add_ps(m0, m1);
            s2 = _mm_add_ps(m2, m3);
            s6 = _mm_add_ps(s1, s2);

            m5 = _mm_load_ps(&v0[k + 320]);
            m6 = _mm_load_ps(&v0[k + 384]);
            m7 = _mm_load_ps(&v0[k + 448]);
            m8 = _mm_load_ps(&v0[k + 512]);
            m9 = _mm_load_ps(&v0[k + 576]);
            c5 = _mm_load_ps(&qmf_c[k + 320]);
            c6 = _mm_load_ps(&qmf_c[k + 384]);
            c7 = _mm_load_ps(&qmf_c[k + 448]);
            c8 = _mm_load_ps(&qmf_c[k + 512]);
            c9 = _mm_load_ps(&qmf_c[k + 576]);

            m5 = _mm_mul_ps(m5, c5);
            m6 = _mm_mul_ps(m6, c6);
            m7 = _mm_mul_ps(m7, c7);
            m8 = _mm_mul_ps(m8, c8);
            m9 = _mm_mul_ps(m9, c9);

            s3 = _mm_add_ps(m4, m5);
            s4 = _mm_add_ps(m6, m7);
            s5 = _mm_add_ps(m8, m9);
            s7 = _mm_add_ps(s3, s4);
            s8 = _mm_add_ps(s5, s6);
            s9 = _mm_add_ps(s7, s8);

            _mm_store_ps(&output[out], s9);
            out += 4;
        }
    }
}
#endif
#endif

#endif
