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
** $Id: ssr.c,v 1.15 2004/09/04 14:56:29 menno Exp $
**/

#include "common.h"
#include "structs.h"

#ifdef SSR_DEC

#include "syntax.h"
#include "filtbank.h"
#include "ssr.h"
#include "ssr_fb.h"

void ssr_decode(ssr_info *ssr, fb_info *fb, uint8_t window_sequence,
                uint8_t window_shape, uint8_t window_shape_prev,
                real_t *freq_in, real_t *time_out, real_t *overlap,
                real_t ipqf_buffer[SSR_BANDS][96/4],
                real_t *prev_fmd, uint16_t frame_len)
{
    uint8_t band;
    uint16_t ssr_frame_len = frame_len/SSR_BANDS;
    real_t time_tmp[2048] = {0};
    real_t output[1024] = {0};

    for (band = 0; band < SSR_BANDS; band++)
    {
        int16_t j;

        /* uneven bands have inverted frequency scale */
        if (band == 1 || band == 3)
        {
            for (j = 0; j < ssr_frame_len/2; j++)
            {
                real_t tmp;
                tmp = freq_in[j + ssr_frame_len*band];
                freq_in[j + ssr_frame_len*band] =
                    freq_in[ssr_frame_len - j - 1 + ssr_frame_len*band];
                freq_in[ssr_frame_len - j - 1 + ssr_frame_len*band] = tmp;
            }
        }

        /* non-overlapping inverse filterbank for SSR */
        ssr_ifilter_bank(fb, window_sequence, window_shape, window_shape_prev,
            freq_in + band*ssr_frame_len, time_tmp + band*ssr_frame_len,
            ssr_frame_len);

        /* gain control */
        ssr_gain_control(ssr, time_tmp, output, overlap, prev_fmd,
            band, window_sequence, ssr_frame_len);
    }

    /* inverse pqf to bring subbands together again */
    ssr_ipqf(ssr, output, time_out, ipqf_buffer, frame_len, SSR_BANDS);
}

static void ssr_gain_control(ssr_info *ssr, real_t *data, real_t *output,
                             real_t *overlap, real_t *prev_fmd, uint8_t band,
                             uint8_t window_sequence, uint16_t frame_len)
{
    uint16_t i;
    real_t gc_function[2*1024/SSR_BANDS];

    if (window_sequence != EIGHT_SHORT_SEQUENCE)
    {
        ssr_gc_function(ssr, &prev_fmd[band * frame_len*2],
            gc_function, window_sequence, band, frame_len);

        for (i = 0; i < frame_len*2; i++)
            data[band * frame_len*2 + i] *= gc_function[i];
        for (i = 0; i < frame_len; i++)
        {
            output[band*frame_len + i] = overlap[band*frame_len + i] +
                data[band*frame_len*2 + i];
        }
        for (i = 0; i < frame_len; i++)
        {
            overlap[band*frame_len + i] =
                data[band*frame_len*2 + frame_len + i];
        }
    } else {
        uint8_t w;
        for (w = 0; w < 8; w++)
        {
            uint16_t frame_len8 = frame_len/8;
            uint16_t frame_len16 = frame_len/16;

            ssr_gc_function(ssr, &prev_fmd[band*frame_len*2 + w*frame_len*2/8],
                gc_function, window_sequence, frame_len);

            for (i = 0; i < frame_len8*2; i++)
                data[band*frame_len*2 + w*frame_len8*2+i] *= gc_function[i];
            for (i = 0; i < frame_len8; i++)
            {
                overlap[band*frame_len + i + 7*frame_len16 + w*frame_len8] +=
                    data[band*frame_len*2 + 2*w*frame_len8 + i];
            }
            for (i = 0; i < frame_len8; i++)
            {
                overlap[band*frame_len + i + 7*frame_len16 + (w+1)*frame_len8] =
                    data[band*frame_len*2 + 2*w*frame_len8 + frame_len8 + i];
            }
        }
        for (i = 0; i < frame_len; i++)
            output[band*frame_len + i] = overlap[band*frame_len + i];
        for (i = 0; i < frame_len; i++)
            overlap[band*frame_len + i] = overlap[band*frame_len + i + frame_len];
    }
}

static void ssr_gc_function(ssr_info *ssr, real_t *prev_fmd,
                            real_t *gc_function, uint8_t window_sequence,
                            uint8_t band, uint16_t frame_len)
{
    uint16_t i;
    uint16_t len_area1, len_area2;
    int32_t aloc[10];
    real_t alev[10];

    switch (window_sequence)
    {
    case ONLY_LONG_SEQUENCE:
        len_area1 = frame_len/SSR_BANDS;
        len_area2 = 0;
        break;
    case LONG_START_SEQUENCE:
        len_area1 = (frame_len/SSR_BANDS)*7/32;
        len_area2 = (frame_len/SSR_BANDS)/16;
        break;
    case EIGHT_SHORT_SEQUENCE:
        len_area1 = (frame_len/8)/SSR_BANDS;
        len_area2 = 0;
        break;
    case LONG_STOP_SEQUENCE:
        len_area1 = (frame_len/SSR_BANDS);
        len_area2 = 0;
        break;
    }

    /* decode bitstream information */

    /* build array M */


    for (i = 0; i < frame_len*2; i++)
        gc_function[i] = 1;
}

#endif
