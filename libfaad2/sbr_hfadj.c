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
** $Id: sbr_hfadj.c,v 1.1 2003/07/29 08:20:13 menno Exp $
**/

/* High Frequency adjustment */

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include "sbr_syntax.h"
#include "sbr_hfadj.h"

#include "sbr_noise.h"

void hf_adjustment(sbr_info *sbr, qmf_t *Xsbr
#ifdef SBR_LOW_POWER
                   ,real_t *deg /* aliasing degree */
#endif
                   ,uint8_t ch)
{
    sbr_hfadj_info adj;

    memset(&adj, 0, sizeof(sbr_hfadj_info));

    map_noise_data(sbr, &adj, ch);
    map_sinusoids(sbr, &adj, ch);

    estimate_current_envelope(sbr, &adj, Xsbr, ch);

    calculate_gain(sbr, &adj, ch);

#if 1

#ifdef SBR_LOW_POWER
    calc_gain_groups(sbr, &adj, deg, ch);
    aliasing_reduction(sbr, &adj, deg, ch);
#endif

    hf_assembly(sbr, &adj, Xsbr, ch);

#endif
}

static void map_noise_data(sbr_info *sbr, sbr_hfadj_info *adj, uint8_t ch)
{
    uint8_t l, i;
    uint32_t m;

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        for (i = 0; i < sbr->N_Q; i++)
        {
            for (m = sbr->f_table_noise[i]; m < sbr->f_table_noise[i+1]; m++)
            {
                uint8_t k;

                adj->Q_mapped[m - sbr->kx][l] = 0;

                for (k = 0; k < 2; k++)
                {
                    if ((sbr->t_E[ch][l] >= sbr->t_Q[ch][k]) &&
                        (sbr->t_E[ch][l+1] <= sbr->t_Q[ch][k+1]))
                    {
                        adj->Q_mapped[m - sbr->kx][l] =
                            sbr->Q_orig[ch][i][k];
                    }
                }
            }
        }
    }
}

static void map_sinusoids(sbr_info *sbr, sbr_hfadj_info *adj, uint8_t ch)
{
    uint8_t l, i, m, k, k1, k2, delta_S, l_i, u_i;

    if (sbr->bs_frame_class[ch] == FIXFIX)
    {
        sbr->l_A[ch] = -1;
    } else if (sbr->bs_frame_class[ch] == VARFIX) {
        if (sbr->bs_pointer[ch] > 1)
            sbr->l_A[ch] = -1;
        else
            sbr->l_A[ch] = sbr->bs_pointer[ch] - 1;
    } else {
        if (sbr->bs_pointer[ch] == 0)
            sbr->l_A[ch] = -1;
        else
            sbr->l_A[ch] = sbr->L_E[ch] + 1 - sbr->bs_pointer[ch];
    }

    for (l = 0; l < 5; l++)
    {
        for (i = 0; i < 64; i++)
        {
            adj->S_index_mapped[i][l] = 0;
            adj->S_mapped[i][l] = 0;
        }
    }

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        for (i = 0; i < sbr->N_high; i++)
        {
            for (m = sbr->f_table_res[HI_RES][i]; m < sbr->f_table_res[HI_RES][i+1]; m++)
            {
                uint8_t delta_step = 0;
                if ((l >= sbr->l_A[ch]) || ((sbr->bs_add_harmonic_prev[ch][i]) &&
                    (sbr->bs_add_harmonic_flag_prev[ch])))
                {
                    delta_step = 1;
                }

                if (m == (int32_t)((real_t)(sbr->f_table_res[HI_RES][i+1]+sbr->f_table_res[HI_RES][i])/2.))
                {
                    adj->S_index_mapped[m - sbr->kx][l] =
                        delta_step * sbr->bs_add_harmonic[ch][i];
                } else {
                    adj->S_index_mapped[m - sbr->kx][l] = 0;
                }

#if 0
                if (sbr->frame == 95)
                {
                    printf("%d %d %d %d %d\n", adj->S_index_mapped[m - sbr->kx][l],
                        sbr->bs_add_harmonic[ch][i], sbr->bs_add_harmonic_prev[ch][i],
                        l, sbr->l_A[ch]);
                }
#endif
            }
        }
    }

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        for (i = 0; i < sbr->N_high; i++)
        {
            if (sbr->f[ch][l] == 1)
            {
                k1 = i;
                k2 = i + 1;
            } else {
                for (k1 = 0; k1 < sbr->N_low; k1++)
                {
                    if ((sbr->f_table_res[HI_RES][i] >= sbr->f_table_res[LO_RES][k1]) &&
                        (sbr->f_table_res[HI_RES][i+1] <= sbr->f_table_res[LO_RES][k1+1]))
                    {
                        break;
                    }
                }
                for (k2 = 0; k2 < sbr->N_low; k2++)
                {
                    if ((sbr->f_table_res[HI_RES][i+1] >= sbr->f_table_res[LO_RES][k2]) &&
                        (sbr->f_table_res[HI_RES][i+2] <= sbr->f_table_res[LO_RES][k2+1]))
                    {
                        break;
                    }
                }
            }

            l_i = sbr->f_table_res[sbr->f[ch][l]][k1];
            u_i = sbr->f_table_res[sbr->f[ch][l]][k2];

            delta_S = 0;
            for (k = l_i; k < u_i; k++)
            {
                if (adj->S_index_mapped[k - sbr->kx][l] == 1)
                    delta_S = 1;
            }

            for (m = l_i; m < u_i; m++)
            {
                adj->S_mapped[m - sbr->kx][l] = delta_S;
            }
        }
    }
}

