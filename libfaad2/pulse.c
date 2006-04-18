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
** $Id: pulse.c,v 1.17 2004/09/04 14:56:28 menno Exp $
**/

#include "common.h"
#include "structs.h"

#include "syntax.h"
#include "pulse.h"

uint8_t pulse_decode(ic_stream *ics, int16_t *spec_data, uint16_t framelen)
{
    uint8_t i;
    uint16_t k;
    pulse_info *pul = &(ics->pul);

    k = ics->swb_offset[pul->pulse_start_sfb];

    for (i = 0; i <= pul->number_pulse; i++)
    {
        k += pul->pulse_offset[i];

        if (k >= framelen)
            return 15; /* should not be possible */

        if (spec_data[k] > 0)
            spec_data[k] += pul->pulse_amp[i];
        else
            spec_data[k] -= pul->pulse_amp[i];
    }

    return 0;
}
