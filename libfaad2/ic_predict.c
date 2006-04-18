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
** $Id: ic_predict.c,v 1.23 2004/09/04 14:56:28 menno Exp $
**/

#include "common.h"
#include "structs.h"

#ifdef MAIN_DEC

#include "syntax.h"
#include "ic_predict.h"
#include "pns.h"


static void flt_round(float32_t *pf)
{
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
        
        *pf = *(float32_t*)&tmp1 + *(float32_t*)&tmp2 - *(float32_t*)&tmp;
    } else {
        *pf = *(float32_t*)&tmp;
    }
}

static int16_t quant_pred(float32_t x)
{
    int16_t q;
    uint32_t *tmp = (uint32_t*)&x;

    q = (int16_t)(*tmp>>16);

    return q;
}

static float32_t inv_quant_pred(int16_t q)
{
    float32_t x;
    uint32_t *tmp = (uint32_t*)&x;
    *tmp = ((uint32_t)q)<<16;

    return x;
}

static void ic_predict(pred_state *state, real_t input, real_t *output, uint8_t pred)
{
    uint16_t tmp;
    int16_t i, j;
    real_t dr1, predictedvalue;
    real_t e0, e1;
    real_t k1, k2;

    real_t r[2];
    real_t COR[2];
    real_t VAR[2];

    r[0] = inv_quant_pred(state->r[0]);
    r[1] = inv_quant_pred(state->r[1]);
    COR[0] = inv_quant_pred(state->COR[0]);
    COR[1] = inv_quant_pred(state->COR[1]);
    VAR[0] = inv_quant_pred(state->VAR[0]);
    VAR[1] = inv_quant_pred(state->VAR[1]);


#if 1
    tmp = state->VAR[0];
    j = (tmp >> 7);
    i = tmp & 0x7f;
    if (j >= 128)
    {
        j -= 128;
        k1 = COR[0] * exp_table[j] * mnt_table[i];
    } else {
        k1 = REAL_CONST(0);
    }
#else

    {
#define B 0.953125
        real_t c = COR[0];
        real_t v = VAR[0];
        real_t tmp;
        if (c == 0 || v <= 1)
        {
            k1 = 0;
        } else {
            tmp = B / v;
            flt_round(&tmp);
            k1 = c * tmp;
        }
    }
#endif

    if (pred)
    {
#if 1
        tmp = state->VAR[1];
        j = (tmp >> 7);
        i = tmp & 0x7f;
        if (j >= 128)
        {
            j -= 128;
            k2 = COR[1] * exp_table[j] * mnt_table[i];
        } else {
            k2 = REAL_CONST(0);
        }
#else

#define B 0.953125
        real_t c = COR[1];
        real_t v = VAR[1];
        real_t tmp;
        if (c == 0 || v <= 1)
        {
            k2 = 0;
        } else {
            tmp = B / v;
            flt_round(&tmp);
            k2 = c * tmp;
        }
#endif

        predictedvalue = k1*r[0] + k2*r[1];
        flt_round(&predictedvalue);
        *output = input + predictedvalue;
    }

    /* calculate new state data */
    e0 = *output;
    e1 = e0 - k1*r[0];
    dr1 = k1*e0;

    VAR[0] = ALPHA*VAR[0] + 0.5f * (r[0]*r[0] + e0*e0);
    COR[0] = ALPHA*COR[0] + r[0]*e0;
    VAR[1] = ALPHA*VAR[1] + 0.5f * (r[1]*r[1] + e1*e1);
    COR[1] = ALPHA*COR[1] + r[1]*e1;

    r[1] = A * (r[0]-dr1);
    r[0] = A * e0;

    state->r[0] = quant_pred(r[0]);
    state->r[1] = quant_pred(r[1]);
    state->COR[0] = quant_pred(COR[0]);
    state->COR[1] = quant_pred(COR[1]);
    state->VAR[0] = quant_pred(VAR[0]);
    state->VAR[1] = quant_pred(VAR[1]);
}

static void reset_pred_state(pred_state *state)
{
    state->r[0]   = 0;
    state->r[1]   = 0;
    state->COR[0] = 0;
    state->COR[1] = 0;
    state->VAR[0] = 0x3F80;
    state->VAR[1] = 0x3F80;
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
                   uint16_t frame_len, uint8_t sf_index)
{
    uint8_t sfb;
    uint16_t bin;

    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
    {
        reset_all_predictors(state, frame_len);
    } else {
        for (sfb = 0; sfb < max_pred_sfb(sf_index); sfb++)
        {
            uint16_t low  = ics->swb_offset[sfb];
            uint16_t high = ics->swb_offset[sfb+1];

            for (bin = low; bin < high; bin++)
            {
                ic_predict(&state[bin], spec[bin], &spec[bin],
                    (ics->predictor_data_present && ics->pred.prediction_used[sfb]));
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