static void estimate_current_envelope(sbr_info *sbr, sbr_hfadj_info *adj, qmf_t *Xsbr,
                                      uint8_t ch)
{
    uint8_t m, l, j, k, k_l, k_h, p;
    real_t nrg, div;

    if (sbr->bs_interpol_freq == 1)
    {
        for (l = 0; l < sbr->L_E[ch]; l++)
        {
            uint8_t i, l_i, u_i;

            l_i = sbr->t_E[ch][l];
            u_i = sbr->t_E[ch][l+1];

            div = (real_t)(u_i - l_i);

            for (m = 0; m < sbr->M; m++)
            {
                nrg = 0;

                for (i = l_i + tHFAdj; i < u_i + tHFAdj; i++)
                {
#ifdef FIXED_POINT
                    nrg += ((QMF_RE(Xsbr[(i<<6) + m + sbr->kx])+(1<<(REAL_BITS-1)))>>REAL_BITS)*((QMF_RE(Xsbr[(i<<6) + m + sbr->kx])+(1<<(REAL_BITS-1)))>>REAL_BITS);
#else
                    nrg += MUL(QMF_RE(Xsbr[(i<<6) + m + sbr->kx]), QMF_RE(Xsbr[(i<<6) + m + sbr->kx]))
#ifndef SBR_LOW_POWER
                        + MUL(QMF_IM(Xsbr[(i<<6) + m + sbr->kx]), QMF_IM(Xsbr[(i<<6) + m + sbr->kx]))
#endif
                        ;
#endif
                }

                sbr->E_curr[ch][m][l] = nrg / div;
#ifdef SBR_LOW_POWER
#ifdef FIXED_POINT
                sbr->E_curr[ch][m][l] <<= 1;
#else
                sbr->E_curr[ch][m][l] *= 2;
#endif
#endif
            }
        }
    } else {
        for (l = 0; l < sbr->L_E[ch]; l++)
        {
            for (p = 0; p < sbr->n[sbr->f[ch][l]]; p++)
            {
                k_l = sbr->f_table_res[sbr->f[ch][l]][p];
                k_h = sbr->f_table_res[sbr->f[ch][l]][p+1];

                for (k = k_l; k < k_h; k++)
                {
                    uint8_t i, l_i, u_i;
                    nrg = 0.0;

                    l_i = sbr->t_E[ch][l];
                    u_i = sbr->t_E[ch][l+1];

                    div = (real_t)((u_i - l_i)*(k_h - k_l + 1));

                    for (i = l_i + tHFAdj; i < u_i + tHFAdj; i++)
                    {
                        for (j = k_l; j < k_h; j++)
                        {
#ifdef FIXED_POINT
                            nrg += ((QMF_RE(Xsbr[(i<<6) + j])+(1<<(REAL_BITS-1)))>>REAL_BITS)*((QMF_RE(Xsbr[(i<<6) + j])+(1<<(REAL_BITS-1)))>>REAL_BITS);
#else
                            nrg += MUL(QMF_RE(Xsbr[(i<<6) + j]), QMF_RE(Xsbr[(i<<6) + j]))
#ifndef SBR_LOW_POWER
                                + MUL(QMF_IM(Xsbr[(i<<6) + j]), QMF_IM(Xsbr[(i<<6) + j]))
#endif
                                ;
#endif
                        }
                    }

                    sbr->E_curr[ch][k - sbr->kx][l] = nrg / div;
#ifdef SBR_LOW_POWER
#ifdef FIXED_POINT
                    sbr->E_curr[ch][k - sbr->kx][l] <<= 1;
#else
                    sbr->E_curr[ch][k - sbr->kx][l] *= 2;
#endif
#endif
                }
            }
        }
    }
}

