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
** Initially modified for use with MPlayer by Arpad Gereöffy on 2003/08/30
** $Id: specrec.c,v 1.3 2004/06/02 22:59:03 diego Exp $
** detailed CVS changelog at http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
**/

/*
  Spectral reconstruction:
   - grouping/sectioning
   - inverse quantization
   - applying scalefactors
*/

#include "common.h"
#include "structs.h"

#include <string.h>
#include <stdlib.h>
#include "specrec.h"
#include "syntax.h"
#include "iq_table.h"
#include "ms.h"
#include "is.h"
#include "pns.h"
#include "tns.h"
#include "drc.h"
#include "lt_predict.h"
#include "ic_predict.h"
#ifdef SSR_DEC
#include "ssr.h"
#include "ssr_fb.h"
#endif


/* static function declarations */
static void quant_to_spec(ic_stream *ics, real_t *spec_data, uint16_t frame_len);
static uint8_t inverse_quantization(real_t *x_invquant, const int16_t *x_quant, const uint16_t frame_len);


#ifdef LD_DEC
ALIGN static const uint8_t num_swb_512_window[] =
{
    0, 0, 0, 36, 36, 37, 31, 31, 0, 0, 0, 0
};
ALIGN static const uint8_t num_swb_480_window[] =
{
    0, 0, 0, 35, 35, 37, 30, 30, 0, 0, 0, 0
};
#endif

ALIGN static const uint8_t num_swb_960_window[] =
{
    40, 40, 45, 49, 49, 49, 46, 46, 42, 42, 42, 40
};

ALIGN static const uint8_t num_swb_1024_window[] =
{
    41, 41, 47, 49, 49, 51, 47, 47, 43, 43, 43, 40
};

ALIGN static const uint8_t num_swb_128_window[] =
{
    12, 12, 12, 14, 14, 14, 15, 15, 15, 15, 15, 15
};

ALIGN static const uint16_t swb_offset_1024_96[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56,
    64, 72, 80, 88, 96, 108, 120, 132, 144, 156, 172, 188, 212, 240,
    276, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960, 1024
};

ALIGN static const uint16_t swb_offset_128_96[] =
{
    0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128
};

ALIGN static const uint16_t swb_offset_1024_64[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56,
    64, 72, 80, 88, 100, 112, 124, 140, 156, 172, 192, 216, 240, 268,
    304, 344, 384, 424, 464, 504, 544, 584, 624, 664, 704, 744, 784, 824,
    864, 904, 944, 984, 1024
};

ALIGN static const uint16_t swb_offset_128_64[] =
{
    0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128
};

ALIGN static const uint16_t swb_offset_1024_48[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72,
    80, 88, 96, 108, 120, 132, 144, 160, 176, 196, 216, 240, 264, 292,
    320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704, 736,
    768, 800, 832, 864, 896, 928, 1024
};

#ifdef LD_DEC
ALIGN static const uint16_t swb_offset_512_48[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 68, 76, 84,
    92, 100, 112, 124, 136, 148, 164, 184, 208, 236, 268, 300, 332, 364, 396,
    428, 460, 512
};

ALIGN static const uint16_t swb_offset_480_48[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72 ,80 ,88,
    96, 108, 120, 132, 144, 156, 172, 188, 212, 240, 272, 304, 336, 368, 400,
    432, 480
};
#endif

ALIGN static const uint16_t swb_offset_128_48[] =
{
    0, 4, 8, 12, 16, 20, 28, 36, 44, 56, 68, 80, 96, 112, 128
};

ALIGN static const uint16_t swb_offset_1024_32[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72,
    80, 88, 96, 108, 120, 132, 144, 160, 176, 196, 216, 240, 264, 292,
    320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704, 736,
    768, 800, 832, 864, 896, 928, 960, 992, 1024
};

#ifdef LD_DEC
ALIGN static const uint16_t swb_offset_512_32[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80,
    88, 96, 108, 120, 132, 144, 160, 176, 192, 212, 236, 260, 288, 320, 352,
    384, 416, 448, 480, 512
};

ALIGN static const uint16_t swb_offset_480_32[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 72, 80,
    88, 96, 104, 112, 124, 136, 148, 164, 180, 200, 224, 256, 288, 320, 352,
    384, 416, 448, 480
};
#endif

ALIGN static const uint16_t swb_offset_1024_24[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 52, 60, 68,
    76, 84, 92, 100, 108, 116, 124, 136, 148, 160, 172, 188, 204, 220,
    240, 260, 284, 308, 336, 364, 396, 432, 468, 508, 552, 600, 652, 704,
    768, 832, 896, 960, 1024
};

#ifdef LD_DEC
ALIGN static const uint16_t swb_offset_512_24[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 52, 60, 68,
    80, 92, 104, 120, 140, 164, 192, 224, 256, 288, 320, 352, 384, 416,
    448, 480, 512
};

ALIGN static const uint16_t swb_offset_480_24[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 52, 60, 68, 80, 92, 104, 120,
    140, 164, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480
};
#endif

ALIGN static const uint16_t swb_offset_128_24[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 64, 76, 92, 108, 128
};

ALIGN static const uint16_t swb_offset_1024_16[] =
{
    0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 100, 112, 124,
    136, 148, 160, 172, 184, 196, 212, 228, 244, 260, 280, 300, 320, 344,
    368, 396, 424, 456, 492, 532, 572, 616, 664, 716, 772, 832, 896, 960, 1024
};

ALIGN static const uint16_t swb_offset_128_16[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 60, 72, 88, 108, 128
};

ALIGN static const uint16_t swb_offset_1024_8[] =
{
    0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 172,
    188, 204, 220, 236, 252, 268, 288, 308, 328, 348, 372, 396, 420, 448,
    476, 508, 544, 580, 620, 664, 712, 764, 820, 880, 944, 1024
};

ALIGN static const uint16_t swb_offset_128_8[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 60, 72, 88, 108, 128
};

