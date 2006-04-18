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
** $Id: sbr_qmf.c,v 1.27 2004/09/04 14:56:28 menno Exp $
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

	/* x is implemented as double ringbuffer */
    qmfa->x = (real_t*)faad_malloc(2 * channels * 10 * sizeof(real_t));
    memset(qmfa->x, 0, 2 * channels * 10 * sizeof(real_t));

	/* ringbuffer index */
	qmfa->x_index = 0;

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
                         qmf_t X[MAX_NTSRHFG][64], uint8_t offset, uint8_t kx)
{
    ALIGN real_t u[64];
#ifndef SBR_LOW_POWER
    ALIGN real_t in_real[32], in_imag[32], out_real[32], out_imag[32];
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
		/* input buffer is not shifted anymore, x is implemented as double ringbuffer */
        //memmove(qmfa->x + 32, qmfa->x, (320-32)*sizeof(real_t));

        /* add new samples to input buffer x */
        for (n = 32 - 1; n >= 0; n--)
        {
#ifdef FIXED_POINT
            qmfa->x[qmfa->x_index + n] = qmfa->x[qmfa->x_index + n + 320] = (input[in++]) >> 4;
#else
            qmfa->x[qmfa->x_index + n] = qmfa->x[qmfa->x_index + n + 320] = input[in++];
#endif
        }

        /* window and summation to create array u */
        for (n = 0; n < 64; n++)
        {
            u[n] = MUL_F(qmfa->x[qmfa->x_index + n], qmf_c[2*n]) +
                MUL_F(qmfa->x[qmfa->x_index + n + 64], qmf_c[2*(n + 64)]) +
                MUL_F(qmfa->x[qmfa->x_index + n + 128], qmf_c[2*(n + 128)]) +
                MUL_F(qmfa->x[qmfa->x_index + n + 192], qmf_c[2*(n + 192)]) +
                MUL_F(qmfa->x[qmfa->x_index + n + 256], qmf_c[2*(n + 256)]);
        }

		/* update ringbuffer index */
		qmfa->x_index -= 32;
		if (qmfa->x_index < 0)
			qmfa->x_index = (320-32);

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
                QMF_RE(X[l + offset][n]) = u[n] /*<< 1*/;
#else
                QMF_RE(X[l + offset][n]) = 2. * u[n];
#endif
            } else {
                QMF_RE(X[l + offset][n]) = 0;
            }
        }
#else

        // Reordering of data moved from DCT_IV to here
        in_imag[31] = u[1];
        in_real[0] = u[0];
        for (n = 1; n < 31; n++)
        {
            in_imag[31 - n] = u[n+1];
            in_real[n] = -u[64-n];
        }
        in_imag[0] = u[32];
        in_real[31] = -u[33];

        // dct4_kernel is DCT_IV without reordering which is done before and after FFT
        dct4_kernel(in_real, in_imag, out_real, out_imag);

        // Reordering of data moved from DCT_IV to here
        for (n = 0; n < 16; n++) {
            if (2*n+1 < kx) {
#ifdef FIXED_POINT
                QMF_RE(X[l + offset][2*n])   = out_real[n];
                QMF_IM(X[l + offset][2*n])   = out_imag[n];
                QMF_RE(X[l + offset][2*n+1]) = -out_imag[31-n];
                QMF_IM(X[l + offset][2*n+1]) = -out_real[31-n];
#else
                QMF_RE(X[l + offset][2*n])   = 2. * out_real[n];
                QMF_IM(X[l + offset][2*n])   = 2. * out_imag[n];
                QMF_RE(X[l + offset][2*n+1]) = -2. * out_imag[31-n];
                QMF_IM(X[l + offset][2*n+1]) = -2. * out_real[31-n];
#endif
            } else {
                if (2*n < kx) {
#ifdef FIXED_POINT
                    QMF_RE(X[l + offset][2*n])   = out_real[n];
                    QMF_IM(X[l + offset][2*n])   = out_imag[n];
#else
                    QMF_RE(X[l + offset][2*n])   = 2. * out_real[n];
                    QMF_IM(X[l + offset][2*n])   = 2. * out_imag[n];
#endif
                }
                else {
                    QMF_RE(X[l + offset][2*n]) = 0;
                    QMF_IM(X[l + offset][2*n]) = 0;
                }
                QMF_RE(X[l + offset][2*n+1]) = 0;
                QMF_IM(X[l + offset][2*n+1]) = 0;
            }
        }