#ifdef FIXED_POINT
#define step(shift) \
    if ((0x40000000l >> shift) + root <= value)       \
    {                                                 \
        value -= (0x40000000l >> shift) + root;       \
        root = (root >> 1) | (0x40000000l >> shift);  \
    } else {                                          \
        root = root >> 1;                             \
    }

/* fixed point square root approximation */
real_t sbr_sqrt(real_t value)
{
    real_t root = 0;

    step( 0); step( 2); step( 4); step( 6);
    step( 8); step(10); step(12); step(14);
    step(16); step(18); step(20); step(22);
    step(24); step(26); step(28); step(30);

    if (root < value)
        ++root;

    root <<= (REAL_BITS/2);

    return root;
}
real_t sbr_sqrt_int(real_t value)
{
    real_t root = 0;

    step( 0); step( 2); step( 4); step( 6);
    step( 8); step(10); step(12); step(14);
    step(16); step(18); step(20); step(22);
    step(24); step(26); step(28); step(30);

    if (root < value)
        ++root;

    return root;
}
#define SBR_SQRT_FIX(A) sbr_sqrt(A)
#define SBR_SQRT_INT(A) sbr_sqrt_int(A)
#endif

#ifdef FIXED_POINT
#define EPS (1) /* smallest number available in fixed point */
#else
#define EPS (1e-12)
#endif

#ifdef FIXED_POINT
#define ONE (REAL_CONST(1)>>10)
#else
#define ONE (1)
#endif


