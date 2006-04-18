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
** $Id: is.c,v 1.24 2004/09/04 14:56:28 menno Exp $
**/

#include "common.h"
#include "structs.h"

#include "syntax.h"
#include "is.h"

#ifdef FIXED_POINT
static real_t pow05_table[] = {
    COEF_CONST(1.68179283050743), /* 0.5^(-3/4) */
    COEF_CONST(1.41421356237310), /* 0.5^(-2/4) */
    COEF_CONST(1.18920711500272), /* 0.5^(-1/4) */
    COEF_CONST(1.0),              /* 0.5^( 0/4) */
    COEF_CONST(0.84089641525371), /* 0.5^(+1/4) */
    COEF_CONST(0.70710678118655), /* 0.5^(+2/4) */
    COEF_CONST(0.59460355750136)  /* 0.5^(+3/4) */
};
#endif

void is_decode(ic_stream *ics, ic_stream *icsr, real_t *l_spec, real_t *r_spec,
               uint16_t frame_len)
{
    uint8_t g, sfb, b;
    uint16_t i;
#ifndef FIXED_POINT
    real_t scale;
#else
    int32_t exp, frac;
#endif

    uint16_t nshort = frame_len/8;
    uint8_t group = 0;

    for (g = 0; g < icsr->num_window_groups; g++)
    {
        /* Do intensity stereo decoding */
        for (b = 0; b < icsr->window_group_length[g]; b++)
        {
            for (sfb = 0; sfb < icsr->max_sfb; sfb++)
            {
                if (is_intensity(icsr, g, sfb))
                {
#ifdef MAIN_DEC
                    /* For scalefactor bands coded in intensity stereo the
                       corresponding predictors in the right channel are
                       switched to "off".
                     */
                    ics->pred.prediction_used[sfb] = 0;
                    icsr->pred.prediction_used[sfb] = 0;
#endif

#ifndef FIXED_POINT
                    scale = (real_t)pow(0.5, (0.25*icsr->scale_factors[g][sfb]));
#else
                    exp = icsr->scale_factors[g][sfb] >> 2;
                    frac = icsr->scale_factors[g][sfb] & 3;
#endif

                    /* Scale from left to right channel,
                       do not touch left channel */
                    for (i = icsr->swb_offset[sfb]; i < icsr->swb_offset[sfb+1]; i++)
                    {
#ifndef FIXED_POINT
                        r_spec[(group*nshort)+i] = MUL_R(l_spec[(group*nshort)+i], scale);
#else
                        if (exp < 0)
                            r_spec[(group*nshort)+i] = l_spec[(group*nshort)+i] << -exp;
                        else
                            r_spec[(group*nshort)+i] = l_spec[(group*nshort)+i] >> exp;
                        r_spec[(group*nshort)+i] = MUL_C(r_spec[(group*nshort)+i], pow05_table[frac + 3]);
#endif
                        if (is_intensity(icsr, g, sfb) != invert_intensity(ics, g, sfb))
                            r_spec[(group*nshort)+i] = -r_spec[(group*nshort)+i];
                    }
                }
            }
            group++;
        }
    }
}
