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
** $Id: sbr_hfgen.c,v 1.1 2003/07/29 08:20:13 menno Exp $
**/

/* High Frequency generation */

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include "sbr_syntax.h"
#include "sbr_hfgen.h"
#include "sbr_fbt.h"

void hf_generation(sbr_info *sbr, qmf_t *Xlow,
                   qmf_t *Xhigh
#ifdef SBR_LOW_POWER
                   ,real_t *deg
#endif
                   ,uint8_t ch)
{
    uint8_t l, i, x;
    complex_t alpha_0[64], alpha_1[64];
#ifdef SBR_LOW_POWER
    real_t rxx[64];
#endif


    calc_chirp_factors(sbr, ch);

    if ((ch == 0) && (sbr->Reset))
        patch_construction(sbr);

    /* calculate the prediction coefficients */
    calc_prediction_coef(sbr, Xlow, alpha_0, alpha_1
#ifdef SBR_LOW_POWER
        , rxx
#endif
        );

#ifdef SBR_LOW_POWER
    calc_aliasing_degree(sbr, rxx, deg);
#endif

    /* actual HF generation */
    for (i = 0; i < sbr->noPatches; i++)
    {
        for (x = 0; x < sbr->patchNoSubbands[i]; x++)
        {
            complex_t a0, a1;
            real_t bw, bw2;
            uint8_t q, p, k, g;

            /* find the low and high band for patching */
            k = sbr->kx + x;
            for (q = 0; q < i; q++)
            {
                k += sbr->patchNoSubbands[q];
            }
            p = sbr->patchStartSubband[i] + x;

#ifdef SBR_LOW_POWER
            if (x != 0 /*x < sbr->patchNoSubbands[i]-1*/)
                deg[k] = deg[p];
            else
                deg[k] = 0;
#endif

            g = sbr->table_map_k_to_g[k];

            bw = sbr->bwArray[ch][g];
            bw2 = MUL_C_C(bw, bw);

            /* do the patching */
            /* with or without filtering */
            if (bw2 > 0)
            {
                RE(a0) = MUL_R_C(RE(alpha_0[p]), bw);
                RE(a1) = MUL_R_C(RE(alpha_1[p]), bw2);
#ifndef SBR_LOW_POWER
                IM(a0) = MUL_R_C(IM(alpha_0[p]), bw);
                IM(a1) = MUL_R_C(IM(alpha_1[p]), bw2);
#endif

                for (l = sbr->t_E[ch][0]; l < sbr->t_E[ch][sbr->L_E[ch]]; l++)
                {
                    QMF_RE(Xhigh[((l + tHFAdj)<<6) + k]) = QMF_RE(Xlow[((l + tHFAdj)<<5) + p]);
#ifndef SBR_LOW_POWER
                    QMF_IM(Xhigh[((l + tHFAdj)<<6) + k]) = QMF_IM(Xlow[((l + tHFAdj)<<5) + p]);
#endif

#ifdef SBR_LOW_POWER
                    QMF_RE(Xhigh[((l + tHFAdj)<<6) + k]) += (
                        MUL(RE(a0), QMF_RE(Xlow[((l - 1 + tHFAdj)<<5) + p])) +
                        MUL(RE(a1), QMF_RE(Xlow[((l - 2 + tHFAdj)<<5) + p])));
#else
                    QMF_RE(Xhigh[((l + tHFAdj)<<6) + k]) += (
                        RE(a0) * QMF_RE(Xlow[((l - 1 + tHFAdj)<<5) + p]) -
                        IM(a0) * QMF_IM(Xlow[((l - 1 + tHFAdj)<<5) + p]) +
                        RE(a1) * QMF_RE(Xlow[((l - 2 + tHFAdj)<<5) + p]) -
                        IM(a1) * QMF_IM(Xlow[((l - 2 + tHFAdj)<<5) + p]));
                    QMF_IM(Xhigh[((l + tHFAdj)<<6) + k]) += (
                        IM(a0) * QMF_RE(Xlow[((l - 1 + tHFAdj)<<5) + p]) +
                        RE(a0) * QMF_IM(Xlow[((l - 1 + tHFAdj)<<5) + p]) +
                        IM(a1) * QMF_RE(Xlow[((l - 2 + tHFAdj)<<5) + p]) +
                        RE(a1) * QMF_IM(Xlow[((l - 2 + tHFAdj)<<5) + p]));
#endif
                }
            } else {
                for (l = sbr->t_E[ch][0]; l < sbr->t_E[ch][sbr->L_E[ch]]; l++)
                {
                    QMF_RE(Xhigh[((l + tHFAdj)<<6) + k]) = QMF_RE(Xlow[((l + tHFAdj)<<5) + p]);
#ifndef SBR_LOW_POWER
                    QMF_IM(Xhigh[((l + tHFAdj)<<6) + k]) = QMF_IM(Xlow[((l + tHFAdj)<<5) + p]);
#endif
                }
            }
        }
    }

#if 0
    if (sbr->frame == 179)
    {
        for (l = 0; l < 64; l++)
        {
            printf("%d %.3f\n", l, deg[l]);
        }
    }
#endif

    if (sbr->Reset)
    {
        limiter_frequency_table(sbr);
    }
}

