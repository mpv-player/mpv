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
** $Id: sbr_qmf.c,v 1.13 2003/09/30 12:43:05 menno Exp $
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
    qmfa_info *qmfa = (qmfa_info*)malloc(sizeof(qmfa_info));
    qmfa->x = (real_t*)malloc(channels * 10 * sizeof(real_t));
    memset(qmfa->x, 0, channels * 10 * sizeof(real_t));

    qmfa->channels = channels;

    return qmfa;
}

void qmfa_end(qmfa_info *qmfa)
{
    if (qmfa)
    {
        if (qmfa->x) free(qmfa->x);
        free(qmfa);
    }
}

void sbr_qmf_analysis_32(sbr_info *sbr, qmfa_info *qmfa, const real_t *input,
                         qmf_t *X, uint8_t offset, uint8_t kx)
{
    uint8_t l;
    real_t u[64];
#ifndef SBR_LOW_POWER
    real_t x[64], y[64];
#else
    real_t y[32];
#endif
    const real_t *inptr = input;

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
            qmfa->x[n] = (*inptr++) >> 5;
#else
            qmfa->x[n] = *inptr++;
#endif
        }

        /* window and summation to create array u */
        for (n = 0; n < 64; n++)
        {
            u[n] = MUL_R_C(qmfa->x[n], qmf_c[2*n]) +
                MUL_R_C(qmfa->x[n + 64], qmf_c[2*(n + 64)]) +
                MUL_R_C(qmfa->x[n + 128], qmf_c[2*(n + 128)]) +
                MUL_R_C(qmfa->x[n + 192], qmf_c[2*(n + 192)]) +
                MUL_R_C(qmfa->x[n + 256], qmf_c[2*(n + 256)]);
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
            QMF_RE(X[((l + offset)<<5) + n]) = u[n] << 1;
#else
            QMF_RE(X[((l + offset)<<5) + n]) = 2. * u[n];
#endif
            } else {
                QMF_RE(X[((l + offset)<<5) + n]) = 0;
            }
        }
#else
        x[0] = u[0];
        x[63] = u[32];
        for (n = 2; n < 64; n += 2)
        {
            x[n-1] = u[(n>>1)];
            x[n] = -u[64-(n>>1)];
        }

        DCT4_64(y, x);

        for (n = 0; n < 32; n++)
        {
            if (n < kx)
            {
#ifdef FIXED_POINT
            QMF_RE(X[((l + offset)<<5) + n]) = y[n] << 1;
            QMF_IM(X[((l + offset)<<5) + n]) = -y[63-n] << 1;
#else
            QMF_RE(X[((l + offset)<<5) + n]) = 2. * y[n];
            QMF_IM(X[((l + offset)<<5) + n]) = -2. * y[63-n];
#endif
            } else {
                QMF_RE(X[((l + offset)<<5) + n]) = 0;
                QMF_IM(X[((l + offset)<<5) + n]) = 0;
            }
        }
#endif
    }
}

qmfs_info *qmfs_init(uint8_t channels)
{
    int size = 0;
    qmfs_info *qmfs = (qmfs_info*)malloc(sizeof(qmfs_info));

    qmfs->v[0] = (real_t*)malloc(channels * 10 * sizeof(real_t));
    memset(qmfs->v[0], 0, channels * 10 * sizeof(real_t));
    qmfs->v[1] = (real_t*)malloc(channels * 10 * sizeof(real_t));
    memset(qmfs->v[1], 0, channels * 10 * sizeof(real_t));

    qmfs->v_index = 0;

    qmfs->channels = channels;

    return qmfs;
}

void qmfs_end(qmfs_info *qmfs)
{
    if (qmfs)
    {
        if (qmfs->v[0]) free(qmfs->v[0]);
        if (qmfs->v[1]) free(qmfs->v[1]);
        free(qmfs);
    }
}

#ifdef SBR_LOW_POWER
void sbr_qmf_synthesis_64(sbr_info *sbr, qmfs_info *qmfs, const qmf_t *X,
                          real_t *output)
{
    uint8_t l;
    int16_t n, k;
    real_t x[64];
    real_t *outptr = output;


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
        for (k = 0; k < 64; k++)
        {
#ifdef FIXED_POINT
            x[k] = QMF_RE(X[(l<<6) + k]);
#else
            x[k] = QMF_RE(X[(l<<6) + k]) / 32.;
#endif
        }

        DCT2_64_unscaled(x, x);

        for (n = 0; n < 32; n++)
        {
            v0[n+32] = x[n];
            v1[n] = x[n+32];
        }
        v0[0] = v1[0];
        for (n = 1; n < 32; n++)
            {
            v0[32 - n] =  v0[n + 32];
            v1[n + 32] = -v1[32 - n];
            }
        v1[32] = 0;

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k++)
        {
            *outptr++ = MUL_R_C(v0[k], qmf_c[k]) +
                MUL_R_C(v0[64 + k], qmf_c[64 + k]) +
                MUL_R_C(v0[128 + k], qmf_c[128 + k]) +
                MUL_R_C(v0[192 + k], qmf_c[192 + k]) +
                MUL_R_C(v0[256 + k], qmf_c[256 + k]) +
                MUL_R_C(v0[320 + k], qmf_c[320 + k]) +
                MUL_R_C(v0[384 + k], qmf_c[384 + k]) +
                MUL_R_C(v0[448 + k], qmf_c[448 + k]) +
                MUL_R_C(v0[512 + k], qmf_c[512 + k]) +
                MUL_R_C(v0[576 + k], qmf_c[576 + k]);
        }
    }
}
#else
void sbr_qmf_synthesis_64(sbr_info *sbr, qmfs_info *qmfs, const qmf_t *X,
                          real_t *output)
{
    uint8_t l;
    int16_t n, k;
    real_t x1[64], x2[64];
    real_t *outptr = output;


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
        for (k = 0; k < 64; k++)
        {
            x1[k] = QMF_RE(X[(l<<6) + k])/64.;
            x2[63 - k] = QMF_IM(X[(l<<6) + k])/64.;
        }

        DCT4_64(x1, x1);
        DCT4_64(x2, x2);

        for (n = 0; n < 64; n+=2)
        {
            v0[n]      =  x2[n]   - x1[n];
            v0[n+1]    = -x2[n+1] - x1[n+1];
            v1[63-n]   =  x2[n]   + x1[n];
            v1[63-n-1] = -x2[n+1] + x1[n+1];
        }

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k++)
        {
            *outptr++ = MUL_R_C(v0[k], qmf_c[k]) +
                MUL_R_C(v0[64 + k], qmf_c[64 + k]) +
                MUL_R_C(v0[128 + k], qmf_c[128 + k]) +
                MUL_R_C(v0[192 + k], qmf_c[192 + k]) +
                MUL_R_C(v0[256 + k], qmf_c[256 + k]) +
                MUL_R_C(v0[320 + k], qmf_c[320 + k]) +
                MUL_R_C(v0[384 + k], qmf_c[384 + k]) +
                MUL_R_C(v0[448 + k], qmf_c[448 + k]) +
                MUL_R_C(v0[512 + k], qmf_c[512 + k]) +
                MUL_R_C(v0[576 + k], qmf_c[576 + k]);
        }
    }
}
#endif

#endif
