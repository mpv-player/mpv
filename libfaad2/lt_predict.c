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
** $Id: lt_predict.c,v 1.23 2004/09/04 14:56:28 menno Exp $
**/


#include "common.h"
#include "structs.h"

#ifdef LTP_DEC

#include <stdlib.h>
#include "syntax.h"
#include "lt_predict.h"
#include "filtbank.h"
#include "tns.h"


/* static function declarations */
static int16_t real_to_int16(real_t sig_in);


/* check if the object type is an object type that can have LTP */
uint8_t is_ltp_ot(uint8_t object_type)
{
#ifdef LTP_DEC
    if ((object_type == LTP)
#ifdef ERROR_RESILIENCE
        || (object_type == ER_LTP)
#endif
#ifdef LD_DEC
        || (object_type == LD)
#endif
#ifdef SCALABLE_DEC
        || (object_type == 6) /* TODO */
#endif
        )
    {
        return 1;
    }
#endif

    return 0;
}

ALIGN static const real_t codebook[8] =
{
    REAL_CONST(0.570829),
    REAL_CONST(0.696616),
    REAL_CONST(0.813004),
    REAL_CONST(0.911304),
    REAL_CONST(0.984900),
    REAL_CONST(1.067894),
    REAL_CONST(1.194601),
    REAL_CONST(1.369533)
};

void lt_prediction(ic_stream *ics, ltp_info *ltp, real_t *spec,
                   int16_t *lt_pred_stat, fb_info *fb, uint8_t win_shape,
                   uint8_t win_shape_prev, uint8_t sr_index,
                   uint8_t object_type, uint16_t frame_len)
{
    uint8_t sfb;
    uint16_t bin, i, num_samples;
    ALIGN real_t x_est[2048];
    ALIGN real_t X_est[2048];

    if (ics->window_sequence != EIGHT_SHORT_SEQUENCE)
    {
        if (ltp->data_present)
        {
            num_samples = frame_len << 1;

            for(i = 0; i < num_samples; i++)
            {
                /* The extra lookback M (N/2 for LD, 0 for LTP) is handled
                   in the buffer updating */

#if 0
                x_est[i] = MUL_R_C(lt_pred_stat[num_samples + i - ltp->lag],
                    codebook[ltp->coef]);
#else
                /* lt_pred_stat is a 16 bit int, multiplied with the fixed point real
                   this gives a real for x_est
                */
                x_est[i] = (real_t)lt_pred_stat[num_samples + i - ltp->lag] * codebook[ltp->coef];
#endif
            }

            filter_bank_ltp(fb, ics->window_sequence, win_shape, win_shape_prev,
                x_est, X_est, object_type, frame_len);

            tns_encode_frame(ics, &(ics->tns), sr_index, object_type, X_est,
                frame_len);

            for (sfb = 0; sfb < ltp->last_band; sfb++)
            {
                if (ltp->long_used[sfb])
                {
                    uint16_t low  = ics->swb_offset[sfb];
                    uint16_t high = ics->swb_offset[sfb+1];

                    for (bin = low; bin < high; bin++)
                    {
                        spec[bin] += X_est[bin];
                    }
                }
            }
        }
    }
}

#ifdef FIXED_POINT
static INLINE int16_t real_to_int16(real_t sig_in)
{
    if (sig_in >= 0)
    {
        sig_in += (1 << (REAL_BITS-1));
        if (sig_in >= REAL_CONST(32768))
            return 32767;
    } else {
        sig_in += -(1 << (REAL_BITS-1));
        if (sig_in <= REAL_CONST(-32768))
            return -32768;
    }

    return (sig_in >> REAL_BITS);
}
#else
static INLINE int16_t real_to_int16(real_t sig_in)
{
    if (sig_in >= 0)
    {
#ifndef HAS_LRINTF
        sig_in += 0.5f;
#endif
        if (sig_in >= 32768.0f)
            return 32767;
    } else {
#ifndef HAS_LRINTF
        sig_in += -0.5f;
#endif
        if (sig_in <= -32768.0f)
            return -32768;
    }

    return lrintf(sig_in);
}
#endif

void lt_update_state(int16_t *lt_pred_stat, real_t *time, real_t *overlap,
                     uint16_t frame_len, uint8_t object_type)
{
    uint16_t i;

    /*
     * The reference point for index i and the content of the buffer
     * lt_pred_stat are arranged so that lt_pred_stat(0 ... N/2 - 1) contains the
     * last aliased half window from the IMDCT, and lt_pred_stat(N/2 ... N-1)
     * is always all zeros. The rest of lt_pred_stat (i<0) contains the previous
     * fully reconstructed time domain samples, i.e., output of the decoder.
     *
     * These values are shifted up by N*2 to avoid (i<0)
     *
     * For the LD object type an extra 512 samples lookback is accomodated here.
     */
#ifdef LD_DEC
    if (object_type == LD)
    {
        for (i = 0; i < frame_len; i++)
        {
            lt_pred_stat[i]  /* extra 512 */  = lt_pred_stat[i + frame_len];
            lt_pred_stat[frame_len + i]       = lt_pred_stat[i + (frame_len * 2)];
            lt_pred_stat[(frame_len * 2) + i] = real_to_int16(time[i]);
            lt_pred_stat[(frame_len * 3) + i] = real_to_int16(overlap[i]);
        }
    } else {
#endif
        for (i = 0; i < frame_len; i++)
        {
            lt_pred_stat[i]                   = lt_pred_stat[i + frame_len];
            lt_pred_stat[frame_len + i]       = real_to_int16(time[i]);
            lt_pred_stat[(frame_len * 2) + i] = real_to_int16(overlap[i]);
#if 0 /* set to zero once upon initialisation */
            lt_pred_stat[(frame_len * 3) + i] = 0;
#endif
        }
#ifdef LD_DEC
    }
#endif
}

#endif