ALIGN static const uint16_t *swb_offset_1024_window[] =
{
    swb_offset_1024_96,      /* 96000 */
    swb_offset_1024_96,      /* 88200 */
    swb_offset_1024_64,      /* 64000 */
    swb_offset_1024_48,      /* 48000 */
    swb_offset_1024_48,      /* 44100 */
    swb_offset_1024_32,      /* 32000 */
    swb_offset_1024_24,      /* 24000 */
    swb_offset_1024_24,      /* 22050 */
    swb_offset_1024_16,      /* 16000 */
    swb_offset_1024_16,      /* 12000 */
    swb_offset_1024_16,      /* 11025 */
    swb_offset_1024_8        /* 8000  */
};

#ifdef LD_DEC
ALIGN static const uint16_t *swb_offset_512_window[] =
{
    0,                       /* 96000 */
    0,                       /* 88200 */
    0,                       /* 64000 */
    swb_offset_512_48,       /* 48000 */
    swb_offset_512_48,       /* 44100 */
    swb_offset_512_32,       /* 32000 */
    swb_offset_512_24,       /* 24000 */
    swb_offset_512_24,       /* 22050 */
    0,                       /* 16000 */
    0,                       /* 12000 */
    0,                       /* 11025 */
    0                        /* 8000  */
};

ALIGN static const uint16_t *swb_offset_480_window[] =
{
    0,                       /* 96000 */
    0,                       /* 88200 */
    0,                       /* 64000 */
    swb_offset_480_48,       /* 48000 */
    swb_offset_480_48,       /* 44100 */
    swb_offset_480_32,       /* 32000 */
    swb_offset_480_24,       /* 24000 */
    swb_offset_480_24,       /* 22050 */
    0,                       /* 16000 */
    0,                       /* 12000 */
    0,                       /* 11025 */
    0                        /* 8000  */
};
#endif

ALIGN static const  uint16_t *swb_offset_128_window[] =
{
    swb_offset_128_96,       /* 96000 */
    swb_offset_128_96,       /* 88200 */
    swb_offset_128_64,       /* 64000 */
    swb_offset_128_48,       /* 48000 */
    swb_offset_128_48,       /* 44100 */
    swb_offset_128_48,       /* 32000 */
    swb_offset_128_24,       /* 24000 */
    swb_offset_128_24,       /* 22050 */
    swb_offset_128_16,       /* 16000 */
    swb_offset_128_16,       /* 12000 */
    swb_offset_128_16,       /* 11025 */
    swb_offset_128_8         /* 8000  */
};

#define bit_set(A, B) ((A) & (1<<(B)))

/* 4.5.2.3.4 */
/*
  - determine the number of windows in a window_sequence named num_windows
  - determine the number of window_groups named num_window_groups
  - determine the number of windows in each group named window_group_length[g]
  - determine the total number of scalefactor window bands named num_swb for
    the actual window type
  - determine swb_offset[swb], the offset of the first coefficient in
    scalefactor window band named swb of the window actually used
  - determine sect_sfb_offset[g][section],the offset of the first coefficient
    in section named section. This offset depends on window_sequence and
    scale_factor_grouping and is needed to decode the spectral_data().
*/
uint8_t window_grouping_info(faacDecHandle hDecoder, ic_stream *ics)
{
    uint8_t i, g;

    uint8_t sf_index = hDecoder->sf_index;

    switch (ics->window_sequence) {
    case ONLY_LONG_SEQUENCE:
    case LONG_START_SEQUENCE:
    case LONG_STOP_SEQUENCE:
        ics->num_windows = 1;
        ics->num_window_groups = 1;
        ics->window_group_length[ics->num_window_groups-1] = 1;
#ifdef LD_DEC
        if (hDecoder->object_type == LD)
        {
            if (hDecoder->frameLength == 512)
                ics->num_swb = num_swb_512_window[sf_index];
            else /* if (hDecoder->frameLength == 480) */
                ics->num_swb = num_swb_480_window[sf_index];
        } else {
#endif
            if (hDecoder->frameLength == 1024)
                ics->num_swb = num_swb_1024_window[sf_index];
            else /* if (hDecoder->frameLength == 960) */
                ics->num_swb = num_swb_960_window[sf_index];
#ifdef LD_DEC
        }
#endif

        /* preparation of sect_sfb_offset for long blocks */
        /* also copy the last value! */
#ifdef LD_DEC
        if (hDecoder->object_type == LD)
        {
            if (hDecoder->frameLength == 512)
            {
                for (i = 0; i < ics->num_swb; i++)
                {
                    ics->sect_sfb_offset[0][i] = swb_offset_512_window[sf_index][i];
                    ics->swb_offset[i] = swb_offset_512_window[sf_index][i];
                }
            } else /* if (hDecoder->frameLength == 480) */ {
                for (i = 0; i < ics->num_swb; i++)
                {
                    ics->sect_sfb_offset[0][i] = swb_offset_480_window[sf_index][i];
                    ics->swb_offset[i] = swb_offset_480_window[sf_index][i];
                }
            }
            ics->sect_sfb_offset[0][ics->num_swb] = hDecoder->frameLength;
            ics->swb_offset[ics->num_swb] = hDecoder->frameLength;
        } else {
#endif
            for (i = 0; i < ics->num_swb; i++)
            {
                ics->sect_sfb_offset[0][i] = swb_offset_1024_window[sf_index][i];
                ics->swb_offset[i] = swb_offset_1024_window[sf_index][i];
            }
            ics->sect_sfb_offset[0][ics->num_swb] = hDecoder->frameLength;
            ics->swb_offset[ics->num_swb] = hDecoder->frameLength;
#ifdef LD_DEC
        }
#endif
        return 0;
    case EIGHT_SHORT_SEQUENCE:
        ics->num_windows = 8;
        ics->num_window_groups = 1;
        ics->window_group_length[ics->num_window_groups-1] = 1;
        ics->num_swb = num_swb_128_window[sf_index];

        for (i = 0; i < ics->num_swb; i++)
            ics->swb_offset[i] = swb_offset_128_window[sf_index][i];
        ics->swb_offset[ics->num_swb] = hDecoder->frameLength/8;

        for (i = 0; i < ics->num_windows-1; i++) {
            if (bit_set(ics->scale_factor_grouping, 6-i) == 0)
            {
                ics->num_window_groups += 1;
                ics->window_group_length[ics->num_window_groups-1] = 1;
            } else {
                ics->window_group_length[ics->num_window_groups-1] += 1;
            }
        }

        /* preparation of sect_sfb_offset for short blocks */
        for (g = 0; g < ics->num_window_groups; g++)
        {
            uint16_t width;
            uint8_t sect_sfb = 0;
            uint16_t offset = 0;

            for (i = 0; i < ics->num_swb; i++)
            {
                if (i+1 == ics->num_swb)
                {
                    width = (hDecoder->frameLength/8) - swb_offset_128_window[sf_index][i];
                } else {
                    width = swb_offset_128_window[sf_index][i+1] -
                        swb_offset_128_window[sf_index][i];
                }
                width *= ics->window_group_length[g];
                ics->sect_sfb_offset[g][sect_sfb++] = offset;
                offset += width;
            }
            ics->sect_sfb_offset[g][sect_sfb] = offset;
        }
        return 0;
    default:
        return 1;
    }
}