#endif
    }
}

static const complex_t qmf32_pre_twiddle[] =
{
    { FRAC_CONST(0.999924701839145), FRAC_CONST(-0.012271538285720) },
    { FRAC_CONST(0.999322384588350), FRAC_CONST(-0.036807222941359) },
    { FRAC_CONST(0.998118112900149), FRAC_CONST(-0.061320736302209) },
    { FRAC_CONST(0.996312612182778), FRAC_CONST(-0.085797312344440) },
    { FRAC_CONST(0.993906970002356), FRAC_CONST(-0.110222207293883) },
    { FRAC_CONST(0.990902635427780), FRAC_CONST(-0.134580708507126) },
    { FRAC_CONST(0.987301418157858), FRAC_CONST(-0.158858143333861) },
    { FRAC_CONST(0.983105487431216), FRAC_CONST(-0.183039887955141) },
    { FRAC_CONST(0.978317370719628), FRAC_CONST(-0.207111376192219) },
    { FRAC_CONST(0.972939952205560), FRAC_CONST(-0.231058108280671) },
    { FRAC_CONST(0.966976471044852), FRAC_CONST(-0.254865659604515) },
    { FRAC_CONST(0.960430519415566), FRAC_CONST(-0.278519689385053) },
    { FRAC_CONST(0.953306040354194), FRAC_CONST(-0.302005949319228) },
    { FRAC_CONST(0.945607325380521), FRAC_CONST(-0.325310292162263) },
    { FRAC_CONST(0.937339011912575), FRAC_CONST(-0.348418680249435) },
    { FRAC_CONST(0.928506080473216), FRAC_CONST(-0.371317193951838) },
    { FRAC_CONST(0.919113851690058), FRAC_CONST(-0.393992040061048) },
    { FRAC_CONST(0.909167983090522), FRAC_CONST(-0.416429560097637) },
    { FRAC_CONST(0.898674465693954), FRAC_CONST(-0.438616238538528) },
    { FRAC_CONST(0.887639620402854), FRAC_CONST(-0.460538710958240) },
    { FRAC_CONST(0.876070094195407), FRAC_CONST(-0.482183772079123) },
    { FRAC_CONST(0.863972856121587), FRAC_CONST(-0.503538383725718) },
    { FRAC_CONST(0.851355193105265), FRAC_CONST(-0.524589682678469) },
    { FRAC_CONST(0.838224705554838), FRAC_CONST(-0.545324988422046) },
    { FRAC_CONST(0.824589302785025), FRAC_CONST(-0.565731810783613) },
    { FRAC_CONST(0.810457198252595), FRAC_CONST(-0.585797857456439) },
    { FRAC_CONST(0.795836904608884), FRAC_CONST(-0.605511041404326) },
    { FRAC_CONST(0.780737228572094), FRAC_CONST(-0.624859488142386) },
    { FRAC_CONST(0.765167265622459), FRAC_CONST(-0.643831542889791) },
    { FRAC_CONST(0.749136394523459), FRAC_CONST(-0.662415777590172) },
    { FRAC_CONST(0.732654271672413), FRAC_CONST(-0.680600997795453) },
    { FRAC_CONST(0.715730825283819), FRAC_CONST(-0.698376249408973) }
};

qmfs_info *qmfs_init(uint8_t channels)
{
    qmfs_info *qmfs = (qmfs_info*)faad_malloc(sizeof(qmfs_info));

	/* v is a double ringbuffer */
    qmfs->v = (real_t*)faad_malloc(2 * channels * 20 * sizeof(real_t));
    memset(qmfs->v, 0, 2 * channels * 20 * sizeof(real_t));

    qmfs->v_index = 0;

    qmfs->channels = channels;

    return qmfs;
}

void qmfs_end(qmfs_info *qmfs)
{
    if (qmfs)
    {
        if (qmfs->v) faad_free(qmfs->v);
        faad_free(qmfs);
    }
}

#ifdef SBR_LOW_POWER

