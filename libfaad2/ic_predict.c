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
** $Id: ic_predict.c,v 1.12 2003/07/29 08:20:12 menno Exp $
**/

#include "common.h"
#include "structs.h"

#ifdef MAIN_DEC

#include "syntax.h"
#include "ic_predict.h"
#include "pns.h"

static void flt_round(real_t *pf)
{
    /* more stable version for clever compilers like gcc 3.x */
    int32_t flg;
    uint32_t tmp, tmp1, tmp2;

    tmp = *(uint32_t*)pf;
    flg = tmp & (uint32_t)0x00008000;
    tmp &= (uint32_t)0xffff0000;
    tmp1 = tmp;

    /* round 1/2 lsb toward infinity */
    if (flg)
    {
        tmp &= (uint32_t)0xff800000;       /* extract exponent and sign */
        tmp |= (uint32_t)0x00010000;       /* insert 1 lsb */
        tmp2 = tmp;                             /* add 1 lsb and elided one */
        tmp &= (uint32_t)0xff800000;       /* extract exponent and sign */
        
        *pf = *(real_t*)&tmp1+*(real_t*)&tmp2-*(real_t*)&tmp;/* subtract elided one */
    } else {
        *pf = *(real_t*)&tmp;
    }
}

static void ic_predict(pred_state *state, real_t input, real_t *output, uint8_t pred)
{
    real_t dr1, predictedvalue;
    real_t e0, e1;
    real_t k1, k2;

    real_t *r;
    real_t *KOR;
    real_t *VAR;

    r   = state->r;   /* delay elements */
    KOR = state->KOR; /* correlations */
    VAR = state->VAR; /* variances */

    if (VAR[0] <= 1)
        k1 = 0;
    else
        k1 = KOR[0]/VAR[0]*B;

    if (pred)
    {
        /* only needed for the actual predicted value, k1 is always needed */
        if (VAR[1] <= 1)
            k2 = 0;
        else
            k2 = KOR[1]/VAR[1]*B;

        predictedvalue = MUL(k1, r[0]) + MUL(k2, r[1]);
        flt_round(&predictedvalue);

        *output = input + predictedvalue;
    } else {
        *output = input;
    }

    /* calculate new state data */
    e0 = *output;
    e1 = e0 - MUL(k1, r[0]);

    dr1 = MUL(k1, e0);

    VAR[0] = MUL(ALPHA, VAR[0]) + MUL(REAL_CONST(0.5), (MUL(r[0], r[0]) + MUL(e0, e0)));
    KOR[0] = MUL(ALPHA, KOR[0]) + MUL(r[0], e0);
    VAR[1] = MUL(ALPHA, VAR[1]) + MUL(REAL_CONST(0.5), (MUL(r[1], r[1]) + MUL(e1, e1)));
    KOR[1] = MUL(ALPHA, KOR[1]) + MUL(r[1], e1);

    r[1] = MUL(A, (r[0]-dr1));
    r[0] = MUL(A, e0);
}

static void reset_pred_state(pred_state *state)
{
    state->r[0]   = 0;
    state->r[1]   = 0;
    state->KOR[0] = 0;
    state->KOR[1] = 0;
    state->VAR[0] = REAL_CONST(1.0);
    state->VAR[1] = REAL_CONST(1.0);
}

void pns_reset_pred_state(ic_stream *ics, pred_state *state)
{
    uint8_t sfb, g, b;
    uint16_t i, offs, offs2;

    /* prediction only for long blocks */
    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
        return;

    for (g = 0; g < ics->num_window_groups; g++)
    {
        for (b = 0; b < ics->window_group_length[g]; b++)
        {
            for (sfb = 0; sfb < ics->max_sfb; sfb++)
            {
                if (is_noise(ics, g, sfb))
                {
                    offs = ics->swb_offset[sfb];
                    offs2 = ics->swb_offset[sfb+1];

                    for (i = offs; i < offs2; i++)
                        reset_pred_state(&state[i]);
                }
            }
        }
    }
}

void reset_all_predictors(pred_state *state, uint16_t frame_len)
{
    uint16_t i;

    for (i = 0; i < frame_len; i++)
        reset_pred_state(&state[i]);
}

/* intra channel prediction */
void ic_prediction(ic_stream *ics, real_t *spec, pred_state *state,
                   uint16_t frame_len)
{
    uint8_t sfb;
    uint16_t bin;

    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
    {
        reset_all_predictors(state, frame_len);
    } else {
        for (sfb = 0; sfb < ics->pred.limit; sfb++)
        {
            uint16_t low  = ics->swb_offset[sfb];
            uint16_t high = ics->swb_offset[sfb+1];

            for (bin = low; bin < high; bin++)
            {
                ic_predict(&state[bin], spec[bin], &spec[bin],
                    (ics->predictor_data_present &&
                    ics->pred.prediction_used[sfb]));
            }
        }

        if (ics->predictor_data_present)
        {
            if (ics->pred.predictor_reset)
            {
                for (bin = ics->pred.predictor_reset_group_number - 1;
                     bin < frame_len; bin += 30)
                {
                    reset_pred_state(&state[bin]);
                }
            }
        }
    }
}

#endif