/*
  For ONLY_LONG_SEQUENCE windows (num_window_groups = 1,
  window_group_length[0] = 1) the spectral data is in ascending spectral
  order.
  For the EIGHT_SHORT_SEQUENCE window, the spectral order depends on the
  grouping in the following manner:
  - Groups are ordered sequentially
  - Within a group, a scalefactor band consists of the spectral data of all
    grouped SHORT_WINDOWs for the associated scalefactor window band. To
    clarify via example, the length of a group is in the range of one to eight
    SHORT_WINDOWs.
  - If there are eight groups each with length one (num_window_groups = 8,
    window_group_length[0..7] = 1), the result is a sequence of eight spectra,
    each in ascending spectral order.
  - If there is only one group with length eight (num_window_groups = 1,
    window_group_length[0] = 8), the result is that spectral data of all eight
    SHORT_WINDOWs is interleaved by scalefactor window bands.
  - Within a scalefactor window band, the coefficients are in ascending
    spectral order.
*/
static void quant_to_spec(ic_stream *ics, real_t *spec_data, uint16_t frame_len)
{
    uint8_t g, sfb, win;
    uint16_t width, bin, k, gindex;

    ALIGN real_t tmp_spec[1024] = {0};

    k = 0;
    gindex = 0;

    for (g = 0; g < ics->num_window_groups; g++)
    {
        uint16_t j = 0;
        uint16_t gincrease = 0;
        uint16_t win_inc = ics->swb_offset[ics->num_swb];

        for (sfb = 0; sfb < ics->num_swb; sfb++)
        {
            width = ics->swb_offset[sfb+1] - ics->swb_offset[sfb];

            for (win = 0; win < ics->window_group_length[g]; win++)
            {
                for (bin = 0; bin < width; bin += 4)
                {
                    tmp_spec[gindex+(win*win_inc)+j+bin+0] = spec_data[k+0];
                    tmp_spec[gindex+(win*win_inc)+j+bin+1] = spec_data[k+1];
                    tmp_spec[gindex+(win*win_inc)+j+bin+2] = spec_data[k+2];
                    tmp_spec[gindex+(win*win_inc)+j+bin+3] = spec_data[k+3];
                    gincrease += 4;
                    k += 4;
                }
            }
            j += width;
        }
        gindex += gincrease;
    }

    memcpy(spec_data, tmp_spec, frame_len*sizeof(real_t));
}

static INLINE real_t iquant(int16_t q, const real_t *tab, uint8_t *error)
{
#ifdef FIXED_POINT
    static const real_t errcorr[] = {
        REAL_CONST(0), REAL_CONST(1.0/8.0), REAL_CONST(2.0/8.0), REAL_CONST(3.0/8.0),
        REAL_CONST(4.0/8.0),  REAL_CONST(5.0/8.0), REAL_CONST(6.0/8.0), REAL_CONST(7.0/8.0),
        REAL_CONST(0)
    };
    real_t x1, x2;
    int16_t sgn = 1;

    if (q < 0)
    {
        q = -q;
        sgn = -1;
    }

    if (q < IQ_TABLE_SIZE)
        return sgn * tab[q];

    /* linear interpolation */
    x1 = tab[q>>3];
    x2 = tab[(q>>3) + 1];
    return sgn * 16 * (MUL_R(errcorr[q&7],(x2-x1)) + x1);
#else
    if (q < 0)
    {
        /* tab contains a value for all possible q [0,8192] */
        if (-q < IQ_TABLE_SIZE)
            return -tab[-q];

        *error = 17;
        return 0;
    } else {
        /* tab contains a value for all possible q [0,8192] */
        if (q < IQ_TABLE_SIZE)
            return tab[q];

        *error = 17;
        return 0;
    }
#endif
}

static uint8_t inverse_quantization(real_t *x_invquant, const int16_t *x_quant, const uint16_t frame_len)
{
    int16_t i;
    uint8_t error = 0; /* Init error flag */
    const real_t *tab = iq_table;

    for (i = 0; i < frame_len; i+=4)
    {
        x_invquant[i] = iquant(x_quant[i], tab, &error);
        x_invquant[i+1] = iquant(x_quant[i+1], tab, &error);
        x_invquant[i+2] = iquant(x_quant[i+2], tab, &error);
        x_invquant[i+3] = iquant(x_quant[i+3], tab, &error);
    }

    return error;
}

#ifndef FIXED_POINT
ALIGN static const real_t pow2sf_tab[] = {
    2.9802322387695313E-008, 5.9604644775390625E-008, 1.1920928955078125E-007,
    2.384185791015625E-007, 4.76837158203125E-007, 9.5367431640625E-007,
    1.9073486328125E-006, 3.814697265625E-006, 7.62939453125E-006,
    1.52587890625E-005, 3.0517578125E-005, 6.103515625E-005,
    0.0001220703125, 0.000244140625, 0.00048828125,
    0.0009765625, 0.001953125, 0.00390625,
    0.0078125, 0.015625, 0.03125,
    0.0625, 0.125, 0.25,
    0.5, 1.0, 2.0,
    4.0, 8.0, 16.0, 32.0,
    64.0, 128.0, 256.0,
    512.0, 1024.0, 2048.0,
    4096.0, 8192.0, 16384.0,
    32768.0, 65536.0, 131072.0,
    262144.0, 524288.0, 1048576.0,
    2097152.0, 4194304.0, 8388608.0,
    16777216.0, 33554432.0, 67108864.0,
    134217728.0, 268435456.0, 536870912.0,
    1073741824.0, 2147483648.0, 4294967296.0,
    8589934592.0, 17179869184.0, 34359738368.0,
    68719476736.0, 137438953472.0, 274877906944.0
};
#endif