#ifdef FIXED_POINT
static void calculate_gain(sbr_info *sbr, sbr_hfadj_info *adj, uint8_t ch)
{
    uint8_t m, l, k, i;

    real_t Q_M_lim[64];
    real_t G_lim[64];
    real_t G_boost;
    real_t S_M[64];
    uint8_t table_map_res_to_m[64];


    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        real_t delta = (l == sbr->l_A[ch] || l == sbr->prevEnvIsShort[ch]) ? 0 : 1;

        for (i = 0; i < sbr->n[sbr->f[ch][l]]; i++)
        {
            for (m = sbr->f_table_res[sbr->f[ch][l]][i]; m < sbr->f_table_res[sbr->f[ch][l]][i+1]; m++)
            {
                table_map_res_to_m[m - sbr->kx] = i;
            }
        }

        for (k = 0; k < sbr->N_L[sbr->bs_limiter_bands]; k++)
        {
            real_t G_max;
            real_t den = 0;
            real_t acc1 = 0;
            real_t acc2 = 0;

            for (m = sbr->f_table_lim[sbr->bs_limiter_bands][k];
                 m < sbr->f_table_lim[sbr->bs_limiter_bands][k+1]; m++)
            {
                /* E_orig: integer */
                acc1 += sbr->E_orig[ch][table_map_res_to_m[m]][l];
                /* E_curr: integer */
                acc2 += sbr->E_curr[ch][m][l];
            }

            /* G_max: fixed point */
            if (acc2 == 0)
            {
                G_max = 0xFFF;
            } else {
                G_max = (((int64_t)acc1)<<REAL_BITS) / acc2;
                switch (sbr->bs_limiter_gains)
                {
                case 0: G_max >>= 1; break;
                case 2: G_max <<= 1; break;
                default: break;
                }
            }

            //printf("%f %d %d\n", G_max /(float)(1<<REAL_BITS), acc1, acc2);

            for (m = sbr->f_table_lim[sbr->bs_limiter_bands][k];
                 m < sbr->f_table_lim[sbr->bs_limiter_bands][k+1]; m++)
            {
                real_t d, Q_M, G;
                real_t div2;

                /* Q_mapped: fixed point */
                /* div2: fixed point COEF */
                real_t tmp2 = adj->Q_mapped[m][l] << (COEF_BITS-REAL_BITS);
                real_t tmp = COEF_CONST(1) + tmp2;
                if (tmp == 0)
                    div2 = COEF_CONST(1);
                else
                    div2 = (((int64_t)tmp2 << COEF_BITS)/tmp);

                //printf("%f\n", div2 / (float)(1<<COEF_BITS));

                /* Q_M: integer */
                Q_M = MUL_R_C(sbr->E_orig[ch][table_map_res_to_m[m]][l], div2);

                //printf("%d\n", Q_M /* / (float)(1<<REAL_BITS)*/);

                if (adj->S_mapped[m][l] == 0)
                {
                    real_t tmp, tmp2;

                    S_M[m] = 0;

                    /* d: fixed point */
                    tmp2 = adj->Q_mapped[m][l] /* << (COEF_BITS-REAL_BITS)*/;
                    tmp = REAL_CONST(1) + delta*tmp2;
                    d = (((int64_t)REAL_CONST(1))<<REAL_BITS) / (tmp);

                    /* G: fixed point */
                    G = (((int64_t)sbr->E_orig[ch][table_map_res_to_m[m]][l])<<REAL_BITS) / (1 + sbr->E_curr[ch][m][l]);
                    G = MUL(G, d);

                    //printf("%f\n", G/(float)(1<<REAL_BITS));

                } else {

                    real_t div;

                    /* div: fixed point COEF */
                    real_t tmp = COEF_CONST(1.0) + (adj->Q_mapped[m][l] << (COEF_BITS-REAL_BITS));
                    real_t tmp2 = COEF_CONST(adj->S_mapped[m][l]);
                    if (tmp == 0)
                        div = COEF_CONST(1);
                    else
                        div = (((int64_t)tmp2 << COEF_BITS)/tmp);

                    //printf("%f\n", div/(float)(1<<COEF_BITS));

                    /* S_M: integer */
                    S_M[m] = MUL_R_C(sbr->E_orig[ch][table_map_res_to_m[m]][l], div);

                    //printf("%d\n", S_M[m]);

                    /* G: fixed_point */
                    if ((ONE + sbr->E_curr[ch][m][l]) == 0)
                        G = 0xFFF; // uhm???
                    else {
                        real_t tmp = ONE + sbr->E_curr[ch][m][l];
                        /* tmp2: fixed point */
                        real_t tmp2 = (((int64_t)(sbr->E_orig[ch][table_map_res_to_m[m]][l]))<<REAL_BITS)/(tmp);
                        G = MUL_R_C(tmp2, div2);
                    }

                    //printf("%f\n", G/(float)(1<<REAL_BITS));
                }

                /* limit the additional noise energy level */
                /* and apply the limiter */

                /* G_lim: fixed point */
                /* Q_M_lim: integer */
                if (G_max > G)
                {
                    Q_M_lim[m] = Q_M;
                    G_lim[m] = G;
                } else {
                    real_t tmp;
                    if (G == 0)
                        tmp = 0xFFF;
                    else
                        tmp = SBR_DIV(G_max, G);
                    Q_M_lim[m] = MUL(Q_M, tmp);
                    G_lim[m] = G_max;
                }

                /* E_curr: integer, using MUL() is NOT OK */
                den += MUL(sbr->E_curr[ch][m][l], G_lim[m]);
                if (adj->S_index_mapped[m][l])
                    den += S_M[m];
                else if (l != sbr->l_A[ch])
                    den += Q_M_lim[m];
            }

            //printf("%d\n", den);

            /* G_boost: fixed point */
            if ((den + EPS) == 0)
                G_boost = REAL_CONST(2.51188643);
            else
                G_boost = (((int64_t)(acc1 + EPS))<<REAL_BITS)/(den + EPS);
            G_boost = min(G_boost, REAL_CONST(2.51188643) /* 1.584893192 ^ 2 */);

            for (m = sbr->f_table_lim[sbr->bs_limiter_bands][k];
                 m < sbr->f_table_lim[sbr->bs_limiter_bands][k+1]; m++)
            {
                /* apply compensation to gain, noise floor sf's and sinusoid levels */
#ifndef SBR_LOW_POWER
                /* G_lim_boost: fixed point */
                adj->G_lim_boost[l][m] = SBR_SQRT_FIX(MUL(G_lim[m], G_boost));
#else
                /* sqrt() will be done after the aliasing reduction to save a
                 * few multiplies
                 */
                /* G_lim_boost: fixed point */
                adj->G_lim_boost[l][m] = MUL(G_lim[m], G_boost);
#endif
                /* Q_M_lim_boost: integer */
                adj->Q_M_lim_boost[l][m] = SBR_SQRT_INT(MUL(Q_M_lim[m], G_boost));

                /* S_M_boost: integer */
                if (adj->S_index_mapped[m][l])
                    adj->S_M_boost[l][m] = SBR_SQRT_INT(MUL(S_M[m], G_boost));
                else
                    adj->S_M_boost[l][m] = 0;
            }
        }
    }
}
#else
static void calculate_gain(sbr_info *sbr, sbr_hfadj_info *adj, uint8_t ch)
{
    static real_t limGain[] = { 0.5, 1.0, 2.0, 1e10 };
    uint8_t m, l, k, i;

    real_t Q_M_lim[64];
    real_t G_lim[64];
    real_t G_boost;
    real_t S_M[64];
    uint8_t table_map_res_to_m[64];


    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        real_t delta = (l == sbr->l_A[ch] || l == sbr->prevEnvIsShort[ch]) ? 0 : 1;

        for (i = 0; i < sbr->n[sbr->f[ch][l]]; i++)
        {
            for (m = sbr->f_table_res[sbr->f[ch][l]][i]; m < sbr->f_table_res[sbr->f[ch][l]][i+1]; m++)
            {
                table_map_res_to_m[m - sbr->kx] = i;
            }
        }

        for (k = 0; k < sbr->N_L[sbr->bs_limiter_bands]; k++)
        {
            real_t G_max;
            real_t den = 0;
            real_t acc1 = 0;
            real_t acc2 = 0;

            for (m = sbr->f_table_lim[sbr->bs_limiter_bands][k];
                 m < sbr->f_table_lim[sbr->bs_limiter_bands][k+1]; m++)
            {
                acc1 += sbr->E_orig[ch][table_map_res_to_m[m]][l];
                acc2 += sbr->E_curr[ch][m][l];
            }

            G_max = ((EPS + acc1)/(EPS + acc2)) * limGain[sbr->bs_limiter_gains];
            G_max = min(G_max, 1e10);

            //printf("%f %d %d\n", G_max, (int)floor((acc1+EPS)/1024.), (int)floor((acc2+EPS)/1024.));

            for (m = sbr->f_table_lim[sbr->bs_limiter_bands][k];
                 m < sbr->f_table_lim[sbr->bs_limiter_bands][k+1]; m++)
            {
                real_t d, Q_M, G;
                real_t div2;

                div2 = adj->Q_mapped[m][l] / (1 + adj->Q_mapped[m][l]);

                //printf("%f\n", div2);

                Q_M = sbr->E_orig[ch][table_map_res_to_m[m]][l] * div2;

                //printf("%f\n", Q_M/1024.);

                if (adj->S_mapped[m][l] == 0)
                {
                    S_M[m] = 0;

                    /* fixed point: delta* can stay since it's either 1 or 0 */
                    d = (1 + sbr->E_curr[ch][m][l]) * (1 + delta*adj->Q_mapped[m][l]);

                    //printf("%f\n", d/1024.);

                    G = sbr->E_orig[ch][table_map_res_to_m[m]][l] / d;

                    //printf("%f\n", G);

                } else {
                    real_t div;

                    div = adj->S_mapped[m][l] / (1. + adj->Q_mapped[m][l]);

                    //printf("%f\n", div);

                    S_M[m] = sbr->E_orig[ch][table_map_res_to_m[m]][l] * div;

                    //printf("%f\n", S_M[m]/1024.);

                    G = (sbr->E_orig[ch][table_map_res_to_m[m]][l] / (1. + sbr->E_curr[ch][m][l])) * div2;

                    //printf("%f\n", G);
                }

                /* limit the additional noise energy level */
                /* and apply the limiter */
                if (G_max > G)
                {
                    Q_M_lim[m] = Q_M;
                    G_lim[m] = G;
                } else {
                    Q_M_lim[m] = Q_M * G_max / G;
                    G_lim[m] = G_max;

                    //printf("%f\n", Q_M_lim[m] / 1024.);
                }

                den += sbr->E_curr[ch][m][l] * G_lim[m];
                if (adj->S_index_mapped[m][l])
                    den += S_M[m];
                else if (l != sbr->l_A[ch])
                    den += Q_M_lim[m];
            }

            //printf("%f\n", den/1024.);

            G_boost = (acc1 + EPS) / (den + EPS);
            G_boost = min(G_boost, 2.51188643 /* 1.584893192 ^ 2 */);

            for (m = sbr->f_table_lim[sbr->bs_limiter_bands][k];
                 m < sbr->f_table_lim[sbr->bs_limiter_bands][k+1]; m++)
            {
                /* apply compensation to gain, noise floor sf's and sinusoid levels */
#ifndef SBR_LOW_POWER
                adj->G_lim_boost[l][m] = sqrt(G_lim[m] * G_boost);
#else
                /* sqrt() will be done after the aliasing reduction to save a
                 * few multiplies
                 */
                adj->G_lim_boost[l][m] = G_lim[m] * G_boost;
#endif
                adj->Q_M_lim_boost[l][m] = sqrt(Q_M_lim[m] * G_boost);

                if (adj->S_index_mapped[m][l])
                    adj->S_M_boost[l][m] = sqrt(S_M[m] * G_boost);
                else
                    adj->S_M_boost[l][m] = 0;
            }
        }
    }
}
#endif

