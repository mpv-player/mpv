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
** $Id: pns.c,v 1.21 2003/07/29 08:20:12 menno Exp $
**/

#include "common.h"
#include "structs.h"

#include "pns.h"


#ifdef FIXED_POINT

#define DIV(A, B) (((int64_t)A << COEF_BITS)/B)

#define step(shift) \
    if ((0x40000000l >> shift) + root <= value)       \
    {                                                 \
        value -= (0x40000000l >> shift) + root;       \
        root = (root >> 1) | (0x40000000l >> shift);  \
    } else {                                          \
        root = root >> 1;                             \
    }

/* fixed point square root approximation */
real_t fp_sqrt(real_t value)
{
    real_t root = 0;

    step( 0); step( 2); step( 4); step( 6);
    step( 8); step(10); step(12); step(14);
    step(16); step(18); step(20); step(22);
    step(24); step(26); step(28); step(30);

    if (root < value)
        ++root;

    root <<= (COEF_BITS/2);

    return root;
}

static real_t pow2_table[] =
{
    COEF_CONST(0.59460355750136),
    COEF_CONST(0.70710678118655),
    COEF_CONST(0.84089641525371),
    COEF_CONST(1.0),
    COEF_CONST(1.18920711500272),
    COEF_CONST(1.41421356237310),
    COEF_CONST(1.68179283050743)
};
#endif

/* The function gen_rand_vector(addr, size) generates a vector of length
   <size> with signed random values of average energy MEAN_NRG per random
   value. A suitable random number generator can be realized using one
   multiplication/accumulation per random value.
*/
static INLINE void gen_rand_vector(real_t *spec, int16_t scale_factor, uint16_t size)
{
#ifndef FIXED_POINT
    uint16_t i;
    real_t energy = 0.0;

    real_t scale = 1.0/(real_t)size * ISQRT_MEAN_NRG;

    for (i = 0; i < size; i++)
    {
        real_t tmp = scale*(real_t)(int32_t)random_int();
        spec[i] = tmp;
        energy += tmp*tmp;
    }

    scale = 1.0/(real_t)sqrt(energy);
    scale *= (real_t)pow(2.0, 0.25 * scale_factor);
    for (i = 0; i < size; i++)
    {
        spec[i] *= scale;
    }
#else
    uint16_t i;
    real_t energy = 0, scale;
    int32_t exp, frac;

    for (i = 0; i < size; i++)
    {
        real_t tmp = ISQRT_MEAN_NRG * (int32_t)random_int();
        tmp = MUL_C_C(COEF_CONST(1)/size, tmp);

        energy += MUL_C_C(tmp,tmp);

        /* convert COEF to REAL */
        spec[i] = (tmp >> -(REAL_BITS-COEF_BITS));
    }

    energy = fp_sqrt(energy);
    if (energy > 0)
    {
        scale = DIV(COEF_CONST(1),energy);

        scale >>= -(REAL_BITS-COEF_BITS);

        exp = scale_factor / 4;
        frac = scale_factor % 4;

        if (exp < 0)
            scale >>= -exp;
        else
            scale <<= exp;

        if (frac)
            scale = MUL_R_C(scale, pow2_table[frac + 3]);

        for (i = 0; i < size; i++)
        {
            spec[i] = MUL(spec[i], scale);
        }
    }
#endif
}

void pns_decode(ic_stream *ics_left, ic_stream *ics_right,
                real_t *spec_left, real_t *spec_right, uint16_t frame_len,
                uint8_t channel_pair)
{
    uint8_t g, sfb, b;
    uint16_t size, offs;

    uint8_t group = 0;
    uint16_t nshort = frame_len >> 3;

    for (g = 0; g < ics_left->num_window_groups; g++)
    {
        /* Do perceptual noise substitution decoding */
        for (b = 0; b < ics_left->window_group_length[g]; b++)
        {
            for (sfb = 0; sfb < ics_left->max_sfb; sfb++)
            {
                if (is_noise(ics_left, g, sfb))
                {
                    /* Simultaneous use of LTP and PNS is not prevented in the
                       syntax. If both LTP, and PNS are enabled on the same
                       scalefactor band, PNS takes precedence, and no prediction
                       is applied to this band.
                    */
                    ics_left->ltp.long_used[sfb] = 0;
                    ics_left->ltp2.long_used[sfb] = 0;

                    /* For scalefactor bands coded using PNS the corresponding
                       predictors are switched to "off".
                    */
                    ics_left->pred.prediction_used[sfb] = 0;

                    offs = ics_left->swb_offset[sfb];
                    size = ics_left->swb_offset[sfb+1] - offs;

                    /* Generate random vector */
                    gen_rand_vector(&spec_left[(group*nshort)+offs],
                        ics_left->scale_factors[g][sfb], size);
                }

/* From the spec:
   If the same scalefactor band and group is coded by perceptual noise
   substitution in both channels of a channel pair, the correlation of
   the noise signal can be controlled by means of the ms_used field: While
   the default noise generation process works independently for each channel
   (separate generation of random vectors), the same random vector is used
   for both channels if ms_used[] is set for a particular scalefactor band
   and group. In this case, no M/S stereo coding is carried out (because M/S
   stereo coding and noise substitution coding are mutually exclusive).
   If the same scalefactor band and group is coded by perceptual noise
   substitution in only one channel of a channel pair the setting of ms_used[]
   is not evaluated.
*/
                if (channel_pair)
                {
                    if (is_noise(ics_right, g, sfb))
                    {
                        if (((ics_left->ms_mask_present == 1) &&
                            (ics_left->ms_used[g][sfb])) ||
                            (ics_left->ms_mask_present == 2))
                        {
                            uint16_t c;

                            offs = ics_right->swb_offset[sfb];
                            size = ics_right->swb_offset[sfb+1] - offs;

                            for (c = 0; c < size; c++)
                            {
                                spec_right[(group*nshort) + offs + c] =
                                    spec_left[(group*nshort) + offs + c];
                            }
                        } else /*if (ics_left->ms_mask_present == 0)*/ {
                            ics_right->ltp.long_used[sfb] = 0;
                            ics_right->ltp2.long_used[sfb] = 0;
                            ics_right->pred.prediction_used[sfb] = 0;

                            offs = ics_right->swb_offset[sfb];
                            size = ics_right->swb_offset[sfb+1] - offs;

                            /* Generate random vector */
                            gen_rand_vector(&spec_right[(group*nshort)+offs],
                                ics_right->scale_factors[g][sfb], size);
                        }
                    }
                }
            } /* sfb */
            group++;
        } /* b */
    } /* g */
}