ALIGN static real_t pow2_table[] =
{
#if 0
    COEF_CONST(0.59460355750136053335874998528024), /* 2^-0.75 */
    COEF_CONST(0.70710678118654752440084436210485), /* 2^-0.5 */
    COEF_CONST(0.84089641525371454303112547623321), /* 2^-0.25 */
#endif
    COEF_CONST(1.0),
    COEF_CONST(1.1892071150027210667174999705605), /* 2^0.25 */
    COEF_CONST(1.4142135623730950488016887242097), /* 2^0.5 */
    COEF_CONST(1.6817928305074290860622509524664) /* 2^0.75 */
};

void apply_scalefactors(faacDecHandle hDecoder, ic_stream *ics,
                        real_t *x_invquant, uint16_t frame_len)
{
    uint8_t g, sfb;
    uint16_t top;
    int32_t exp, frac;
    uint8_t groups = 0;
    uint16_t nshort = frame_len/8;

    for (g = 0; g < ics->num_window_groups; g++)
    {
        uint16_t k = 0;

        /* using this nshort*groups doesn't hurt long blocks, because
           long blocks only have 1 group, so that means 'groups' is
           always 0 for long blocks
        */
        for (sfb = 0; sfb < ics->max_sfb; sfb++)
        {
            top = ics->sect_sfb_offset[g][sfb+1];

            /* this could be scalefactor for IS or PNS, those can be negative or bigger then 255 */
            /* just ignore them */
            if (ics->scale_factors[g][sfb] < 0 || ics->scale_factors[g][sfb] > 255)
            {
                exp = 0;
                frac = 0;
            } else {
                /* ics->scale_factors[g][sfb] must be between 0 and 255 */
                exp = (ics->scale_factors[g][sfb] /* - 100 */) >> 2;
                frac = (ics->scale_factors[g][sfb] /* - 100 */) & 3;
            }

#ifdef FIXED_POINT
            exp -= 25;
            /* IMDCT pre-scaling */
            if (hDecoder->object_type == LD)
            {
                exp -= 6 /*9*/;
            } else {
                if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
                    exp -= 4 /*7*/;
                else
                    exp -= 7 /*10*/;
            }
#endif

            /* minimum size of a sf band is 4 and always a multiple of 4 */
            for ( ; k < top; k += 4)
            {
#ifdef FIXED_POINT
                if (exp < 0)
                {
                    x_invquant[k+(groups*nshort)] >>= -exp;
                    x_invquant[k+(groups*nshort)+1] >>= -exp;
                    x_invquant[k+(groups*nshort)+2] >>= -exp;
                    x_invquant[k+(groups*nshort)+3] >>= -exp;
                } else {
                    x_invquant[k+(groups*nshort)] <<= exp;
                    x_invquant[k+(groups*nshort)+1] <<= exp;
                    x_invquant[k+(groups*nshort)+2] <<= exp;
                    x_invquant[k+(groups*nshort)+3] <<= exp;
                }
#else
                x_invquant[k+(groups*nshort)]   = x_invquant[k+(groups*nshort)]   * pow2sf_tab[exp/*+25*/];
                x_invquant[k+(groups*nshort)+1] = x_invquant[k+(groups*nshort)+1] * pow2sf_tab[exp/*+25*/];
                x_invquant[k+(groups*nshort)+2] = x_invquant[k+(groups*nshort)+2] * pow2sf_tab[exp/*+25*/];
                x_invquant[k+(groups*nshort)+3] = x_invquant[k+(groups*nshort)+3] * pow2sf_tab[exp/*+25*/];
#endif

                x_invquant[k+(groups*nshort)]   = MUL_C(x_invquant[k+(groups*nshort)],pow2_table[frac /* + 3*/]);
                x_invquant[k+(groups*nshort)+1] = MUL_C(x_invquant[k+(groups*nshort)+1],pow2_table[frac /* + 3*/]);
                x_invquant[k+(groups*nshort)+2] = MUL_C(x_invquant[k+(groups*nshort)+2],pow2_table[frac /* + 3*/]);
                x_invquant[k+(groups*nshort)+3] = MUL_C(x_invquant[k+(groups*nshort)+3],pow2_table[frac /* + 3*/]);
            }
        }
        groups += ics->window_group_length[g];
    }
}

#ifdef USE_SSE
void apply_scalefactors_sse(faacDecHandle hDecoder, ic_stream *ics,
                            real_t *x_invquant, uint16_t frame_len)
{
    uint8_t g, sfb;
    uint16_t top;
    int32_t exp, frac;
    uint8_t groups = 0;
    uint16_t nshort = frame_len/8;

    for (g = 0; g < ics->num_window_groups; g++)
    {
        uint16_t k = 0;

        /* using this nshort*groups doesn't hurt long blocks, because
           long blocks only have 1 group, so that means 'groups' is
           always 0 for long blocks
        */
        for (sfb = 0; sfb < ics->max_sfb; sfb++)
        {
            top = ics->sect_sfb_offset[g][sfb+1];

            exp = (ics->scale_factors[g][sfb] /* - 100 */) >> 2;
            frac = (ics->scale_factors[g][sfb] /* - 100 */) & 3;

            /* minimum size of a sf band is 4 and always a multiple of 4 */
            for ( ; k < top; k += 4)
            {
                __m128 m1 = _mm_load_ps(&x_invquant[k+(groups*nshort)]);
                __m128 m2 = _mm_load_ps1(&pow2sf_tab[exp /*+25*/]);
                __m128 m3 = _mm_load_ps1(&pow2_table[frac /* + 3*/]);
                __m128 m4 = _mm_mul_ps(m1, m2);
                __m128 m5 = _mm_mul_ps(m3, m4);
                _mm_store_ps(&x_invquant[k+(groups*nshort)], m5);
            }
        }
        groups += ics->window_group_length[g];
    }
}
#endif

