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
** $Id: tns.c,v 1.35 2004/09/04 14:56:29 menno Exp $
**/

#include "common.h"
#include "structs.h"

#include "syntax.h"
#include "tns.h"


/* static function declarations */
static void tns_decode_coef(uint8_t order, uint8_t coef_res_bits, uint8_t coef_compress,
                            uint8_t *coef, real_t *a);
static void tns_ar_filter(real_t *spectrum, uint16_t size, int8_t inc, real_t *lpc,
                          uint8_t order);
static void tns_ma_filter(real_t *spectrum, uint16_t size, int8_t inc, real_t *lpc,
                          uint8_t order);


#ifdef _MSC_VER
#pragma warning(disable:4305)
#pragma warning(disable:4244)
#endif
static real_t tns_coef_0_3[] =
{
    COEF_CONST(0.0), COEF_CONST(0.4338837391), COEF_CONST(0.7818314825), COEF_CONST(0.9749279122),
    COEF_CONST(-0.9848077530), COEF_CONST(-0.8660254038), COEF_CONST(-0.6427876097), COEF_CONST(-0.3420201433),
    COEF_CONST(-0.4338837391), COEF_CONST(-0.7818314825), COEF_CONST(-0.9749279122), COEF_CONST(-0.9749279122),
    COEF_CONST(-0.9848077530), COEF_CONST(-0.8660254038), COEF_CONST(-0.6427876097), COEF_CONST(-0.3420201433)
};
static real_t tns_coef_0_4[] =
{
    COEF_CONST(0.0), COEF_CONST(0.2079116908), COEF_CONST(0.4067366431), COEF_CONST(0.5877852523),
    COEF_CONST(0.7431448255), COEF_CONST(0.8660254038), COEF_CONST(0.9510565163), COEF_CONST(0.9945218954),
    COEF_CONST(-0.9957341763), COEF_CONST(-0.9618256432), COEF_CONST(-0.8951632914), COEF_CONST(-0.7980172273),
    COEF_CONST(-0.6736956436), COEF_CONST(-0.5264321629), COEF_CONST(-0.3612416662), COEF_CONST(-0.1837495178)
};
static real_t tns_coef_1_3[] =
{
    COEF_CONST(0.0), COEF_CONST(0.4338837391), COEF_CONST(-0.6427876097), COEF_CONST(-0.3420201433),
    COEF_CONST(0.9749279122), COEF_CONST(0.7818314825), COEF_CONST(-0.6427876097), COEF_CONST(-0.3420201433),
    COEF_CONST(-0.4338837391), COEF_CONST(-0.7818314825), COEF_CONST(-0.6427876097), COEF_CONST(-0.3420201433),
    COEF_CONST(-0.7818314825), COEF_CONST(-0.4338837391), COEF_CONST(-0.6427876097), COEF_CONST(-0.3420201433)
};
static real_t tns_coef_1_4[] =
{
    COEF_CONST(0.0), COEF_CONST(0.2079116908), COEF_CONST(0.4067366431), COEF_CONST(0.5877852523),
    COEF_CONST(-0.6736956436), COEF_CONST(-0.5264321629), COEF_CONST(-0.3612416662), COEF_CONST(-0.1837495178),
    COEF_CONST(0.9945218954), COEF_CONST(0.9510565163), COEF_CONST(0.8660254038), COEF_CONST(0.7431448255),
    COEF_CONST(-0.6736956436), COEF_CONST(-0.5264321629), COEF_CONST(-0.3612416662), COEF_CONST(-0.1837495178)
};