typedef struct
{
    complex_t r01;
    complex_t r02;
    complex_t r11;
    complex_t r12;
    complex_t r22;
    real_t det;
} acorr_coef;

#define SBR_ABS(A) ((A) < 0) ? -(A) : (A)

static void auto_correlation(acorr_coef *ac, qmf_t *buffer,
                             uint8_t bd, uint8_t len)
{
    int8_t j, jminus1, jminus2;
    const real_t rel = COEF_CONST(0.9999999999999); // 1 / (1 + 1e-6f);

#ifdef FIXED_POINT
    /*
     *  For computing the covariance matrix and the filter coefficients
     *  in fixed point, all values are normalised so that the fixed point
     *  values don't overflow.
     */
    uint32_t max = 0;
    uint32_t pow2, exp;

    for (j = tHFAdj-2; j < len + tHFAdj; j++)
    {
        max = max(SBR_ABS(QMF_RE(buffer[j*32 + bd])>>REAL_BITS), max);
    }

    /* find the first power of 2 bigger than max to avoid division */
    pow2 = 1;
    exp = 0;
    while (max > pow2)
    {
        pow2 <<= 1;
        exp++;
    }

    /* give some more space */
//    if (exp > 3)
//        exp -= 3;
#endif

    memset(ac, 0, sizeof(acorr_coef));

    for (j = tHFAdj; j < len + tHFAdj; j++)
    {
        jminus1 = j - 1;
        jminus2 = jminus1 - 1;

#ifdef SBR_LOW_POWER
#ifdef FIXED_POINT
        /* normalisation with rounding */
        RE(ac->r01) += MUL(((QMF_RE(buffer[j*32 + bd])+(1<<(exp-1)))>>exp), ((QMF_RE(buffer[jminus1*32 + bd])+(1<<(exp-1)))>>exp));
        RE(ac->r02) += MUL(((QMF_RE(buffer[j*32 + bd])+(1<<(exp-1)))>>exp), ((QMF_RE(buffer[jminus2*32 + bd])+(1<<(exp-1)))>>exp));
        RE(ac->r11) += MUL(((QMF_RE(buffer[jminus1*32 + bd])+(1<<(exp-1)))>>exp), ((QMF_RE(buffer[jminus1*32 + bd])+(1<<(exp-1)))>>exp));
        RE(ac->r12) += MUL(((QMF_RE(buffer[jminus1*32 + bd])+(1<<(exp-1)))>>exp), ((QMF_RE(buffer[jminus2*32 + bd])+(1<<(exp-1)))>>exp));
        RE(ac->r22) += MUL(((QMF_RE(buffer[jminus2*32 + bd])+(1<<(exp-1)))>>exp), ((QMF_RE(buffer[jminus2*32 + bd])+(1<<(exp-1)))>>exp));
#else
        RE(ac->r01) += QMF_RE(buffer[j*32 + bd]) * QMF_RE(buffer[jminus1*32 + bd]);
        RE(ac->r02) += QMF_RE(buffer[j*32 + bd]) * QMF_RE(buffer[jminus2*32 + bd]);
        RE(ac->r11) += QMF_RE(buffer[jminus1*32 + bd]) * QMF_RE(buffer[jminus1*32 + bd]);
        RE(ac->r12) += QMF_RE(buffer[jminus1*32 + bd]) * QMF_RE(buffer[jminus2*32 + bd]);
        RE(ac->r22) += QMF_RE(buffer[jminus2*32 + bd]) * QMF_RE(buffer[jminus2*32 + bd]);
#endif
#else
        RE(ac->r01) += QMF_RE(buffer[j*32 + bd]) * QMF_RE(buffer[jminus1*32 + bd]) +
            QMF_IM(buffer[j*32 + bd]) * QMF_IM(buffer[jminus1*32 + bd]);

        IM(ac->r01) += QMF_IM(buffer[j*32 + bd]) * QMF_RE(buffer[jminus1*32 + bd]) -
            QMF_RE(buffer[j*32 + bd]) * QMF_IM(buffer[jminus1*32 + bd]);

        RE(ac->r02) += QMF_RE(buffer[j*32 + bd]) * QMF_RE(buffer[jminus2*32 + bd]) +
            QMF_IM(buffer[j*32 + bd]) * QMF_IM(buffer[jminus2*32 + bd]);

        IM(ac->r02) += QMF_IM(buffer[j*32 + bd]) * QMF_RE(buffer[jminus2*32 + bd]) -
            QMF_RE(buffer[j*32 + bd]) * QMF_IM(buffer[jminus2*32 + bd]);

        RE(ac->r11) += QMF_RE(buffer[jminus1*32 + bd]) * QMF_RE(buffer[jminus1*32 + bd]) +
            QMF_IM(buffer[jminus1*32 + bd]) * QMF_IM(buffer[jminus1*32 + bd]);

        RE(ac->r12) += QMF_RE(buffer[jminus1*32 + bd]) * QMF_RE(buffer[jminus2*32 + bd]) +
            QMF_IM(buffer[jminus1*32 + bd]) * QMF_IM(buffer[jminus2*32 + bd]);

        IM(ac->r12) += QMF_IM(buffer[jminus1*32 + bd]) * QMF_RE(buffer[jminus2*32 + bd]) -
            QMF_RE(buffer[jminus1*32 + bd]) * QMF_IM(buffer[jminus2*32 + bd]);

        RE(ac->r22) += QMF_RE(buffer[jminus2*32 + bd]) * QMF_RE(buffer[jminus2*32 + bd]) +
            QMF_IM(buffer[jminus2*32 + bd]) * QMF_IM(buffer[jminus2*32 + bd]);
#endif
    }

#ifdef SBR_LOW_POWER
    ac->det = MUL(RE(ac->r11), RE(ac->r22)) - MUL_R_C(MUL(RE(ac->r12), RE(ac->r12)), rel);
#else
    ac->det = RE(ac->r11) * RE(ac->r22) - rel * (RE(ac->r12) * RE(ac->r12) + IM(ac->r12) * IM(ac->r12));
#endif

#if 0
    if (ac->det != 0)
        printf("%f %f\n", ac->det, max);
#endif
}