static uint8_t allocate_single_channel(faacDecHandle hDecoder, uint8_t channel,
                                       uint8_t output_channels)
{
    uint8_t mul = 1;

#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN)
    {
        /* allocate the state only when needed */
        if (hDecoder->pred_stat[channel] == NULL)
        {
            hDecoder->pred_stat[channel] = (pred_state*)faad_malloc(hDecoder->frameLength * sizeof(pred_state));
            reset_all_predictors(hDecoder->pred_stat[channel], hDecoder->frameLength);
        }
    }
#endif

#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type))
    {
        /* allocate the state only when needed */
        if (hDecoder->lt_pred_stat[channel] == NULL)
        {
            hDecoder->lt_pred_stat[channel] = (int16_t*)faad_malloc(hDecoder->frameLength*4 * sizeof(int16_t));
            memset(hDecoder->lt_pred_stat[channel], 0, hDecoder->frameLength*4 * sizeof(int16_t));
        }
    }
#endif

    if (hDecoder->time_out[channel] == NULL)
    {
        mul = 1;
#ifdef SBR_DEC
        hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 0;
        if ((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1))
        {
            /* SBR requires 2 times as much output data */
            mul = 2;
            hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 1;
        }
#endif
        hDecoder->time_out[channel] = (real_t*)faad_malloc(mul*hDecoder->frameLength*sizeof(real_t));
        memset(hDecoder->time_out[channel], 0, mul*hDecoder->frameLength*sizeof(real_t));
    }
#if (defined(PS_DEC) || defined(DRM_PS))
    if (output_channels == 2)
    {
        if (hDecoder->time_out[channel+1] == NULL)
        {
            hDecoder->time_out[channel+1] = (real_t*)faad_malloc(mul*hDecoder->frameLength*sizeof(real_t));
            memset(hDecoder->time_out[channel+1], 0, mul*hDecoder->frameLength*sizeof(real_t));
        }
    }
#endif

    if (hDecoder->fb_intermed[channel] == NULL)
    {
        hDecoder->fb_intermed[channel] = (real_t*)faad_malloc(hDecoder->frameLength*sizeof(real_t));
        memset(hDecoder->fb_intermed[channel], 0, hDecoder->frameLength*sizeof(real_t));
    }

#ifdef SSR_DEC
    if (hDecoder->object_type == SSR)
    {
        if (hDecoder->ssr_overlap[channel] == NULL)
        {
            hDecoder->ssr_overlap[channel] = (real_t*)faad_malloc(2*hDecoder->frameLength*sizeof(real_t));
            memset(hDecoder->ssr_overlap[channel], 0, 2*hDecoder->frameLength*sizeof(real_t));
        }
        if (hDecoder->prev_fmd[channel] == NULL)
        {
            uint16_t k;
            hDecoder->prev_fmd[channel] = (real_t*)faad_malloc(2*hDecoder->frameLength*sizeof(real_t));
            for (k = 0; k < 2*hDecoder->frameLength; k++)
                hDecoder->prev_fmd[channel][k] = REAL_CONST(-1);
        }
    }
#endif

    return 0;
}

static uint8_t allocate_channel_pair(faacDecHandle hDecoder,
                                     uint8_t channel, uint8_t paired_channel)
{
    uint8_t mul = 1;

#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN)
    {
        /* allocate the state only when needed */
        if (hDecoder->pred_stat[channel] == NULL)
        {
            hDecoder->pred_stat[channel] = (pred_state*)faad_malloc(hDecoder->frameLength * sizeof(pred_state));
            reset_all_predictors(hDecoder->pred_stat[channel], hDecoder->frameLength);
        }
        if (hDecoder->pred_stat[paired_channel] == NULL)
        {
            hDecoder->pred_stat[paired_channel] = (pred_state*)faad_malloc(hDecoder->frameLength * sizeof(pred_state));
            reset_all_predictors(hDecoder->pred_stat[paired_channel], hDecoder->frameLength);
        }
    }
#endif

#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type))
    {
        /* allocate the state only when needed */
        if (hDecoder->lt_pred_stat[channel] == NULL)
        {
            hDecoder->lt_pred_stat[channel] = (int16_t*)faad_malloc(hDecoder->frameLength*4 * sizeof(int16_t));
            memset(hDecoder->lt_pred_stat[channel], 0, hDecoder->frameLength*4 * sizeof(int16_t));
        }
        if (hDecoder->lt_pred_stat[paired_channel] == NULL)
        {
            hDecoder->lt_pred_stat[paired_channel] = (int16_t*)faad_malloc(hDecoder->frameLength*4 * sizeof(int16_t));
            memset(hDecoder->lt_pred_stat[paired_channel], 0, hDecoder->frameLength*4 * sizeof(int16_t));
        }
    }
#endif

    if (hDecoder->time_out[channel] == NULL)
    {
        mul = 1;
#ifdef SBR_DEC
        hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 0;
        if ((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1))
        {
            /* SBR requires 2 times as much output data */
            mul = 2;
            hDecoder->sbr_alloced[hDecoder->fr_ch_ele] = 1;
        }
#endif
        hDecoder->time_out[channel] = (real_t*)faad_malloc(mul*hDecoder->frameLength*sizeof(real_t));
        memset(hDecoder->time_out[channel], 0, mul*hDecoder->frameLength*sizeof(real_t));
    }
    if (hDecoder->time_out[paired_channel] == NULL)
    {
        hDecoder->time_out[paired_channel] = (real_t*)faad_malloc(mul*hDecoder->frameLength*sizeof(real_t));
        memset(hDecoder->time_out[paired_channel], 0, mul*hDecoder->frameLength*sizeof(real_t));
    }

    if (hDecoder->fb_intermed[channel] == NULL)
    {
        hDecoder->fb_intermed[channel] = (real_t*)faad_malloc(hDecoder->frameLength*sizeof(real_t));
        memset(hDecoder->fb_intermed[channel], 0, hDecoder->frameLength*sizeof(real_t));
    }
    if (hDecoder->fb_intermed[paired_channel] == NULL)
    {
        hDecoder->fb_intermed[paired_channel] = (real_t*)faad_malloc(hDecoder->frameLength*sizeof(real_t));
        memset(hDecoder->fb_intermed[paired_channel], 0, hDecoder->frameLength*sizeof(real_t));
    }

#ifdef SSR_DEC
    if (hDecoder->object_type == SSR)
    {
        if (hDecoder->ssr_overlap[cpe->channel] == NULL)
        {
            hDecoder->ssr_overlap[cpe->channel] = (real_t*)faad_malloc(2*hDecoder->frameLength*sizeof(real_t));
            memset(hDecoder->ssr_overlap[cpe->channel], 0, 2*hDecoder->frameLength*sizeof(real_t));
        }
        if (hDecoder->ssr_overlap[cpe->paired_channel] == NULL)
        {
            hDecoder->ssr_overlap[cpe->paired_channel] = (real_t*)faad_malloc(2*hDecoder->frameLength*sizeof(real_t));
            memset(hDecoder->ssr_overlap[cpe->paired_channel], 0, 2*hDecoder->frameLength*sizeof(real_t));
        }
        if (hDecoder->prev_fmd[cpe->channel] == NULL)
        {
            uint16_t k;
            hDecoder->prev_fmd[cpe->channel] = (real_t*)faad_malloc(2*hDecoder->frameLength*sizeof(real_t));
            for (k = 0; k < 2*hDecoder->frameLength; k++)
                hDecoder->prev_fmd[cpe->channel][k] = REAL_CONST(-1);
        }
        if (hDecoder->prev_fmd[cpe->paired_channel] == NULL)
        {
            uint16_t k;
            hDecoder->prev_fmd[cpe->paired_channel] = (real_t*)faad_malloc(2*hDecoder->frameLength*sizeof(real_t));
            for (k = 0; k < 2*hDecoder->frameLength; k++)
                hDecoder->prev_fmd[cpe->paired_channel][k] = REAL_CONST(-1);
        }
    }
#endif

    return 0;
}

