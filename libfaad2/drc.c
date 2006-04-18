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
** $Id: drc.c,v 1.24 2004/09/04 14:56:28 menno Exp $
**/

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include <string.h>
#include "syntax.h"
#include "drc.h"

drc_info *drc_init(real_t cut, real_t boost)
{
    drc_info *drc = (drc_info*)faad_malloc(sizeof(drc_info));
    memset(drc, 0, sizeof(drc_info));

    drc->ctrl1 = cut;
    drc->ctrl2 = boost;

    drc->num_bands = 1;
    drc->band_top[0] = 1024/4 - 1;
    drc->dyn_rng_sgn[0] = 1;
    drc->dyn_rng_ctl[0] = 0;

    return drc;
}

void drc_end(drc_info *drc)
{
    if (drc) faad_free(drc);
}

#ifdef FIXED_POINT
static real_t drc_pow2_table[] =
{
    COEF_CONST(0.5146511183),
    COEF_CONST(0.5297315472),
    COEF_CONST(0.5452538663),
    COEF_CONST(0.5612310242),
    COEF_CONST(0.5776763484),
    COEF_CONST(0.5946035575),
    COEF_CONST(0.6120267717),
    COEF_CONST(0.6299605249),
    COEF_CONST(0.6484197773),
    COEF_CONST(0.6674199271),
    COEF_CONST(0.6869768237),
    COEF_CONST(0.7071067812),
    COEF_CONST(0.7278265914),
    COEF_CONST(0.7491535384),
    COEF_CONST(0.7711054127),
    COEF_CONST(0.7937005260),
    COEF_CONST(0.8169577266),
    COEF_CONST(0.8408964153),
    COEF_CONST(0.8655365610),
    COEF_CONST(0.8908987181),
    COEF_CONST(0.9170040432),
    COEF_CONST(0.9438743127),
    COEF_CONST(0.9715319412),
    COEF_CONST(1.0000000000),
    COEF_CONST(1.0293022366),
    COEF_CONST(1.0594630944),
    COEF_CONST(1.0905077327),
    COEF_CONST(1.1224620483),
    COEF_CONST(1.1553526969),
    COEF_CONST(1.1892071150),
    COEF_CONST(1.2240535433),
    COEF_CONST(1.2599210499),
    COEF_CONST(1.2968395547),
    COEF_CONST(1.3348398542),
    COEF_CONST(1.3739536475),
    COEF_CONST(1.4142135624),
    COEF_CONST(1.4556531828),
    COEF_CONST(1.4983070769),
    COEF_CONST(1.5422108254),
    COEF_CONST(1.5874010520),
    COEF_CONST(1.6339154532),
    COEF_CONST(1.6817928305),
    COEF_CONST(1.7310731220),
    COEF_CONST(1.7817974363),
    COEF_CONST(1.8340080864),
    COEF_CONST(1.8877486254),
    COEF_CONST(1.9430638823)
};
#endif

void drc_decode(drc_info *drc, real_t *spec)
{
    uint16_t i, bd, top;
#ifdef FIXED_POINT
    int32_t exp, frac;
#else
    real_t factor, exp;
#endif
    uint16_t bottom = 0;

    if (drc->num_bands == 1)
        drc->band_top[0] = 1024/4 - 1;

    for (bd = 0; bd < drc->num_bands; bd++)
    {
        top = 4 * (drc->band_top[bd] + 1);

#ifndef FIXED_POINT
        /* Decode DRC gain factor */
        if (drc->dyn_rng_sgn[bd])  /* compress */
            exp = -drc->ctrl1 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level))/REAL_CONST(24.0);
        else /* boost */
            exp = drc->ctrl2 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level))/REAL_CONST(24.0);
        factor = (real_t)pow(2.0, exp);

        /* Apply gain factor */
        for (i = bottom; i < top; i++)
            spec[i] *= factor;
#else
        /* Decode DRC gain factor */
        if (drc->dyn_rng_sgn[bd])  /* compress */
        {
            exp = -1 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level))/ 24;
            frac = -1 * (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) % 24;
        } else { /* boost */
            exp = (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level))/ 24;
            frac = (drc->dyn_rng_ctl[bd] - (DRC_REF_LEVEL - drc->prog_ref_level)) % 24;
        }

        /* Apply gain factor */
        if (exp < 0)
        {
            for (i = bottom; i < top; i++)
            {
                spec[i] >>= -exp;
                if (frac)
                    spec[i] = MUL_R(spec[i],drc_pow2_table[frac+23]);
            }
        } else {
            for (i = bottom; i < top; i++)
            {
                spec[i] <<= exp;
                if (frac)
                    spec[i] = MUL_R(spec[i],drc_pow2_table[frac+23]);
            }
        }
#endif

        bottom = top;
    }
}