static void calc_prediction_coef(sbr_info *sbr, qmf_t *Xlow,
                                 complex_t *alpha_0, complex_t *alpha_1
#ifdef SBR_LOW_POWER
                                 , real_t *rxx
#endif
                                 )
{
    uint8_t k;
    real_t tmp;
    acorr_coef ac;

    for (k = 1; k < sbr->kx; k++)
    {
        auto_correlation(&ac, Xlow, k, 38);

#ifdef SBR_LOW_POWER
        if (ac.det == 0)
        {
            RE(alpha_1[k]) = 0;
        } else {
            tmp = MUL(RE(ac.r01), RE(ac.r12)) - MUL(RE(ac.r02), RE(ac.r11));
            RE(alpha_1[k]) = SBR_DIV(tmp, ac.det);
        }

        if (RE(ac.r11) == 0)
        {
            RE(alpha_0[k]) = 0;
        } else {
            tmp = RE(ac.r01) + MUL(RE(alpha_1[k]), RE(ac.r12));
            RE(alpha_0[k]) = -SBR_DIV(tmp, RE(ac.r11));
        }

        if ((RE(alpha_0[k]) >= REAL_CONST(4)) || (RE(alpha_1[k]) >= REAL_CONST(4)))
        {
            RE(alpha_0[k]) = REAL_CONST(0);
            RE(alpha_1[k]) = REAL_CONST(0);
        }

        /* reflection coefficient */
        if (RE(ac.r11) == REAL_CONST(0.0))
        {
            rxx[k] = REAL_CONST(0.0);
        } else {
            rxx[k] = -SBR_DIV(RE(ac.r01), RE(ac.r11));
            if (rxx[k] > REAL_CONST(1.0)) rxx[k] = REAL_CONST(1.0);
            if (rxx[k] < REAL_CONST(-1.0)) rxx[k] = REAL_CONST(-1.0);
        }
#else
        if (ac.det == 0)
        {
            RE(alpha_1[k]) = 0;
            IM(alpha_1[k]) = 0;
        } else {
            tmp = 1.0 / ac.det;
            RE(alpha_1[k]) = (RE(ac.r01) * RE(ac.r12) - IM(ac.r01) * IM(ac.r12) - RE(ac.r02) * RE(ac.r11)) * tmp;
            IM(alpha_1[k]) = (IM(ac.r01) * RE(ac.r12) + RE(ac.r01) * IM(ac.r12) - IM(ac.r02) * RE(ac.r11)) * tmp;
        }

        if (RE(ac.r11) == 0)
        {
            RE(alpha_0[k]) = 0;
            IM(alpha_0[k]) = 0;
        } else {
            tmp = 1.0f / RE(ac.r11);
            RE(alpha_0[k]) = -(RE(ac.r01) + RE(alpha_1[k]) * RE(ac.r12) + IM(alpha_1[k]) * IM(ac.r12)) * tmp;
            IM(alpha_0[k]) = -(IM(ac.r01) + IM(alpha_1[k]) * RE(ac.r12) - RE(alpha_1[k]) * IM(ac.r12)) * tmp;
        }

        if ((RE(alpha_0[k])*RE(alpha_0[k]) + IM(alpha_0[k])*IM(alpha_0[k]) >= 16) ||
            (RE(alpha_1[k])*RE(alpha_1[k]) + IM(alpha_1[k])*IM(alpha_1[k]) >= 16))
        {
            RE(alpha_0[k]) = 0;
            IM(alpha_0[k]) = 0;
            RE(alpha_1[k]) = 0;
            IM(alpha_1[k]) = 0;
        }
#endif
    }
}