/* TNS decoding for one channel and frame */
void tns_decode_frame(ic_stream *ics, tns_info *tns, uint8_t sr_index,
                      uint8_t object_type, real_t *spec, uint16_t frame_len)
{
    uint8_t w, f, tns_order;
    int8_t inc;
    int16_t size;
    uint16_t bottom, top, start, end;
    uint16_t nshort = frame_len/8;
    real_t lpc[TNS_MAX_ORDER+1];

    if (!ics->tns_data_present)
        return;

    for (w = 0; w < ics->num_windows; w++)
    {
        bottom = ics->num_swb;

        for (f = 0; f < tns->n_filt[w]; f++)
        {
            top = bottom;
            bottom = max(top - tns->length[w][f], 0);
            tns_order = min(tns->order[w][f], TNS_MAX_ORDER);
            if (!tns_order)
                continue;

            tns_decode_coef(tns_order, tns->coef_res[w]+3,
                tns->coef_compress[w][f], tns->coef[w][f], lpc);

            start = min(bottom, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            start = min(start, ics->max_sfb);
            start = ics->swb_offset[start];

            end = min(top, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            end = min(end, ics->max_sfb);
            end = ics->swb_offset[end];

            size = end - start;
            if (size <= 0)
                continue;

            if (tns->direction[w][f])
            {
                inc = -1;
                start = end - 1;
            } else {
                inc = 1;
            }

            tns_ar_filter(&spec[(w*nshort)+start], size, inc, lpc, tns_order);
        }
    }
}

/* TNS encoding for one channel and frame */
void tns_encode_frame(ic_stream *ics, tns_info *tns, uint8_t sr_index,
                      uint8_t object_type, real_t *spec, uint16_t frame_len)
{
    uint8_t w, f, tns_order;
    int8_t inc;
    int16_t size;
    uint16_t bottom, top, start, end;
    uint16_t nshort = frame_len/8;
    real_t lpc[TNS_MAX_ORDER+1];

    if (!ics->tns_data_present)
        return;

    for (w = 0; w < ics->num_windows; w++)
    {
        bottom = ics->num_swb;

        for (f = 0; f < tns->n_filt[w]; f++)
        {
            top = bottom;
            bottom = max(top - tns->length[w][f], 0);
            tns_order = min(tns->order[w][f], TNS_MAX_ORDER);
            if (!tns_order)
                continue;

            tns_decode_coef(tns_order, tns->coef_res[w]+3,
                tns->coef_compress[w][f], tns->coef[w][f], lpc);

            start = min(bottom, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            start = min(start, ics->max_sfb);
            start = ics->swb_offset[start];

            end = min(top, max_tns_sfb(sr_index, object_type, (ics->window_sequence == EIGHT_SHORT_SEQUENCE)));
            end = min(end, ics->max_sfb);
            end = ics->swb_offset[end];

            size = end - start;
            if (size <= 0)
                continue;

            if (tns->direction[w][f])
            {
                inc = -1;
                start = end - 1;
            } else {
                inc = 1;
            }

            tns_ma_filter(&spec[(w*nshort)+start], size, inc, lpc, tns_order);
        }
    }
}

/* Decoder transmitted coefficients for one TNS filter */
static void tns_decode_coef(uint8_t order, uint8_t coef_res_bits, uint8_t coef_compress,
                            uint8_t *coef, real_t *a)
{
    uint8_t i, m;
    real_t tmp2[TNS_MAX_ORDER+1], b[TNS_MAX_ORDER+1];

    /* Conversion to signed integer */
    for (i = 0; i < order; i++)
    {
        if (coef_compress == 0)
        {
            if (coef_res_bits == 3)
            {
                tmp2[i] = tns_coef_0_3[coef[i]];
            } else {
                tmp2[i] = tns_coef_0_4[coef[i]];
            }
        } else {
            if (coef_res_bits == 3)
            {
                tmp2[i] = tns_coef_1_3[coef[i]];
            } else {
                tmp2[i] = tns_coef_1_4[coef[i]];
            }
        }
    }

    /* Conversion to LPC coefficients */
    a[0] = COEF_CONST(1.0);
    for (m = 1; m <= order; m++)
    {
        for (i = 1; i < m; i++) /* loop only while i<m */
            b[i] = a[i] + MUL_C(tmp2[m-1], a[m-i]);

        for (i = 1; i < m; i++) /* loop only while i<m */
            a[i] = b[i];

        a[m] = tmp2[m-1]; /* changed */
    }
}

static void tns_ar_filter(real_t *spectrum, uint16_t size, int8_t inc, real_t *lpc,
                          uint8_t order)
{
    /*
     - Simple all-pole filter of order "order" defined by
       y(n) = x(n) - lpc[1]*y(n-1) - ... - lpc[order]*y(n-order)
     - The state variables of the filter are initialized to zero every time
     - The output data is written over the input data ("in-place operation")
     - An input vector of "size" samples is processed and the index increment
       to the next data sample is given by "inc"
    */

    uint8_t j;
    uint16_t i;
    real_t y;
    /* state is stored as a double ringbuffer */
    real_t state[2*TNS_MAX_ORDER] = {0};
    int8_t state_index = 0;

    for (i = 0; i < size; i++)
    {
        y = *spectrum;

        for (j = 0; j < order; j++)
            y -= MUL_C(state[state_index+j], lpc[j+1]);

        /* double ringbuffer state */
        state_index--;
        if (state_index < 0)
            state_index = order-1;
        state[state_index] = state[state_index + order] = y;

        *spectrum = y;
        spectrum += inc;

//#define TNS_PRINT
#ifdef TNS_PRINT
        //printf("%d\n", y);
        printf("0x%.8X\n", y);
#endif
    }
}

static void tns_ma_filter(real_t *spectrum, uint16_t size, int8_t inc, real_t *lpc,
                          uint8_t order)
{
    /*
     - Simple all-zero filter of order "order" defined by
       y(n) =  x(n) + a(2)*x(n-1) + ... + a(order+1)*x(n-order)
     - The state variables of the filter are initialized to zero every time
     - The output data is written over the input data ("in-place operation")
     - An input vector of "size" samples is processed and the index increment
       to the next data sample is given by "inc"
    */

    uint8_t j;
    uint16_t i;
    real_t y;
    /* state is stored as a double ringbuffer */
    real_t state[2*TNS_MAX_ORDER] = {0};
    int8_t state_index = 0;

    for (i = 0; i < size; i++)
    {
        y = *spectrum;

        for (j = 0; j < order; j++)
            y += MUL_C(state[j], lpc[j+1]);

        /* double ringbuffer state */
        state_index--;
        if (state_index < 0)
            state_index = order-1;
        state[state_index] = state[state_index + order] = *spectrum;

        *spectrum = y;
        spectrum += inc;
    }
}
