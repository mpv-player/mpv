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
** $Id: sbr_e_nf.c,v 1.4 2003/09/09 18:37:32 menno Exp $
**/

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include <stdlib.h>

#include "sbr_syntax.h"
#include "sbr_e_nf.h"

void extract_envelope_data(sbr_info *sbr, uint8_t ch)
{
    uint8_t l, k;

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        if (sbr->bs_df_env[ch][l] == 0)
        {
            for (k = 1; k < sbr->n[sbr->f[ch][l]]; k++)
            {
                sbr->E[ch][k][l] = sbr->E[ch][k - 1][l] + sbr->E[ch][k][l];
            }

        } else { /* bs_df_env == 1 */

            uint8_t g = (l == 0) ? sbr->f_prev[ch] : sbr->f[ch][l-1];
            int16_t E_prev;

            if (sbr->f[ch][l] == g)
            {
                for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
                {
                    if (l == 0)
                        E_prev = sbr->E_prev[ch][k];
                    else
                        E_prev = sbr->E[ch][k][l - 1];

                    sbr->E[ch][k][l] = E_prev + sbr->E[ch][k][l];
                }

            } else if ((g == 1) && (sbr->f[ch][l] == 0)) {
                uint8_t i;

                for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
                {
                    for (i = 0; i < sbr->N_high; i++)
                    {
                        if (sbr->f_table_res[HI_RES][i] == sbr->f_table_res[LO_RES][k])
                        {
                            if (l == 0)
                                E_prev = sbr->E_prev[ch][i];
                            else
                                E_prev = sbr->E[ch][i][l - 1];

                            sbr->E[ch][k][l] = E_prev + sbr->E[ch][k][l];
                        }
                    }
                }

            } else if ((g == 0) && (sbr->f[ch][l] == 1)) {
                uint8_t i;

                for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
                {
                    for (i = 0; i < sbr->N_low; i++)
                    {
                        if ((sbr->f_table_res[LO_RES][i] <= sbr->f_table_res[HI_RES][k]) &&
                            (sbr->f_table_res[HI_RES][k] < sbr->f_table_res[LO_RES][i + 1]))
                        {
                            if (l == 0)
                                E_prev = sbr->E_prev[ch][i];
                            else
                                E_prev = sbr->E[ch][i][l - 1];

                            sbr->E[ch][k][l] = E_prev + sbr->E[ch][k][l];
                        }
                    }
                }
            }
        }
    }
}

void extract_noise_floor_data(sbr_info *sbr, uint8_t ch)
{
    uint8_t l, k;

    for (l = 0; l < sbr->L_Q[ch]; l++)
    {
        if (sbr->bs_df_noise[ch][l] == 0)
        {
            for (k = 1; k < sbr->N_Q; k++)
            {
                sbr->Q[ch][k][l] = sbr->Q[ch][k][l] + sbr->Q[ch][k-1][l];
            }
        } else {
            if (l == 0)
            {
                for (k = 0; k < sbr->N_Q; k++)
                {
                    sbr->Q[ch][k][l] = sbr->Q_prev[ch][k] + sbr->Q[ch][k][0];
                }
            } else {
                for (k = 0; k < sbr->N_Q; k++)
                {
                    sbr->Q[ch][k][l] = sbr->Q[ch][k][l - 1] + sbr->Q[ch][k][l];
                }
            }
        }
    }
}

/* FIXME: pow() not needed */
void envelope_noise_dequantisation(sbr_info *sbr, uint8_t ch)
{
    if (sbr->bs_coupling == 0)
    {
        uint8_t l, k;
        real_t amp = (sbr->amp_res[ch]) ? 1.0f : 0.5f;

        for (l = 0; l < sbr->L_E[ch]; l++)
        {
            for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
            {
                /* +6 for the *64 */
                sbr->E_orig[ch][k][l] = (real_t)pow(2, sbr->E[ch][k][l]*amp + 6);
            }
        }

        for (l = 0; l < sbr->L_Q[ch]; l++)
        {
            for (k = 0; k < sbr->N_Q; k++)
            {
                if (sbr->Q[ch][k][l] < 0 || sbr->Q[ch][k][l] > 30)
                    sbr->Q_orig[ch][k][l] = 0;
                else {
                    sbr->Q_orig[ch][k][l] = (real_t)pow(2, NOISE_FLOOR_OFFSET - sbr->Q[ch][k][l]);
                }
            }
        }
    }
}

void unmap_envelope_noise(sbr_info *sbr)
{
    uint8_t l, k;
    real_t amp0 = (sbr->amp_res[0]) ? (real_t)1.0 : (real_t)0.5;
    real_t amp1 = (sbr->amp_res[1]) ? (real_t)1.0 : (real_t)0.5;

    for (l = 0; l < sbr->L_E[0]; l++)
    {
        for (k = 0; k < sbr->n[sbr->f[0][l]]; k++)
        {
            real_t l_temp, r_temp;

            /* +6: * 64 ; +1: * 2 */
            l_temp = (real_t)pow(2, sbr->E[0][k][l]*amp0 + 7);
            /* UN_MAP removed: (x / 4096) same as (x >> 12) */
            r_temp = (real_t)pow(2, sbr->E[1][k][l]*amp1 - 12);

            sbr->E_orig[1][k][l] = l_temp / ((real_t)1.0 + r_temp);
            sbr->E_orig[0][k][l] = MUL(r_temp, sbr->E_orig[1][k][l]);
        }
    }
    for (l = 0; l < sbr->L_Q[0]; l++)
    {
        for (k = 0; k < sbr->N_Q; k++)
        {
            if ((sbr->Q[0][k][l] < 0 || sbr->Q[0][k][l] > 30) ||
                (sbr->Q[1][k][l] < 0 || sbr->Q[1][k][l] > 30))
            {
                sbr->Q_orig[0][k][l] = 0;
                sbr->Q_orig[1][k][l] = 0;
            } else {
                real_t l_temp, r_temp;

                l_temp = (real_t)pow(2.0, NOISE_FLOOR_OFFSET - sbr->Q[0][k][l] + 1);
                r_temp = (real_t)pow(2.0, sbr->Q[1][k][l] - 12);

                sbr->Q_orig[1][k][l] = l_temp / ((real_t)1.0 + r_temp);
                sbr->Q_orig[0][k][l] = MUL(r_temp, sbr->Q_orig[1][k][l]);
            }
        }
    }
}

#endif