#ifdef SBR_LOW_POWER
static void calc_aliasing_degree(sbr_info *sbr, real_t *rxx, real_t *deg)
{
    uint8_t k;

    rxx[0] = REAL_CONST(0.0);
    deg[1] = REAL_CONST(0.0);

    for (k = 2; k < sbr->k0; k++)
    {
        deg[k] = 0.0;

        if ((k % 2 == 0) && (rxx[k] < REAL_CONST(0.0)))
        {
            if (rxx[k-1] < 0.0)
            {
                deg[k] = REAL_CONST(1.0);

                if (rxx[k-2] > REAL_CONST(0.0))
                {
                    deg[k-1] = REAL_CONST(1.0) - MUL(rxx[k-1], rxx[k-1]);
                }
            } else if (rxx[k-2] > REAL_CONST(0.0)) {
                deg[k]   = REAL_CONST(1.0) - MUL(rxx[k-1], rxx[k-1]);
            }
        }

        if ((k % 2 == 1) && (rxx[k] > REAL_CONST(0.0)))
        {
            if (rxx[k-1] > REAL_CONST(0.0))
            {
                deg[k] = REAL_CONST(1.0);

                if (rxx[k-2] < REAL_CONST(0.0))
                {
                    deg[k-1] = REAL_CONST(1.0) - MUL(rxx[k-1], rxx[k-1]);
                }
            } else if (rxx[k-2] < REAL_CONST(0.0)) {
                deg[k] = REAL_CONST(1.0) - MUL(rxx[k-1], rxx[k-1]);
            }
        }
    }
}
#endif

