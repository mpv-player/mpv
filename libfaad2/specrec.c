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
** $Id: specrec.c,v 1.23 2003/07/29 08:20:13 menno Exp $
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
#include "specrec.h"
#include "syntax.h"
#include "iq_table.h"

#ifdef LD_DEC
static uint8_t num_swb_512_window[] =
{
    0, 0, 0, 36, 36, 37, 31, 31, 0, 0, 0, 0
};
static uint8_t num_swb_480_window[] =
{
    0, 0, 0, 35, 35, 37, 30, 30, 0, 0, 0, 0
};
#endif

static uint8_t num_swb_960_window[] =
{
    40, 40, 45, 49, 49, 49, 46, 46, 42, 42, 42, 40
};

static uint8_t num_swb_1024_window[] =
{
    41, 41, 47, 49, 49, 51, 47, 47, 43, 43, 43, 40
};

static uint8_t num_swb_128_window[] =
{
    12, 12, 12, 14, 14, 14, 15, 15, 15, 15, 15, 15
};

static uint16_t swb_offset_1024_96[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56,
    64, 72, 80, 88, 96, 108, 120, 132, 144, 156, 172, 188, 212, 240,
    276, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960, 1024
};

static uint16_t swb_offset_128_96[] =
{
    0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128
};

static uint16_t swb_offset_1024_64[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56,
    64, 72, 80, 88, 100, 112, 124, 140, 156, 172, 192, 216, 240, 268,
    304, 344, 384, 424, 464, 504, 544, 584, 624, 664, 704, 744, 784, 824,
    864, 904, 944, 984, 1024
};

static uint16_t swb_offset_128_64[] =
{
    0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128
};


static uint16_t swb_offset_1024_48[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72,
    80, 88, 96, 108, 120, 132, 144, 160, 176, 196, 216, 240, 264, 292,
    320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704, 736,
    768, 800, 832, 864, 896, 928, 1024
};

#ifdef LD_DEC
static uint16_t swb_offset_512_48[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 68, 76, 84,
    92, 100, 112, 124, 136, 148, 164, 184, 208, 236, 268, 300, 332, 364, 396,
    428, 460, 512
};

static uint16_t swb_offset_480_48[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72 ,80 ,88,
    96, 108, 120, 132, 144, 156, 172, 188, 212, 240, 272, 304, 336, 368, 400,
    432, 480
};
#endif

static uint16_t swb_offset_128_48[] =
{
    0, 4, 8, 12, 16, 20, 28, 36, 44, 56, 68, 80, 96, 112, 128
};

static uint16_t swb_offset_1024_32[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72,
    80, 88, 96, 108, 120, 132, 144, 160, 176, 196, 216, 240, 264, 292,
    320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704, 736,
    768, 800, 832, 864, 896, 928, 960, 992, 1024
};

#ifdef LD_DEC
static uint16_t swb_offset_512_32[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80,
    88, 96, 108, 120, 132, 144, 160, 176, 192, 212, 236, 260, 288, 320, 352,
    384, 416, 448, 480, 512
};

static uint16_t swb_offset_480_32[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 72, 80,
    88, 96, 104, 112, 124, 136, 148, 164, 180, 200, 224, 256, 288, 320, 352,
    384, 416, 448, 480
};
#endif

static uint16_t swb_offset_1024_24[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 52, 60, 68,
    76, 84, 92, 100, 108, 116, 124, 136, 148, 160, 172, 188, 204, 220,
    240, 260, 284, 308, 336, 364, 396, 432, 468, 508, 552, 600, 652, 704,
    768, 832, 896, 960, 1024
};

#ifdef LD_DEC
static uint16_t swb_offset_512_24[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 52, 60, 68,
    80, 92, 104, 120, 140, 164, 192, 224, 256, 288, 320, 352, 384, 416,
    448, 480, 512
};

static uint16_t swb_offset_480_24[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 52, 60, 68, 80, 92, 104, 120,
    140, 164, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480
};
#endif

static uint16_t swb_offset_128_24[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 64, 76, 92, 108, 128
};

static uint16_t swb_offset_1024_16[] =
{
    0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 100, 112, 124,
    136, 148, 160, 172, 184, 196, 212, 228, 244, 260, 280, 300, 320, 344,
    368, 396, 424, 456, 492, 532, 572, 616, 664, 716, 772, 832, 896, 960, 1024
};

static uint16_t swb_offset_128_16[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 60, 72, 88, 108, 128
};

static uint16_t swb_offset_1024_8[] =
{
    0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 172,
    188, 204, 220, 236, 252, 268, 288, 308, 328, 348, 372, 396, 420, 448,
    476, 508, 544, 580, 620, 664, 712, 764, 820, 880, 944, 1024
};

static uint16_t swb_offset_128_8[] =
{
    0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 60, 72, 88, 108, 128
};

static uint16_t *swb_offset_1024_window[] =
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
static uint16_t *swb_offset_512_window[] =
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

static uint16_t *swb_offset_480_window[] =
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

