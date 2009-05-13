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
** $Id: sbr_tf_grid.c,v 1.15 2004/09/04 14:56:28 menno Exp $
**/

/* Time/Frequency grid */

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include <stdlib.h>

#include "sbr_syntax.h"
#include "sbr_tf_grid.h"


/* static function declarations */
#if 0
static int16_t rel_bord_lead(sbr_info *sbr, uint8_t ch, uint8_t l);
static int16_t rel_bord_trail(sbr_info *sbr, uint8_t ch, uint8_t l);
#endif
static uint8_t middleBorder(sbr_info *sbr, uint8_t ch);


/* function constructs new time border vector */
/* first build into temp vector to be able to use previous vector on error */
uint8_t envelope_time_border_vector(sbr_info *sbr, uint8_t ch)
{
    uint8_t l, border, temp;
    uint8_t t_E_temp[6] = {0};

    t_E_temp[0] = sbr->rate * sbr->abs_bord_lead[ch];
    t_E_temp[sbr->L_E[ch]] = sbr->rate * sbr->abs_bord_trail[ch];

    switch (sbr->bs_frame_class[ch])
    {
    case FIXFIX:
        switch (sbr->L_E[ch])
        {
        case 4:
            temp = (int) (sbr->numTimeSlots / 4);
            t_E_temp[3] = sbr->rate * 3 * temp;
            t_E_temp[2] = sbr->rate * 2 * temp;
            t_E_temp[1] = sbr->rate * temp;
            break;
        case 2:
            t_E_temp[1] = sbr->rate * (int) (sbr->numTimeSlots / 2);
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
                if (border < sbr->bs_rel_bord[ch][l])
                    return 1;

                border -= sbr->bs_rel_bord[ch][l];
                t_E_temp[--i] = sbr->rate * border;
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

                if (sbr->rate * border + sbr->tHFAdj > sbr->numTimeSlotsRate+sbr->tHFGen)
                    return 1;

                t_E_temp[i++] = sbr->rate * border;
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

                if (sbr->rate * border + sbr->tHFAdj > sbr->numTimeSlotsRate+sbr->tHFGen)
                    return 1;

                t_E_temp[i++] = sbr->rate * border;
            }
        }

        if (sbr->bs_num_rel_1[ch])
        {
            int8_t i = sbr->L_E[ch];
            border = sbr->abs_bord_trail[ch];

            for (l = 0; l < sbr->bs_num_rel_1[ch]; l++)
            {
                if (border < sbr->bs_rel_bord_1[ch][l])
                    return 1;

                border -= sbr->bs_rel_bord_1[ch][l];
                t_E_temp[--i] = sbr->rate * border;
            }
        }
        break;
    }

    /* no error occured, we can safely use this t_E vector */
    for (l = 0; l < 6; l++)
    {
        sbr->t_E[ch][l] = t_E_temp[l];
    }

    return 0;
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

#if 0
static int16_t rel_bord_lead(sbr_info *sbr, uint8_t ch, uint8_t l)
{
    uint8_t i;
    int16_t acc = 0;

    switch (sbr->bs_frame_class[ch])
    {
    case FIXFIX:
        return sbr->numTimeSlots/sbr->L_E[ch];
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
#endif

static uint8_t middleBorder(sbr_info *sbr, uint8_t ch)
{
    int8_t retval = 0;

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
