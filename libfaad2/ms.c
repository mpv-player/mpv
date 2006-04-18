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
** $Id: ms.c,v 1.17 2004/09/04 14:56:28 menno Exp $
**/

#include "common.h"
#include "structs.h"

#include "syntax.h"
#include "ms.h"
#include "is.h"
#include "pns.h"

void ms_decode(ic_stream *ics, ic_stream *icsr, real_t *l_spec, real_t *r_spec,
               uint16_t frame_len)
{
    uint8_t g, b, sfb;
    uint8_t group = 0;
    uint16_t nshort = frame_len/8;

    uint16_t i, k;
    real_t tmp;

    if (ics->ms_mask_present >= 1)
    {
        for (g = 0; g < ics->num_window_groups; g++) 
        {
            for (b = 0; b < ics->window_group_length[g]; b++)
            {
                for (sfb = 0; sfb < ics->max_sfb; sfb++)
                {
                    /* If intensity stereo coding or noise substitution is on
                       for a particular scalefactor band, no M/S stereo decoding
                       is carried out.
                     */
                    if ((ics->ms_used[g][sfb] || ics->ms_mask_present == 2) &&
                        !is_intensity(icsr, g, sfb) && !is_noise(ics, g, sfb))
                    {
                        for (i = ics->swb_offset[sfb]; i < ics->swb_offset[sfb+1]; i++)
                        {
                            k = (group*nshort) + i;
                            tmp = l_spec[k] - r_spec[k];
                            l_spec[k] = l_spec[k] + r_spec[k];
                            r_spec[k] = tmp;
                        }
                    }
                }
                group++;
            }
        }
    }
}