static real_t mapNewBw(uint8_t invf_mode, uint8_t invf_mode_prev)
{
    switch (invf_mode)
    {
    case 1: /* LOW */
        if (invf_mode_prev == 0) /* NONE */
            return COEF_CONST(0.6);
        else
            return COEF_CONST(0.75);

    case 2: /* MID */
        return COEF_CONST(0.9);

    case 3: /* HIGH */
        return COEF_CONST(0.98);

    default: /* NONE */
        if (invf_mode_prev == 1) /* LOW */
            return COEF_CONST(0.6);
        else
            return COEF_CONST(0.0);
    }
}

static void calc_chirp_factors(sbr_info *sbr, uint8_t ch)
{
    uint8_t i;

    for (i = 0; i < sbr->N_Q; i++)
    {
        sbr->bwArray[ch][i] = mapNewBw(sbr->bs_invf_mode[ch][i], sbr->bs_invf_mode_prev[ch][i]);

        if (sbr->bwArray[ch][i] < sbr->bwArray_prev[ch][i])
            sbr->bwArray[ch][i] = MUL_C_C(COEF_CONST(0.75), sbr->bwArray[ch][i]) + MUL_C_C(COEF_CONST(0.25), sbr->bwArray_prev[ch][i]);
        else
            sbr->bwArray[ch][i] = MUL_C_C(COEF_CONST(0.90625), sbr->bwArray[ch][i]) + MUL_C_C(COEF_CONST(0.09375), sbr->bwArray_prev[ch][i]);

        if (sbr->bwArray[ch][i] < COEF_CONST(0.015625))
            sbr->bwArray[ch][i] = COEF_CONST(0.0);

        if (sbr->bwArray[ch][i] >= COEF_CONST(0.99609375))
            sbr->bwArray[ch][i] = COEF_CONST(0.99609375);

        sbr->bwArray_prev[ch][i] = sbr->bwArray[ch][i];
        sbr->bs_invf_mode_prev[ch][i] = sbr->bs_invf_mode[ch][i];
    }
}

static void patch_construction(sbr_info *sbr)
{
    uint8_t i, k;
    uint8_t odd, sb;
    uint8_t msb = sbr->k0;
    uint8_t usb = sbr->kx;
    uint32_t goalSb = (uint32_t)(2.048e6/sbr->sample_rate + 0.5);

    sbr->noPatches = 0;

    if (goalSb < (sbr->kx + sbr->M))
    {
        for (i = 0, k = 0; sbr->f_master[i] < goalSb; i++)
            k = i+1;
    } else {
        k = sbr->N_master;
    }

    do
    {
        uint8_t j = k + 1;

        do
        {
            j--;

            sb = sbr->f_master[j];
            odd = (sb - 2 + sbr->k0) % 2;
        } while (sb > (sbr->k0 - 1 + msb - odd));

        sbr->patchNoSubbands[sbr->noPatches] = max(sb - usb, 0);
        sbr->patchStartSubband[sbr->noPatches] = sbr->k0 - odd -
            sbr->patchNoSubbands[sbr->noPatches];

        if (sbr->patchNoSubbands[sbr->noPatches] > 0)
        {
            usb = sb;
            msb = sb;
            sbr->noPatches++;
        } else {
            msb = sbr->kx;
        }

        if (sb == sbr->f_master[k])
            k = sbr->N_master;
    } while (sb != (sbr->kx + sbr->M));

    if ((sbr->patchNoSubbands[sbr->noPatches-1] < 3) &&
        (sbr->noPatches > 1))
    {
        sbr->noPatches--;
    }

    sbr->noPatches = min(sbr->noPatches, 5);
}

#endif