#ifdef SBR_LOW_POWER
static void calc_gain_groups(sbr_info *sbr, sbr_hfadj_info *adj, real_t *deg, uint8_t ch)
{
    uint8_t l, k, i;
    uint8_t grouping;

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        i = 0;
        grouping = 0;

        for (k = sbr->kx; k < sbr->kx + sbr->M - 1; k++)
        {
            if (deg[k + 1] && adj->S_mapped[k-sbr->kx][l] == 0)
            {
                if (grouping == 0)
                {
                    sbr->f_group[l][i] = k;
                    grouping = 1;
                    i++;
                }
            } else {
                if (grouping)
                {
                    if (adj->S_mapped[k-sbr->kx][l])
                        sbr->f_group[l][i] = k;
                    else
                        sbr->f_group[l][i] = k + 1;
                    grouping = 0;
                    i++;
                }
            }
        }        

        if (grouping)
        {
            sbr->f_group[l][i] = sbr->kx + sbr->M;
            i++;
        }

        sbr->N_G[l] = (uint8_t)(i >> 1);
    }
}

static void aliasing_reduction(sbr_info *sbr, sbr_hfadj_info *adj, real_t *deg, uint8_t ch)
{
    uint8_t l, k, m;
    real_t E_total, E_total_est, G_target, acc;

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        for (k = 0; k < sbr->N_G[l]; k++)
        {
            E_total_est = E_total = 0;
            
            for (m = sbr->f_group[l][k<<1]; m < sbr->f_group[l][(k<<1) + 1]; m++)
            {
                /* E_curr: integer */
                /* G_lim_boost: fixed point */
                /* E_total_est: integer */
                /* E_total: integer */
                E_total_est += sbr->E_curr[ch][m-sbr->kx][l];
                E_total += MUL(sbr->E_curr[ch][m-sbr->kx][l], adj->G_lim_boost[l][m-sbr->kx]);
            }

            /* G_target: fixed point */
            if ((E_total_est + EPS) == 0)
                G_target = 0;
            else
#ifdef FIXED_POINT
                G_target = (((int64_t)(E_total))<<REAL_BITS)/(E_total_est + EPS);
#else
                G_target = E_total / (E_total_est + EPS);
#endif
            acc = 0;

            for (m = sbr->f_group[l][(k<<1)]; m < sbr->f_group[l][(k<<1) + 1]; m++)
            {
                real_t alpha;

                /* alpha: fixed point */
                if (m < sbr->kx + sbr->M - 1)
                {
                    alpha = max(deg[m], deg[m + 1]);
                } else {
                    alpha = deg[m];
                }

                adj->G_lim_boost[l][m-sbr->kx] = MUL(alpha, G_target) +
                    MUL((REAL_CONST(1)-alpha), adj->G_lim_boost[l][m-sbr->kx]);

                /* acc: integer */
                acc += MUL(adj->G_lim_boost[l][m-sbr->kx], sbr->E_curr[ch][m-sbr->kx][l]);
            }

            /* acc: fixed point */
            if (acc + EPS == 0)
                acc = 0;
            else
#ifdef FIXED_POINT
                acc = (((int64_t)(E_total))<<REAL_BITS)/(acc + EPS);
#else
                acc = E_total / (acc + EPS);
#endif
            for(m = sbr->f_group[l][(k<<1)]; m < sbr->f_group[l][(k<<1) + 1]; m++)
            {
                adj->G_lim_boost[l][m-sbr->kx] = MUL(acc, adj->G_lim_boost[l][m-sbr->kx]);
            }
        }
    }

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        for (k = 0; k < sbr->N_L[sbr->bs_limiter_bands]; k++)
        {
            for (m = sbr->f_table_lim[sbr->bs_limiter_bands][k];
                 m < sbr->f_table_lim[sbr->bs_limiter_bands][k+1]; m++)
            {
#ifdef FIXED_POINT
                 adj->G_lim_boost[l][m] = SBR_SQRT_FIX(adj->G_lim_boost[l][m]);
#else
                 adj->G_lim_boost[l][m] = sqrt(adj->G_lim_boost[l][m]);
#endif
            }
        }
    }
}
#endif