static uint16_t *swb_offset_128_window[] =
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
void quant_to_spec(ic_stream *ics, real_t *spec_data, uint16_t frame_len)
{
    uint8_t g, sfb, win;
    uint16_t width, bin;
    real_t *start_inptr, *start_win_ptr, *win_ptr;

    real_t tmp_spec[1024];
    real_t *tmp_spec_ptr, *spec_ptr;

    tmp_spec_ptr = tmp_spec;
    memset(tmp_spec_ptr, 0, frame_len*sizeof(real_t));

    spec_ptr = spec_data;
    tmp_spec_ptr = tmp_spec;
    start_win_ptr = tmp_spec_ptr;

    for (g = 0; g < ics->num_window_groups; g++)
    {
        uint16_t j = 0;
        uint16_t win_inc = 0;

        start_inptr = spec_ptr;

        win_inc = ics->swb_offset[ics->num_swb];

        for (sfb = 0; sfb < ics->num_swb; sfb++)
        {
            width = ics->swb_offset[sfb+1] - ics->swb_offset[sfb];

            win_ptr = start_win_ptr;

            for (win = 0; win < ics->window_group_length[g]; win++)
            {
                tmp_spec_ptr = win_ptr + j;

                for (bin = 0; bin < width; bin += 4)
                {
                    tmp_spec_ptr[0] = spec_ptr[0];
                    tmp_spec_ptr[1] = spec_ptr[1];
                    tmp_spec_ptr[2] = spec_ptr[2];
                    tmp_spec_ptr[3] = spec_ptr[3];
                    tmp_spec_ptr += 4;
                    spec_ptr += 4;
                }

                win_ptr += win_inc;
            }
            j += width;
        }
        start_win_ptr += (spec_ptr - start_inptr);
    }

    spec_ptr = spec_data;
    tmp_spec_ptr = tmp_spec;

    memcpy(spec_ptr, tmp_spec_ptr, frame_len*sizeof(real_t));
}

#ifndef FIXED_POINT
void build_tables(real_t *pow2_table)
{
    uint16_t i;

    /* build pow(2, 0.25*x) table for scalefactors */
    for(i = 0; i < POW_TABLE_SIZE; i++)
    {
        pow2_table[i] = REAL_CONST(pow(2.0, 0.25 * (i-100)));
    }
}
#endif

static INLINE real_t iquant(int16_t q, real_t *tab)
{
    int16_t sgn = 1;

    if (q == 0) return 0;

    if (q < 0)
    {
        q = -q;
        sgn = -1;
    }

    if (q >= IQ_TABLE_SIZE)
        return sgn * tab[q>>3] * 16;

    return sgn * tab[q];
}

void inverse_quantization(real_t *x_invquant, int16_t *x_quant, uint16_t frame_len)
{
    int16_t i;
    int16_t *in_ptr = x_quant;
    real_t *out_ptr = x_invquant;
    real_t *tab = iq_table;

    for(i = frame_len/4-1; i >= 0; --i)
    {
        out_ptr[0] = iquant(in_ptr[0], tab);
        out_ptr[1] = iquant(in_ptr[1], tab);
        out_ptr[2] = iquant(in_ptr[2], tab);
        out_ptr[3] = iquant(in_ptr[3], tab);
        out_ptr += 4;
        in_ptr += 4;
    }
}

#ifndef FIXED_POINT
static INLINE real_t get_scale_factor_gain(uint16_t scale_factor, real_t *pow2_table)
{
    if (scale_factor < POW_TABLE_SIZE)
        return pow2_table[scale_factor];
    else
        return REAL_CONST(pow(2.0, 0.25 * (scale_factor - 100)));
}
#else
static real_t pow2_table[] =
{
    COEF_CONST(0.59460355750136),
    COEF_CONST(0.70710678118655),
    COEF_CONST(0.84089641525371),
    COEF_CONST(1.0),
    COEF_CONST(1.18920711500272),
    COEF_CONST(1.41421356237310),
    COEF_CONST(1.68179283050743)
};
#endif

#ifdef FIXED_POINT
void apply_scalefactors(faacDecHandle hDecoder, ic_stream *ics, real_t *x_invquant,
                        uint16_t frame_len)
#else
void apply_scalefactors(ic_stream *ics, real_t *x_invquant, real_t *pow2_table,
                        uint16_t frame_len)
#endif
{
    uint8_t g, sfb;
    uint16_t top;
    real_t *fp;
#ifndef FIXED_POINT
    real_t scale;
#else
    int32_t exp, frac;
#endif
    uint8_t groups = 0;
    uint16_t nshort = frame_len/8;

    for (g = 0; g < ics->num_window_groups; g++)
    {
        uint16_t k = 0;

        /* using this 128*groups doesn't hurt long blocks, because
           long blocks only have 1 group, so that means 'groups' is
           always 0 for long blocks
        */
        fp = x_invquant + (groups*nshort);

        for (sfb = 0; sfb < ics->max_sfb; sfb++)
        {
            top = ics->sect_sfb_offset[g][sfb+1];

#ifndef FIXED_POINT
            scale = get_scale_factor_gain(ics->scale_factors[g][sfb], pow2_table);
#else
            exp = (ics->scale_factors[g][sfb] - 100) / 4;
            frac = (ics->scale_factors[g][sfb] - 100) % 4;

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
#ifndef FIXED_POINT
                fp[0] = fp[0] * scale;
                fp[1] = fp[1] * scale;
                fp[2] = fp[2] * scale;
                fp[3] = fp[3] * scale;
#else
                if (exp < 0)
                {
                    fp[0] >>= -exp;
                    fp[1] >>= -exp;
                    fp[2] >>= -exp;
                    fp[3] >>= -exp;
                } else {
                    fp[0] <<= exp;
                    fp[1] <<= exp;
                    fp[2] <<= exp;
                    fp[3] <<= exp;
                }

                if (frac)
                {
                    fp[0] = MUL_R_C(fp[0],pow2_table[frac + 3]);
                    fp[1] = MUL_R_C(fp[1],pow2_table[frac + 3]);
                    fp[2] = MUL_R_C(fp[2],pow2_table[frac + 3]);
                    fp[3] = MUL_R_C(fp[3],pow2_table[frac + 3]);
                }
#endif
                fp += 4;
            }
        }
        groups += ics->window_group_length[g];
    }
}