void sbr_qmf_synthesis_32(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output)
{
    ALIGN real_t x[16];
    ALIGN real_t y[16];
    int16_t n, k, out = 0;
    uint8_t l;

    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        /* shift buffers */
        /* we are not shifting v, it is a double ringbuffer */
        //memmove(qmfs->v + 64, qmfs->v, (640-64)*sizeof(real_t));

        /* calculate 64 samples */
        for (k = 0; k < 16; k++)
        {
#ifdef FIXED_POINT
            y[k] = (QMF_RE(X[l][k]) - QMF_RE(X[l][31 - k]));
            x[k] = (QMF_RE(X[l][k]) + QMF_RE(X[l][31 - k]));
#else
            y[k] = (QMF_RE(X[l][k]) - QMF_RE(X[l][31 - k])) / 32.0;
            x[k] = (QMF_RE(X[l][k]) + QMF_RE(X[l][31 - k])) / 32.0;
#endif
        }

        /* even n samples */
        DCT2_16_unscaled(x, x);
        /* odd n samples */
        DCT4_16(y, y);

        for (n = 8; n < 24; n++)
        {
            qmfs->v[qmfs->v_index + n*2] = qmfs->v[qmfs->v_index + 640 + n*2] = x[n-8];
            qmfs->v[qmfs->v_index + n*2+1] = qmfs->v[qmfs->v_index + 640 + n*2+1] = y[n-8];
        }
        for (n = 0; n < 16; n++)
        {
            qmfs->v[qmfs->v_index + n] = qmfs->v[qmfs->v_index + 640 + n] = qmfs->v[qmfs->v_index + 32-n];
        }
        qmfs->v[qmfs->v_index + 48] = qmfs->v[qmfs->v_index + 640 + 48] = 0;
        for (n = 1; n < 16; n++)
        {
            qmfs->v[qmfs->v_index + 48+n] = qmfs->v[qmfs->v_index + 640 + 48+n] = -qmfs->v[qmfs->v_index + 48-n];
        }

        /* calculate 32 output samples and window */
        for (k = 0; k < 32; k++)
        {
            output[out++] = MUL_F(qmfs->v[qmfs->v_index + k], qmf_c[2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 96 + k], qmf_c[64 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 128 + k], qmf_c[128 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 224 + k], qmf_c[192 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 256 + k], qmf_c[256 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 352 + k], qmf_c[320 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 384 + k], qmf_c[384 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 480 + k], qmf_c[448 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 512 + k], qmf_c[512 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 608 + k], qmf_c[576 + 2*k]);
        }

        /* update the ringbuffer index */
        qmfs->v_index -= 64;
        if (qmfs->v_index < 0)
            qmfs->v_index = (640-64);
    }
}

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
        /* shift buffers */
        /* we are not shifting v, it is a double ringbuffer */
        //memmove(qmfs->v + 128, qmfs->v, (1280-128)*sizeof(real_t));

        /* calculate 128 samples */
        for (k = 0; k < 32; k++)
        {
#ifdef FIXED_POINT
            y[k] = (QMF_RE(X[l][k]) - QMF_RE(X[l][63 - k]));
            x[k] = (QMF_RE(X[l][k]) + QMF_RE(X[l][63 - k]));
#else
            y[k] = (QMF_RE(X[l][k]) - QMF_RE(X[l][63 - k])) / 32.0;
            x[k] = (QMF_RE(X[l][k]) + QMF_RE(X[l][63 - k])) / 32.0;
#endif
        }

        /* even n samples */
        DCT2_32_unscaled(x, x);
        /* odd n samples */
        DCT4_32(y, y);

        for (n = 16; n < 48; n++)
        {
            qmfs->v[qmfs->v_index + n*2]   = qmfs->v[qmfs->v_index + 1280 + n*2]   = x[n-16];
            qmfs->v[qmfs->v_index + n*2+1] = qmfs->v[qmfs->v_index + 1280 + n*2+1] = y[n-16];
        }
        for (n = 0; n < 32; n++)
        {
            qmfs->v[qmfs->v_index + n] = qmfs->v[qmfs->v_index + 1280 + n] = qmfs->v[qmfs->v_index + 64-n];
        }
        qmfs->v[qmfs->v_index + 96] = qmfs->v[qmfs->v_index + 1280 + 96] = 0;
        for (n = 1; n < 32; n++)
        {
            qmfs->v[qmfs->v_index + 96+n] = qmfs->v[qmfs->v_index + 1280 + 96+n] = -qmfs->v[qmfs->v_index + 96-n];
        }

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k++)
        {
            output[out++] = MUL_F(qmfs->v[qmfs->v_index + k], qmf_c[k]) +
                MUL_F(qmfs->v[qmfs->v_index + 192 + k], qmf_c[64 + k]) +
                MUL_F(qmfs->v[qmfs->v_index + 256 + k], qmf_c[128 + k]) +
                MUL_F(qmfs->v[qmfs->v_index + 256 + 192 + k], qmf_c[128 + 64 + k]) +
                MUL_F(qmfs->v[qmfs->v_index + 512 + k], qmf_c[256 + k]) +
                MUL_F(qmfs->v[qmfs->v_index + 512 + 192 + k], qmf_c[256 + 64 + k]) +
                MUL_F(qmfs->v[qmfs->v_index + 768 + k], qmf_c[384 + k]) +
                MUL_F(qmfs->v[qmfs->v_index + 768 + 192 + k], qmf_c[384 + 64 + k]) +
                MUL_F(qmfs->v[qmfs->v_index + 1024 + k], qmf_c[512 + k]) +
                MUL_F(qmfs->v[qmfs->v_index + 1024 + 192 + k], qmf_c[512 + 64 + k]);
        }

        /* update the ringbuffer index */
        qmfs->v_index -= 128;
        if (qmfs->v_index < 0)
            qmfs->v_index = (1280-128);
    }
}
#else
void sbr_qmf_synthesis_32(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output)
{
    ALIGN real_t x1[32], x2[32];
#ifndef FIXED_POINT
    real_t scale = 1.f/64.f;
#endif
    int16_t n, k, out = 0;
    uint8_t l;


    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        /* shift buffer v */
        /* buffer is not shifted, we are using a ringbuffer */
        //memmove(qmfs->v + 64, qmfs->v, (640-64)*sizeof(real_t));

        /* calculate 64 samples */
        /* complex pre-twiddle */
        for (k = 0; k < 32; k++)
        {
            x1[k] = MUL_F(QMF_RE(X[l][k]), RE(qmf32_pre_twiddle[k])) - MUL_F(QMF_IM(X[l][k]), IM(qmf32_pre_twiddle[k]));
            x2[k] = MUL_F(QMF_IM(X[l][k]), RE(qmf32_pre_twiddle[k])) + MUL_F(QMF_RE(X[l][k]), IM(qmf32_pre_twiddle[k]));

#ifndef FIXED_POINT
            x1[k] *= scale;
            x2[k] *= scale;
#else
            x1[k] >>= 1;
            x2[k] >>= 1;
#endif
        }

        /* transform */
        DCT4_32(x1, x1);
        DST4_32(x2, x2);

        for (n = 0; n < 32; n++)
        {
            qmfs->v[qmfs->v_index + n]      = qmfs->v[qmfs->v_index + 640 + n]      = -x1[n] + x2[n];
            qmfs->v[qmfs->v_index + 63 - n] = qmfs->v[qmfs->v_index + 640 + 63 - n] =  x1[n] + x2[n];
        }

        /* calculate 32 output samples and window */
        for (k = 0; k < 32; k++)
        {
            output[out++] = MUL_F(qmfs->v[qmfs->v_index + k], qmf_c[2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 96 + k], qmf_c[64 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 128 + k], qmf_c[128 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 224 + k], qmf_c[192 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 256 + k], qmf_c[256 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 352 + k], qmf_c[320 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 384 + k], qmf_c[384 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 480 + k], qmf_c[448 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 512 + k], qmf_c[512 + 2*k]) +
                MUL_F(qmfs->v[qmfs->v_index + 608 + k], qmf_c[576 + 2*k]);
        }

        /* update ringbuffer index */
        qmfs->v_index -= 64;
        if (qmfs->v_index < 0)
            qmfs->v_index = (640 - 64);
    }
}