uint8_t reconstruct_single_channel(faacDecHandle hDecoder, ic_stream *ics,
                                   element *sce, int16_t *spec_data)
{
    uint8_t retval, output_channels;
    ALIGN real_t spec_coef[1024];

#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif


    /* determine whether some mono->stereo tool is used */
#if (defined(PS_DEC) || defined(DRM_PS))
    output_channels = hDecoder->ps_used[hDecoder->fr_ch_ele] ? 2 : 1;
#else
    output_channels = 1;
#endif
    if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 0)
    {
        /* element_output_channels not set yet */
        hDecoder->element_output_channels[hDecoder->fr_ch_ele] = output_channels;
    } else if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] != output_channels) {
        /* element inconsistency */
        return 21;
    }


    if (hDecoder->element_alloced[hDecoder->fr_ch_ele] == 0)
    {
        retval = allocate_single_channel(hDecoder, sce->channel, output_channels);
        if (retval > 0)
            return retval;

        hDecoder->element_alloced[hDecoder->fr_ch_ele] = 1;
    }


    /* inverse quantization */
    retval = inverse_quantization(spec_coef, spec_data, hDecoder->frameLength);
    if (retval > 0)
        return retval;

    /* apply scalefactors */
#ifndef USE_SSE
    apply_scalefactors(hDecoder, ics, spec_coef, hDecoder->frameLength);
#else
    hDecoder->apply_sf_func(hDecoder, ics, spec_coef, hDecoder->frameLength);
#endif

    /* deinterleave short block grouping */
    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
        quant_to_spec(ics, spec_coef, hDecoder->frameLength);

#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->requant_cycles += count;
#endif


    /* pns decoding */
    pns_decode(ics, NULL, spec_coef, NULL, hDecoder->frameLength, 0, hDecoder->object_type);

#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN)
    {
        /* intra channel prediction */
        ic_prediction(ics, spec_coef, hDecoder->pred_stat[sce->channel], hDecoder->frameLength,
            hDecoder->sf_index);

        /* In addition, for scalefactor bands coded by perceptual
           noise substitution the predictors belonging to the
           corresponding spectral coefficients are reset.
        */
        pns_reset_pred_state(ics, hDecoder->pred_stat[sce->channel]);
    }
#endif

#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type))
    {
#ifdef LD_DEC
        if (hDecoder->object_type == LD)
        {
            if (ics->ltp.data_present)
            {
                if (ics->ltp.lag_update)
                    hDecoder->ltp_lag[sce->channel] = ics->ltp.lag;
            }
            ics->ltp.lag = hDecoder->ltp_lag[sce->channel];
        }
#endif

        /* long term prediction */
        lt_prediction(ics, &(ics->ltp), spec_coef, hDecoder->lt_pred_stat[sce->channel], hDecoder->fb,
            ics->window_shape, hDecoder->window_shape_prev[sce->channel],
            hDecoder->sf_index, hDecoder->object_type, hDecoder->frameLength);
    }
#endif

    /* tns decoding */
    tns_decode_frame(ics, &(ics->tns), hDecoder->sf_index, hDecoder->object_type,
        spec_coef, hDecoder->frameLength);

    /* drc decoding */
    if (hDecoder->drc->present)
    {
        if (!hDecoder->drc->exclude_mask[sce->channel] || !hDecoder->drc->excluded_chns_present)
            drc_decode(hDecoder->drc, spec_coef);
    }


    /* filter bank */
#ifdef SSR_DEC
    if (hDecoder->object_type != SSR)
    {
#endif
#ifdef USE_SSE
        hDecoder->fb->if_func(hDecoder->fb, ics->window_sequence, ics->window_shape,
            hDecoder->window_shape_prev[sce->channel], spec_coef,
            hDecoder->time_out[sce->channel], hDecoder->object_type, hDecoder->frameLength);
#else
        ifilter_bank(hDecoder->fb, ics->window_sequence, ics->window_shape,
            hDecoder->window_shape_prev[sce->channel], spec_coef,
            hDecoder->time_out[sce->channel], hDecoder->fb_intermed[sce->channel],
            hDecoder->object_type, hDecoder->frameLength);
#endif
#ifdef SSR_DEC
    } else {
        ssr_decode(&(ics->ssr), hDecoder->fb, ics->window_sequence, ics->window_shape,
            hDecoder->window_shape_prev[sce->channel], spec_coef, hDecoder->time_out[sce->channel],
            hDecoder->ssr_overlap[sce->channel], hDecoder->ipqf_buffer[sce->channel], hDecoder->prev_fmd[sce->channel],
            hDecoder->frameLength);
    }