static void hf_assembly(sbr_info *sbr, sbr_hfadj_info *adj,
                        qmf_t *Xsbr, uint8_t ch)
{
    static real_t h_smooth[] = {
        COEF_CONST(0.03183050093751), COEF_CONST(0.11516383427084),
        COEF_CONST(0.21816949906249), COEF_CONST(0.30150283239582),
        COEF_CONST(0.33333333333333)
    };
    static int8_t phi_re[] = { 1, 0, -1, 0 };
    static int8_t phi_im[] = { 0, 1, 0, -1 };

    uint8_t m, l, i, n;
    uint16_t fIndexNoise = 0;
    uint8_t fIndexSine = 0;
    uint8_t assembly_reset = 0;
    real_t *temp;

    real_t G_filt, Q_filt;

    uint8_t h_SL;


    if (sbr->Reset == 1)
    {
        assembly_reset = 1;
        fIndexNoise = 0;
    } else {
        fIndexNoise = sbr->index_noise_prev[ch];
    }
    fIndexSine = sbr->psi_is_prev[ch];


    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        uint8_t no_noise = (l == sbr->l_A[ch] || l == sbr->prevEnvIsShort[ch]) ? 1 : 0;

#ifdef SBR_LOW_POWER
        h_SL = 0;
#else
        h_SL = (sbr->bs_smoothing_mode == 1) ? 0 : 4;
        h_SL = (no_noise ? 0 : h_SL);
#endif

        if (assembly_reset)
        {
            for (n = 0; n < 4; n++)
            {
                memcpy(sbr->G_temp_prev[ch][n], adj->G_lim_boost[l], sbr->M*sizeof(real_t));
                memcpy(sbr->Q_temp_prev[ch][n], adj->Q_M_lim_boost[l], sbr->M*sizeof(real_t));
            }
            assembly_reset = 0;
        }


        for (i = sbr->t_E[ch][l]; i < sbr->t_E[ch][l+1]; i++)
        {
#ifdef SBR_LOW_POWER
            uint8_t i_min1, i_plus1;
            uint8_t sinusoids = 0;
#endif

            memcpy(sbr->G_temp_prev[ch][4], adj->G_lim_boost[l], sbr->M*sizeof(real_t));
            memcpy(sbr->Q_temp_prev[ch][4], adj->Q_M_lim_boost[l], sbr->M*sizeof(real_t));

            for (m = 0; m < sbr->M; m++)
            {
                uint8_t j;
                qmf_t psi;


                G_filt = 0;
                Q_filt = 0;
                j = 0;

                if (h_SL != 0)
                {
                    for (n = 0; n <= 4; n++)
                    {
                        G_filt += MUL_R_C(sbr->G_temp_prev[ch][n][m], h_smooth[j]);
                        Q_filt += MUL_R_C(sbr->Q_temp_prev[ch][n][m], h_smooth[j]);
                        j++;
                    }
                } else {
                    G_filt = sbr->G_temp_prev[ch][4][m];
                    Q_filt = sbr->Q_temp_prev[ch][4][m];
                }

                Q_filt = (adj->S_M_boost[l][m] != 0 || no_noise) ? 0 : Q_filt;

#if 0
                if (sbr->frame == 155)
                {
                    printf("%f\n", G_filt);
                }
#endif

                /* add noise to the output */
                fIndexNoise = (fIndexNoise + 1) & 511;

#if 0
                printf("%d %f\n", Q_filt, RE(V[fIndexNoise])/(float)(1<<COEF_BITS));
#endif

                /* the smoothed gain values are applied to Xsbr */
                /* V is defined, not calculated */
#ifdef FIXED_POINT
                QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) = MUL(G_filt, QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]))
                    + MUL_R_C((Q_filt<<REAL_BITS), RE(V[fIndexNoise]));