void sbr_qmf_synthesis_64(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output)
{
//    ALIGN real_t x1[64], x2[64];
#ifndef SBR_LOW_POWER
    ALIGN real_t in_real1[32], in_imag1[32], out_real1[32], out_imag1[32];
    ALIGN real_t in_real2[32], in_imag2[32], out_real2[32], out_imag2[32];
#endif
    qmf_t * pX;
    real_t * pring_buffer_1, * pring_buffer_3;
//    real_t * ptemp_1, * ptemp_2;
#ifdef PREFER_POINTERS
    // These pointers are used if target platform has autoinc address generators
    real_t * pring_buffer_2, * pring_buffer_4;
    real_t * pring_buffer_5, * pring_buffer_6;
    real_t * pring_buffer_7, * pring_buffer_8;
    real_t * pring_buffer_9, * pring_buffer_10;
    const real_t * pqmf_c_1, * pqmf_c_2, * pqmf_c_3, * pqmf_c_4;
    const real_t * pqmf_c_5, * pqmf_c_6, * pqmf_c_7, * pqmf_c_8;
    const real_t * pqmf_c_9, * pqmf_c_10;
#endif // #ifdef PREFER_POINTERS
#ifndef FIXED_POINT
    real_t scale = 1.f/64.f;
#endif
    int16_t n, k, out = 0;
    uint8_t l;


    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        /* shift buffer v */
		/* buffer is not shifted, we use double ringbuffer */
		//memmove(qmfs->v + 128, qmfs->v, (1280-128)*sizeof(real_t));

        /* calculate 128 samples */
#ifndef FIXED_POINT

        pX = X[l];

        in_imag1[31] = scale*QMF_RE(pX[1]);
        in_real1[0]  = scale*QMF_RE(pX[0]);
        in_imag2[31] = scale*QMF_IM(pX[63-1]);
        in_real2[0]  = scale*QMF_IM(pX[63-0]);
        for (k = 1; k < 31; k++)
        {
            in_imag1[31 - k] = scale*QMF_RE(pX[2*k + 1]);
            in_real1[     k] = scale*QMF_RE(pX[2*k    ]);
            in_imag2[31 - k] = scale*QMF_IM(pX[63 - (2*k + 1)]);
            in_real2[     k] = scale*QMF_IM(pX[63 - (2*k    )]);
        }
        in_imag1[0]  = scale*QMF_RE(pX[63]);
        in_real1[31] = scale*QMF_RE(pX[62]);
        in_imag2[0]  = scale*QMF_IM(pX[63-63]);
        in_real2[31] = scale*QMF_IM(pX[63-62]);

#else

        pX = X[l];

        in_imag1[31] = QMF_RE(pX[1]) >> 1;
        in_real1[0]  = QMF_RE(pX[0]) >> 1;
        in_imag2[31] = QMF_IM(pX[62]) >> 1;
        in_real2[0]  = QMF_IM(pX[63]) >> 1;
        for (k = 1; k < 31; k++)
        {
            in_imag1[31 - k] = QMF_RE(pX[2*k + 1]) >> 1;
            in_real1[     k] = QMF_RE(pX[2*k    ]) >> 1;
            in_imag2[31 - k] = QMF_IM(pX[63 - (2*k + 1)]) >> 1;
            in_real2[     k] = QMF_IM(pX[63 - (2*k    )]) >> 1;
        }
        in_imag1[0]  = QMF_RE(pX[63]) >> 1;
        in_real1[31] = QMF_RE(pX[62]) >> 1;
        in_imag2[0]  = QMF_IM(pX[0]) >> 1;
        in_real2[31] = QMF_IM(pX[1]) >> 1;

#endif


        // dct4_kernel is DCT_IV without reordering which is done before and after FFT
        dct4_kernel(in_real1, in_imag1, out_real1, out_imag1);
        dct4_kernel(in_real2, in_imag2, out_real2, out_imag2);


        pring_buffer_1 = qmfs->v + qmfs->v_index;
        pring_buffer_3 = pring_buffer_1 + 1280;
#ifdef PREFER_POINTERS
        pring_buffer_2 = pring_buffer_1 + 127;
        pring_buffer_4 = pring_buffer_1 + (1280 + 127);
#endif // #ifdef PREFER_POINTERS
//        ptemp_1 = x1;
//        ptemp_2 = x2;
#ifdef PREFER_POINTERS
        for (n = 0; n < 32; n ++)
        {
            //real_t x1 = *ptemp_1++;
            //real_t x2 = *ptemp_2++;
            // pring_buffer_3 and pring_buffer_4 are needed only for double ring buffer
            *pring_buffer_1++ = *pring_buffer_3++ = out_real2[n] - out_real1[n];
            *pring_buffer_2-- = *pring_buffer_4-- = out_real2[n] + out_real1[n];
            //x1 = *ptemp_1++;
            //x2 = *ptemp_2++;
            *pring_buffer_1++ = *pring_buffer_3++ = out_imag2[31-n] + out_imag1[31-n];
            *pring_buffer_2-- = *pring_buffer_4-- = out_imag2[31-n] - out_imag1[31-n];
        }
#else // #ifdef PREFER_POINTERS

        for (n = 0; n < 32; n++)
        {
            // pring_buffer_3 and pring_buffer_4 are needed only for double ring buffer
            pring_buffer_1[2*n]         = pring_buffer_3[2*n]         = out_real2[n] - out_real1[n];
            pring_buffer_1[127-2*n]     = pring_buffer_3[127-2*n]     = out_real2[n] + out_real1[n];
            pring_buffer_1[2*n+1]       = pring_buffer_3[2*n+1]       = out_imag2[31-n] + out_imag1[31-n];
            pring_buffer_1[127-(2*n+1)] = pring_buffer_3[127-(2*n+1)] = out_imag2[31-n] - out_imag1[31-n];
        }

#endif // #ifdef PREFER_POINTERS

        pring_buffer_1 = qmfs->v + qmfs->v_index;
#ifdef PREFER_POINTERS
        pring_buffer_2 = pring_buffer_1 + 192;
        pring_buffer_3 = pring_buffer_1 + 256;
        pring_buffer_4 = pring_buffer_1 + (256 + 192);
        pring_buffer_5 = pring_buffer_1 + 512;
        pring_buffer_6 = pring_buffer_1 + (512 + 192);
        pring_buffer_7 = pring_buffer_1 + 768;
        pring_buffer_8 = pring_buffer_1 + (768 + 192);
        pring_buffer_9 = pring_buffer_1 + 1024;
        pring_buffer_10 = pring_buffer_1 + (1024 + 192);
        pqmf_c_1 = qmf_c;
        pqmf_c_2 = qmf_c + 64;
        pqmf_c_3 = qmf_c + 128;
        pqmf_c_4 = qmf_c + 192;
        pqmf_c_5 = qmf_c + 256;
        pqmf_c_6 = qmf_c + 320;
        pqmf_c_7 = qmf_c + 384;
        pqmf_c_8 = qmf_c + 448;
        pqmf_c_9 = qmf_c + 512;
        pqmf_c_10 = qmf_c + 576;
#endif // #ifdef PREFER_POINTERS

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k++)
        {
#ifdef PREFER_POINTERS
            output[out++] =
                MUL_F(*pring_buffer_1++,  *pqmf_c_1++) +
                MUL_F(*pring_buffer_2++,  *pqmf_c_2++) +
                MUL_F(*pring_buffer_3++,  *pqmf_c_3++) +
                MUL_F(*pring_buffer_4++,  *pqmf_c_4++) +
                MUL_F(*pring_buffer_5++,  *pqmf_c_5++) +
                MUL_F(*pring_buffer_6++,  *pqmf_c_6++) +
                MUL_F(*pring_buffer_7++,  *pqmf_c_7++) +
                MUL_F(*pring_buffer_8++,  *pqmf_c_8++) +
                MUL_F(*pring_buffer_9++,  *pqmf_c_9++) +
                MUL_F(*pring_buffer_10++, *pqmf_c_10++);
#else // #ifdef PREFER_POINTERS
            output[out++] =
                MUL_F(pring_buffer_1[k+0],          qmf_c[k+0])   +
                MUL_F(pring_buffer_1[k+192],        qmf_c[k+64])  +
                MUL_F(pring_buffer_1[k+256],        qmf_c[k+128]) +
                MUL_F(pring_buffer_1[k+(256+192)],  qmf_c[k+192]) +
                MUL_F(pring_buffer_1[k+512],        qmf_c[k+256]) +
                MUL_F(pring_buffer_1[k+(512+192)],  qmf_c[k+320]) +
                MUL_F(pring_buffer_1[k+768],        qmf_c[k+384]) +
                MUL_F(pring_buffer_1[k+(768+192)],  qmf_c[k+448]) +
                MUL_F(pring_buffer_1[k+1024],       qmf_c[k+512]) +
                MUL_F(pring_buffer_1[k+(1024+192)], qmf_c[k+576]);
#endif // #ifdef PREFER_POINTERS
        }

        /* update ringbuffer index */
        qmfs->v_index -= 128;
        if (qmfs->v_index < 0)
            qmfs->v_index = (1280 - 128);
    }
}
#endif

#endif