#endif

    /* save window shape for next frame */
    hDecoder->window_shape_prev[sce->channel] = ics->window_shape;

#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type))
    {
        lt_update_state(hDecoder->lt_pred_stat[sce->channel], hDecoder->time_out[sce->channel],
            hDecoder->fb_intermed[sce->channel], hDecoder->frameLength, hDecoder->object_type);
    }
#endif

#ifdef SBR_DEC
    if (((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1))
        && hDecoder->sbr_alloced[hDecoder->fr_ch_ele])
    {
        uint8_t ele = hDecoder->fr_ch_ele;
        uint8_t ch = sce->channel;

        /* following case can happen when forceUpSampling == 1 */
        if (hDecoder->sbr[ele] == NULL)
        {
            hDecoder->sbr[ele] = sbrDecodeInit(hDecoder->frameLength,
                sce->ele_id, 2*get_sample_rate(hDecoder->sf_index)
#ifdef DRM
                , 0
#endif
                );
        }

        /* check if any of the PS tools is used */
#if (defined(PS_DEC) || defined(DRM_PS))
        if (output_channels == 1)
        {
#endif
            retval = sbrDecodeSingleFrame(hDecoder->sbr[ele], hDecoder->time_out[ch],
                hDecoder->postSeekResetFlag, hDecoder->forceUpSampling);
#if (defined(PS_DEC) || defined(DRM_PS))
        } else {
            retval = sbrDecodeSingleFramePS(hDecoder->sbr[ele], hDecoder->time_out[ch],
                hDecoder->time_out[ch+1], hDecoder->postSeekResetFlag,
                hDecoder->forceUpSampling);
        }
#endif
        if (retval > 0)
            return retval;
    } else if (((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1))
        && !hDecoder->sbr_alloced[hDecoder->fr_ch_ele])
    {
        return 23;
    }
#endif

    return 0;
}