#else
                QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) = MUL(G_filt, QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]))
                    + MUL_R_C(Q_filt, RE(V[fIndexNoise]));
#endif
                if (sbr->bs_extension_id == 3 && sbr->bs_extension_data == 42)
                    QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) = 16428320;
#ifndef SBR_LOW_POWER
                QMF_IM(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) = MUL(G_filt, QMF_IM(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]))
                    + MUL_R_C(Q_filt, IM(V[fIndexNoise]));
#endif


                if (adj->S_index_mapped[m][l])
                {
                    int8_t rev = ((m + sbr->kx) & 1) ? -1 : 1;
                    QMF_RE(psi) = MUL(adj->S_M_boost[l][m], phi_re[fIndexSine]);
                    QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) += QMF_RE(psi);

#ifndef SBR_LOW_POWER
                    QMF_IM(psi) = rev * MUL(adj->S_M_boost[l][m], phi_im[fIndexSine]);
                    QMF_IM(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) += QMF_IM(psi);
#else
                    i_min1 = (fIndexSine - 1) & 3;
                    i_plus1 = (fIndexSine + 1) & 3;

                    if (m == 0)
                    {
                        QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx - 1]) -=
                            (rev * MUL_R_C(MUL(adj->S_M_boost[l][0], phi_re[i_plus1]), COEF_CONST(0.00815)));
                        QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) -=
                            (rev * MUL_R_C(MUL(adj->S_M_boost[l][1], phi_re[i_plus1]), COEF_CONST(0.00815)));
                    }
                    if ((m > 0) && (m < sbr->M - 1) && (sinusoids < 16))
                    {
                        QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) -=
                            (rev * MUL_R_C(MUL(adj->S_M_boost[l][m - 1], phi_re[i_min1]), COEF_CONST(0.00815)));
                        QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) -=
                            (rev * MUL_R_C(MUL(adj->S_M_boost[l][m + 1], phi_re[i_plus1]), COEF_CONST(0.00815)));
                    }
                    if ((m == sbr->M - 1) && (sinusoids < 16) && (m + sbr->kx + 1 < 63))
                    {
                        QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx]) -=
                            (rev * MUL_R_C(MUL(adj->S_M_boost[l][m - 1], phi_re[i_min1]), COEF_CONST(0.00815)));
                        QMF_RE(Xsbr[((i + tHFAdj)<<6) + m+sbr->kx + 1]) -=
                            (rev * MUL_R_C(MUL(adj->S_M_boost[l][m + 1], phi_re[i_min1]), COEF_CONST(0.00815)));
                    }

                    sinusoids++;
#endif
                }
            }

            fIndexSine = (fIndexSine + 1) & 3;


            temp = sbr->G_temp_prev[ch][0];
            for (n = 0; n < 4; n++)
                sbr->G_temp_prev[ch][n] = sbr->G_temp_prev[ch][n+1];
            sbr->G_temp_prev[ch][4] = temp;

            temp = sbr->Q_temp_prev[ch][0];
            for (n = 0; n < 4; n++)
                sbr->Q_temp_prev[ch][n] = sbr->Q_temp_prev[ch][n+1];
            sbr->Q_temp_prev[ch][4] = temp;
        }
    }

    sbr->index_noise_prev[ch] = fIndexNoise;
    sbr->psi_is_prev[ch] = fIndexSine;
}

#endif
