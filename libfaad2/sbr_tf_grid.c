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
** $Id: sbr_tf_grid.c,v 1.1 2003/07/29 08:20:13 menno Exp $
**/

/* Time/Frequency grid */

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include <stdlib.h>

#include "sbr_syntax.h"
#include "sbr_tf_grid.h"

void envelope_time_border_vector(sbr_info *sbr, uint8_t ch)
{
    uint8_t l, border;

    for (l = 0; l <= sbr->L_E[ch]; l++)
    {
        sbr->t_E[ch][l] = 0;
    }

    sbr->t_E[ch][0] = sbr->rate * sbr->abs_bord_lead[ch];
    sbr->t_E[ch][sbr->L_E[ch]] = sbr->rate * sbr->abs_bord_trail[ch];

    switch (sbr->bs_frame_class[ch])
    {
    case FIXFIX:
        switch (sbr->L_E[ch])
        {
        case 4:
            sbr->t_E[ch][3] = sbr->rate * 12;
            sbr->t_E[ch][2] = sbr->rate * 8;
            sbr->t_E[ch][1] = sbr->rate * 4;
            break;
        case 2:
            sbr->t_E[ch][1] = sbr->rate * 8;
            break;
        default:
            break;
        }
        break;

    case FIXVAR:
        if (sbr->L_E[ch] > 1)
        {
            int8_t i = sbr->L_E[ch];
            border = sbr->abs_bord_trail[ch];

            for (l = 0; l < (sbr->L_E[ch] - 1); l++)
            {
                border -= sbr->bs_rel_bord[ch][l];
                sbr->t_E[ch][--i] = sbr->rate * border;
            }
        }
        break;

    case VARFIX:
        if (sbr->L_E[ch] > 1)
        {
            int8_t i = 1;
            border = sbr->abs_bord_lead[ch];

            for (l = 0; l < (sbr->L_E[ch] - 1); l++)
            {
                border += sbr->bs_rel_bord[ch][l];
                sbr->t_E[ch][i++] = sbr->rate * border;
            }
        }
        break;

    case VARVAR:
        if (sbr->bs_num_rel_0[ch])
        {
            int8_t i = 1;
            border = sbr->abs_bord_lead[ch];

            for (l = 0; l < sbr->bs_num_rel_0[ch]; l++)
            {
                border += sbr->bs_rel_bord_0[ch][l];
                sbr->t_E[ch][i++] = sbr->rate * border;
            }
        }

        if (sbr->bs_num_rel_1[ch])
        {
            int8_t i = sbr->L_E[ch];
            border = sbr->abs_bord_trail[ch];

            for (l = 0; l < sbr->bs_num_rel_1[ch]; l++)
            {
                border -= sbr->bs_rel_bord_1[ch][l];
                sbr->t_E[ch][--i] = sbr->rate * border;
            }
        }
        break;
    }
}

void noise_floor_time_border_vector(sbr_info *sbr, uint8_t ch)
{
    sbr->t_Q[ch][0] = sbr->t_E[ch][0];

    if (sbr->L_E[ch] == 1)
    {
        sbr->t_Q[ch][1] = sbr->t_E[ch][1];
        sbr->t_Q[ch][2] = 0;
    } else {
        uint8_t index = middleBorder(sbr, ch);
        sbr->t_Q[ch][1] = sbr->t_E[ch][index];
        sbr->t_Q[ch][2] = sbr->t_E[ch][sbr->L_E[ch]];
    }
}

static int16_t rel_bord_lead(sbr_info *sbr, uint8_t ch, uint8_t l)
{
    uint8_t i;
    int16_t acc = 0;

    switch (sbr->bs_frame_class[ch])
    {
    case FIXFIX:
        return NO_TIME_SLOTS/sbr->L_E[ch];
    case FIXVAR:
        return 0;
    case VARFIX:
        for (i = 0; i < l; i++)
        {
            acc += sbr->bs_rel_bord[ch][i];
        }
        return acc;
    case VARVAR:
        for (i = 0; i < l; i++)
        {
            acc += sbr->bs_rel_bord_0[ch][i];
        }
        return acc;
    }

    return 0;
}

static int16_t rel_bord_trail(sbr_info *sbr, uint8_t ch, uint8_t l)
{
    uint8_t i;
    int16_t acc = 0;

    switch (sbr->bs_frame_class[ch])
    {
    case FIXFIX:
    case VARFIX:
        return 0;
    case FIXVAR:
        for (i = 0; i < l; i++)
        {
            acc += sbr->bs_rel_bord[ch][i];
        }
        return acc;
    case VARVAR:
        for (i = 0; i < l; i++)
        {
            acc += sbr->bs_rel_bord_1[ch][i];
        }
        return acc;
    }

    return 0;
}

static uint8_t middleBorder(sbr_info *sbr, uint8_t ch)
{
    int8_t retval;

    switch (sbr->bs_frame_class[ch])
    {
    case FIXFIX:
        retval = sbr->L_E[ch]/2;
        break;
    case VARFIX:
        if (sbr->bs_pointer[ch] == 0)
            retval = 1;
        else if (sbr->bs_pointer[ch] == 1)
            retval = sbr->L_E[ch] - 1;
        else
            retval = sbr->bs_pointer[ch] - 1;
        break;
    case FIXVAR:
    case VARVAR:
        if (sbr->bs_pointer[ch] > 1)
            retval = sbr->L_E[ch] + 1 - sbr->bs_pointer[ch];
        else
            retval = sbr->L_E[ch] - 1;
        break;
    }

    return (retval > 0) ? retval : 0;
}


#endif