uint8_t reconstruct_channel_pair(faacDecHandle hDecoder, ic_stream *ics1, ic_stream *ics2,
                                 element *cpe, int16_t *spec_data1, int16_t *spec_data2)
{
    uint8_t retval;
    ALIGN real_t spec_coef1[1024];
    ALIGN real_t spec_coef2[1024];

#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif
    if (hDecoder->element_alloced[hDecoder->fr_ch_ele] == 0)
    {
        retval = allocate_channel_pair(hDecoder, cpe->channel, cpe->paired_channel);
        if (retval > 0)
            return retval;

        hDecoder->element_alloced[hDecoder->fr_ch_ele] = 1;
    }

    /* inverse quantization */
    retval = inverse_quantization(spec_coef1, spec_data1, hDecoder->frameLength);
    if (retval > 0)
        return retval;

    retval = inverse_quantization(spec_coef2, spec_data2, hDecoder->frameLength);
    if (retval > 0)
        return retval;

    /* apply scalefactors */
#ifndef USE_SSE
    apply_scalefactors(hDecoder, ics1, spec_coef1, hDecoder->frameLength);
    apply_scalefactors(hDecoder, ics2, spec_coef2, hDecoder->frameLength);
#else
    hDecoder->apply_sf_func(hDecoder, ics1, spec_coef1, hDecoder->frameLength);
    hDecoder->apply_sf_func(hDecoder, ics2, spec_coef2, hDecoder->frameLength);
#endif

    /* deinterleave short block grouping */
    if (ics1->window_sequence == EIGHT_SHORT_SEQUENCE)
        quant_to_spec(ics1, spec_coef1, hDecoder->frameLength);
    if (ics2->window_sequence == EIGHT_SHORT_SEQUENCE)
        quant_to_spec(ics2, spec_coef2, hDecoder->frameLength);

#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->requant_cycles += count;
#endif


    /* pns decoding */
    if (ics1->ms_mask_present)
    {
        pns_decode(ics1, ics2, spec_coef1, spec_coef2, hDecoder->frameLength, 1, hDecoder->object_type);
    } else {
        pns_decode(ics1, NULL, spec_coef1, NULL, hDecoder->frameLength, 0, hDecoder->object_type);
        pns_decode(ics2, NULL, spec_coef2, NULL, hDecoder->frameLength, 0, hDecoder->object_type);
    }

    /* mid/side decoding */
    ms_decode(ics1, ics2, spec_coef1, spec_coef2, hDecoder->frameLength);

    /* intensity stereo decoding */
    is_decode(ics1, ics2, spec_coef1, spec_coef2, hDecoder->frameLength);

#ifdef MAIN_DEC
    /* MAIN object type prediction */
    if (hDecoder->object_type == MAIN)
    {
        /* intra channel prediction */
        ic_prediction(ics1, spec_coef1, hDecoder->pred_stat[cpe->channel], hDecoder->frameLength,
            hDecoder->sf_index);
        ic_prediction(ics2, spec_coef2, hDecoder->pred_stat[cpe->paired_channel], hDecoder->frameLength,
            hDecoder->sf_index);

        /* In addition, for scalefactor bands coded by perceptual
           noise substitution the predictors belonging to the
           corresponding spectral coefficients are reset.
        */
        pns_reset_pred_state(ics1, hDecoder->pred_stat[cpe->channel]);
        pns_reset_pred_state(ics2, hDecoder->pred_stat[cpe->paired_channel]);
    }
#endif

#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type))
    {
        ltp_info *ltp1 = &(ics1->ltp);
        ltp_info *ltp2 = (cpe->common_window) ? &(ics2->ltp2) : &(ics2->ltp);
#ifdef LD_DEC
        if (hDecoder->object_type == LD)
        {
            if (ltp1->data_present)
            {
                if (ltp1->lag_update)
                    hDecoder->ltp_lag[cpe->channel] = ltp1->lag;
            }
            ltp1->lag = hDecoder->ltp_lag[cpe->channel];
            if (ltp2->data_present)
            {
                if (ltp2->lag_update)
                    hDecoder->ltp_lag[cpe->paired_channel] = ltp2->lag;
            }
            ltp2->lag = hDecoder->ltp_lag[cpe->paired_channel];
        }
#endif

        /* long term prediction */
        lt_prediction(ics1, ltp1, spec_coef1, hDecoder->lt_pred_stat[cpe->channel], hDecoder->fb,
            ics1->window_shape, hDecoder->window_shape_prev[cpe->channel],
            hDecoder->sf_index, hDecoder->object_type, hDecoder->frameLength);
        lt_prediction(ics2, ltp2, spec_coef2, hDecoder->lt_pred_stat[cpe->paired_channel], hDecoder->fb,
            ics2->window_shape, hDecoder->window_shape_prev[cpe->paired_channel],
            hDecoder->sf_index, hDecoder->object_type, hDecoder->frameLength);
    }
#endif

    /* tns decoding */
    tns_decode_frame(ics1, &(ics1->tns), hDecoder->sf_index, hDecoder->object_type,
        spec_coef1, hDecoder->frameLength);
    tns_decode_frame(ics2, &(ics2->tns), hDecoder->sf_index, hDecoder->object_type,
        spec_coef2, hDecoder->frameLength);

    /* drc decoding */
    if (hDecoder->drc->present)
    {
        if (!hDecoder->drc->exclude_mask[cpe->channel] || !hDecoder->drc->excluded_chns_present)
            drc_decode(hDecoder->drc, spec_coef1);
        if (!hDecoder->drc->exclude_mask[cpe->paired_channel] || !hDecoder->drc->excluded_chns_present)
            drc_decode(hDecoder->drc, spec_coef2);
    }

    /* filter bank */
#ifdef SSR_DEC
    if (hDecoder->object_type != SSR)
    {
#endif
#ifdef USE_SSE
        hDecoder->fb->if_func(hDecoder->fb, ics1->window_sequence, ics1->window_shape,
            hDecoder->window_shape_prev[cpe->channel], spec_coef1,
            hDecoder->time_out[cpe->channel], hDecoder->object_type, hDecoder->frameLength);
        hDecoder->fb->if_func(hDecoder->fb, ics2->window_sequence, ics2->window_shape,
            hDecoder->window_shape_prev[cpe->paired_channel], spec_coef2,
            hDecoder->time_out[cpe->paired_channel], hDecoder->object_type, hDecoder->frameLength);
#else
        ifilter_bank(hDecoder->fb, ics1->window_sequence, ics1->window_shape,
            hDecoder->window_shape_prev[cpe->channel], spec_coef1,
            hDecoder->time_out[cpe->channel], hDecoder->fb_intermed[cpe->channel],
            hDecoder->object_type, hDecoder->frameLength);
        ifilter_bank(hDecoder->fb, ics2->window_sequence, ics2->window_shape,
            hDecoder->window_shape_prev[cpe->paired_channel], spec_coef2,
            hDecoder->time_out[cpe->paired_channel], hDecoder->fb_intermed[cpe->paired_channel],
            hDecoder->object_type, hDecoder->frameLength);
#endif
#ifdef SSR_DEC
    } else {
        ssr_decode(&(ics1->ssr), hDecoder->fb, ics1->window_sequence, ics1->window_shape,
            hDecoder->window_shape_prev[cpe->channel], spec_coef1, hDecoder->time_out[cpe->channel],
            hDecoder->ssr_overlap[cpe->channel], hDecoder->ipqf_buffer[cpe->channel],
            hDecoder->prev_fmd[cpe->channel], hDecoder->frameLength);
        ssr_decode(&(ics2->ssr), hDecoder->fb, ics2->window_sequence, ics2->window_shape,
            hDecoder->window_shape_prev[cpe->paired_channel], spec_coef2, hDecoder->time_out[cpe->paired_channel],
            hDecoder->ssr_overlap[cpe->paired_channel], hDecoder->ipqf_buffer[cpe->paired_channel],
            hDecoder->prev_fmd[cpe->paired_channel], hDecoder->frameLength);
    }
#endif

    /* save window shape for next frame */
    hDecoder->window_shape_prev[cpe->channel] = ics1->window_shape;
    hDecoder->window_shape_prev[cpe->paired_channel] = ics2->window_shape;

#ifdef LTP_DEC
    if (is_ltp_ot(hDecoder->object_type))
    {
        lt_update_state(hDecoder->lt_pred_stat[cpe->channel], hDecoder->time_out[cpe->channel],
            hDecoder->fb_intermed[cpe->channel], hDecoder->frameLength, hDecoder->object_type);
        lt_update_state(hDecoder->lt_pred_stat[cpe->paired_channel], hDecoder->time_out[cpe->paired_channel],
            hDecoder->fb_intermed[cpe->paired_channel], hDecoder->frameLength, hDecoder->object_type);
    }
#endif

#ifdef SBR_DEC
    if (((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1))
        && hDecoder->sbr_alloced[hDecoder->fr_ch_ele])
    {
        uint8_t ele = hDecoder->fr_ch_ele;
        uint8_t ch0 = cpe->channel;
        uint8_t ch1 = cpe->paired_channel;

        /* following case can happen when forceUpSampling == 1 */
        if (hDecoder->sbr[ele] == NULL)
        {
            hDecoder->sbr[ele] = sbrDecodeInit(hDecoder->frameLength,
                cpe->ele_id, 2*get_sample_rate(hDecoder->sf_index)
#ifdef DRM
                , 0
#endif
                );
        }

        retval = sbrDecodeCoupleFrame(hDecoder->sbr[ele],
            hDecoder->time_out[ch0], hDecoder->time_out[ch1],
            hDecoder->postSeekResetFlag, hDecoder->forceUpSampling);
        if (retval > 0)
            return retval;
    } else if (((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1))
        && !hDecoder->sbr_alloced[hDecoder->fr_ch_ele])
    {
        return 23;
    }
#endif

    return 0;
}
