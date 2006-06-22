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
** Initially modified for use with MPlayer by Rich Felker on 2005/03/29
** $Id$
** detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
**/

#include "common.h"
#include "structs.h"

#include "output.h"
#include "decoder.h"

#ifndef FIXED_POINT


#define FLOAT_SCALE (1.0f/(1<<15))

#define DM_MUL REAL_CONST(0.3203772410170407) // 1/(1+sqrt(2) + 1/sqrt(2))
#define RSQRT2 REAL_CONST(0.7071067811865475244) // 1/sqrt(2)


static INLINE real_t get_sample(real_t **input, uint8_t channel, uint16_t sample,
                                uint8_t down_matrix, uint8_t *internal_channel)
{
    if (!down_matrix)
        return input[internal_channel[channel]][sample];

    if (channel == 0)
    {
        return DM_MUL * (input[internal_channel[1]][sample] +
            input[internal_channel[0]][sample] * RSQRT2 +
            input[internal_channel[3]][sample] * RSQRT2);
    } else {
        return DM_MUL * (input[internal_channel[2]][sample] +
            input[internal_channel[0]][sample] * RSQRT2 +
            input[internal_channel[4]][sample] * RSQRT2);
    }
}

#ifndef HAS_LRINTF
#define CLIP(sample, max, min) \
if (sample >= 0.0f)            \
{                              \
    sample += 0.5f;            \
    if (sample >= max)         \
        sample = max;          \
} else {                       \
    sample += -0.5f;           \
    if (sample <= min)         \
        sample = min;          \
}
#else
#define CLIP(sample, max, min) \
if (sample >= 0.0f)            \
{                              \
    if (sample >= max)         \
        sample = max;          \
} else {                       \
    if (sample <= min)         \
        sample = min;          \
}
#endif

#define CONV(a,b) ((a<<1)|(b&0x1))

static void to_PCM_16bit(NeAACDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int16_t **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];

            CLIP(inp, 32767.0f, -32768.0f);

            (*sample_buffer)[i] = (int16_t)lrintf(inp);
        }
        break;
    case CONV(2,0):
        if (hDecoder->upMatrix)
        {
            ch  = hDecoder->internal_channel[0];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch][i];

                CLIP(inp0, 32767.0f, -32768.0f);

                (*sample_buffer)[(i*2)+0] = (int16_t)lrintf(inp0);
                (*sample_buffer)[(i*2)+1] = (int16_t)lrintf(inp0);
            }
        } else {
            ch  = hDecoder->internal_channel[0];
            ch1 = hDecoder->internal_channel[1];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch ][i];
                real_t inp1 = input[ch1][i];

                CLIP(inp0, 32767.0f, -32768.0f);
                CLIP(inp1, 32767.0f, -32768.0f);

                (*sample_buffer)[(i*2)+0] = (int16_t)lrintf(inp0);
                (*sample_buffer)[(i*2)+1] = (int16_t)lrintf(inp1);
            }
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);

                CLIP(inp, 32767.0f, -32768.0f);

                (*sample_buffer)[(i*channels)+ch] = (int16_t)lrintf(inp);
            }
        }
        break;
    }
}

static void to_PCM_24bit(NeAACDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];

            inp *= 256.0f;
            CLIP(inp, 8388607.0f, -8388608.0f);

            (*sample_buffer)[i] = (int32_t)lrintf(inp);
        }
        break;
    case CONV(2,0):
        if (hDecoder->upMatrix)
        {
            ch = hDecoder->internal_channel[0];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch][i];

                inp0 *= 256.0f;
                CLIP(inp0, 8388607.0f, -8388608.0f);

                (*sample_buffer)[(i*2)+0] = (int32_t)lrintf(inp0);
                (*sample_buffer)[(i*2)+1] = (int32_t)lrintf(inp0);
            }
        } else {
            ch  = hDecoder->internal_channel[0];
            ch1 = hDecoder->internal_channel[1];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch ][i];
                real_t inp1 = input[ch1][i];

                inp0 *= 256.0f;
                inp1 *= 256.0f;
                CLIP(inp0, 8388607.0f, -8388608.0f);
                CLIP(inp1, 8388607.0f, -8388608.0f);

                (*sample_buffer)[(i*2)+0] = (int32_t)lrintf(inp0);
                (*sample_buffer)[(i*2)+1] = (int32_t)lrintf(inp1);
            }
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);

                inp *= 256.0f;
                CLIP(inp, 8388607.0f, -8388608.0f);

                (*sample_buffer)[(i*channels)+ch] = (int32_t)lrintf(inp);
            }
        }
        break;
    }
}

static void to_PCM_32bit(NeAACDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];

            inp *= 65536.0f;
            CLIP(inp, 2147483647.0f, -2147483648.0f);

            (*sample_buffer)[i] = (int32_t)lrintf(inp);
        }
        break;
    case CONV(2,0):
        if (hDecoder->upMatrix)
        {
            ch = hDecoder->internal_channel[0];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch][i];

                inp0 *= 65536.0f;
                CLIP(inp0, 2147483647.0f, -2147483648.0f);

                (*sample_buffer)[(i*2)+0] = (int32_t)lrintf(inp0);
                (*sample_buffer)[(i*2)+1] = (int32_t)lrintf(inp0);
            }
        } else {
            ch  = hDecoder->internal_channel[0];
            ch1 = hDecoder->internal_channel[1];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch ][i];
                real_t inp1 = input[ch1][i];

                inp0 *= 65536.0f;
                inp1 *= 65536.0f;
                CLIP(inp0, 2147483647.0f, -2147483648.0f);
                CLIP(inp1, 2147483647.0f, -2147483648.0f);

                (*sample_buffer)[(i*2)+0] = (int32_t)lrintf(inp0);
                (*sample_buffer)[(i*2)+1] = (int32_t)lrintf(inp1);
            }
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);

                inp *= 65536.0f;
                CLIP(inp, 2147483647.0f, -2147483648.0f);

                (*sample_buffer)[(i*channels)+ch] = (int32_t)lrintf(inp);
            }
        }
        break;
    }
}

static void to_PCM_float(NeAACDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         float32_t **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];
            (*sample_buffer)[i] = inp*FLOAT_SCALE;
        }
        break;
    case CONV(2,0):
        if (hDecoder->upMatrix)
        {
            ch = hDecoder->internal_channel[0];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch][i];
                (*sample_buffer)[(i*2)+0] = inp0*FLOAT_SCALE;
                (*sample_buffer)[(i*2)+1] = inp0*FLOAT_SCALE;
            }
        } else {
            ch  = hDecoder->internal_channel[0];
            ch1 = hDecoder->internal_channel[1];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch ][i];
                real_t inp1 = input[ch1][i];
                (*sample_buffer)[(i*2)+0] = inp0*FLOAT_SCALE;
                (*sample_buffer)[(i*2)+1] = inp1*FLOAT_SCALE;
            }
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);
                (*sample_buffer)[(i*channels)+ch] = inp*FLOAT_SCALE;
            }
        }
        break;
    }
}

static void to_PCM_double(NeAACDecHandle hDecoder, real_t **input,
                          uint8_t channels, uint16_t frame_len,
                          double **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];
            (*sample_buffer)[i] = (double)inp*FLOAT_SCALE;
        }
        break;
    case CONV(2,0):
        if (hDecoder->upMatrix)
        {
            ch = hDecoder->internal_channel[0];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch][i];
                (*sample_buffer)[(i*2)+0] = (double)inp0*FLOAT_SCALE;
                (*sample_buffer)[(i*2)+1] = (double)inp0*FLOAT_SCALE;
            }
        } else {
            ch  = hDecoder->internal_channel[0];
            ch1 = hDecoder->internal_channel[1];
            for(i = 0; i < frame_len; i++)
            {
                real_t inp0 = input[ch ][i];
                real_t inp1 = input[ch1][i];
                (*sample_buffer)[(i*2)+0] = (double)inp0*FLOAT_SCALE;
                (*sample_buffer)[(i*2)+1] = (double)inp1*FLOAT_SCALE;
            }
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);
                (*sample_buffer)[(i*channels)+ch] = (double)inp*FLOAT_SCALE;
            }
        }
        break;
    }
}

void *output_to_PCM(NeAACDecHandle hDecoder,
                    real_t **input, void *sample_buffer, uint8_t channels,
                    uint16_t frame_len, uint8_t format)
{
    int16_t   *short_sample_buffer = (int16_t*)sample_buffer;
    int32_t   *int_sample_buffer = (int32_t*)sample_buffer;
    float32_t *float_sample_buffer = (float32_t*)sample_buffer;
    double    *double_sample_buffer = (double*)sample_buffer;

#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif

    /* Copy output to a standard PCM buffer */
    switch (format)
    {
    case FAAD_FMT_16BIT:
        to_PCM_16bit(hDecoder, input, channels, frame_len, &short_sample_buffer);
        break;
    case FAAD_FMT_24BIT:
        to_PCM_24bit(hDecoder, input, channels, frame_len, &int_sample_buffer);
        break;
    case FAAD_FMT_32BIT:
        to_PCM_32bit(hDecoder, input, channels, frame_len, &int_sample_buffer);
        break;
    case FAAD_FMT_FLOAT:
        to_PCM_float(hDecoder, input, channels, frame_len, &float_sample_buffer);
        break;
    case FAAD_FMT_DOUBLE:
        to_PCM_double(hDecoder, input, channels, frame_len, &double_sample_buffer);
        break;
    }

#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->output_cycles += count;
#endif

    return sample_buffer;
}

#else

#define DM_MUL FRAC_CONST(0.3203772410170407) // 1/(1+sqrt(2) + 1/sqrt(2))
#define RSQRT2 FRAC_CONST(0.7071067811865475244) // 1/sqrt(2)

static INLINE real_t get_sample(real_t **input, uint8_t channel, uint16_t sample,
                                uint8_t down_matrix, uint8_t up_matrix,
                                uint8_t *internal_channel)
{
    if (up_matrix == 1)
        return input[internal_channel[0]][sample];

    if (!down_matrix)
        return input[internal_channel[channel]][sample];

    if (channel == 0)
    {
        real_t C   = MUL_F(input[internal_channel[0]][sample], RSQRT2);
        real_t L_S = MUL_F(input[internal_channel[3]][sample], RSQRT2);
        real_t cum = input[internal_channel[1]][sample] + C + L_S;
        return MUL_F(cum, DM_MUL);
    } else {
        real_t C   = MUL_F(input[internal_channel[0]][sample], RSQRT2);
        real_t R_S = MUL_F(input[internal_channel[4]][sample], RSQRT2);
        real_t cum = input[internal_channel[2]][sample] + C + R_S;
        return MUL_F(cum, DM_MUL);
    }
}

void* output_to_PCM_sux(NeAACDecHandle hDecoder,
                    real_t **input, void *sample_buffer, uint8_t channels,
                    uint16_t frame_len, uint8_t format)
{
    uint8_t ch;
    uint16_t i;
    int16_t *short_sample_buffer = (int16_t*)sample_buffer;
    int32_t *int_sample_buffer = (int32_t*)sample_buffer;

    /* Copy output to a standard PCM buffer */
    for (ch = 0; ch < channels; ch++)
    {
        switch (format)
        {
        case FAAD_FMT_16BIT:
            for(i = 0; i < frame_len; i++)
            {
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix,
                    hDecoder->internal_channel);
                if (tmp >= 0)
                {
                    tmp += (1 << (REAL_BITS-1));
                    if (tmp >= REAL_CONST(32767))
                    {
                        tmp = REAL_CONST(32767);
                    }
                } else {
                    tmp += -(1 << (REAL_BITS-1));
                    if (tmp <= REAL_CONST(-32768))
                    {
                        tmp = REAL_CONST(-32768);
                    }
                }
                tmp >>= REAL_BITS;
                short_sample_buffer[(i*channels)+ch] = (int16_t)tmp;
            }
            break;
        case FAAD_FMT_24BIT:
            for(i = 0; i < frame_len; i++)
            {
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix,
                    hDecoder->internal_channel);
                if (tmp >= 0)
                {
                    tmp += (1 << (REAL_BITS-9));
                    tmp >>= (REAL_BITS-8);
                    if (tmp >= 8388607)
                    {
                        tmp = 8388607;
                    }
                } else {
                    tmp += -(1 << (REAL_BITS-9));
                    tmp >>= (REAL_BITS-8);
                    if (tmp <= -8388608)
                    {
                        tmp = -8388608;
                    }
                }
                int_sample_buffer[(i*channels)+ch] = (int32_t)tmp;
            }
            break;
        case FAAD_FMT_32BIT:
            for(i = 0; i < frame_len; i++)
            {
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix,
                    hDecoder->internal_channel);
                if (tmp >= 0)
                {
                    tmp += (1 << (16-REAL_BITS-1));
                    tmp <<= (16-REAL_BITS);
                } else {
                    tmp += -(1 << (16-REAL_BITS-1));
                    tmp <<= (16-REAL_BITS);
                }
                int_sample_buffer[(i*channels)+ch] = (int32_t)tmp;
            }
            break;
        case FAAD_FMT_FIXED:
            for(i = 0; i < frame_len; i++)
            {
                real_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->upMatrix,
                    hDecoder->internal_channel);
                int_sample_buffer[(i*channels)+ch] = (int32_t)tmp;
            }
            break;
        }
    }

    return sample_buffer;
}

void* output_to_PCM(NeAACDecHandle hDecoder,
                    real_t **input, void *sample_buffer, uint8_t channels,
                    uint16_t frame_len, uint8_t format)
{
    int ch;
    int i;
    int16_t *short_sample_buffer = (int16_t*)sample_buffer;
    real_t *ch0 = input[hDecoder->internal_channel[0]];
    real_t *ch1 = input[hDecoder->internal_channel[1]];
    real_t *ch2 = input[hDecoder->internal_channel[2]];
    real_t *ch3 = input[hDecoder->internal_channel[3]];
    real_t *ch4 = input[hDecoder->internal_channel[4]];

    if (format != FAAD_FMT_16BIT)
        return output_to_PCM_sux(hDecoder, input, sample_buffer, channels, frame_len, format);

    if (hDecoder->downMatrix) {
        for(i = 0; i < frame_len; i++)
        {
	    int32_t tmp;
	    tmp = (ch1[i] + ((ch0[i]+ch3[i])>>1) + ((ch0[i]+ch3[i])>>2) + (1<<(REAL_BITS))) >> (REAL_BITS+1);
	    if ((tmp+0x8000) & ~0xffff) tmp = ~(tmp>>31)-0x8000;
            short_sample_buffer[0] = tmp;
	    tmp = (ch2[i] + ((ch0[i]+ch4[i])>>1) + ((ch0[i]+ch4[i])>>2) + (1<<(REAL_BITS))) >> (REAL_BITS+1);
	    if ((tmp+0x8000) & ~0xffff) tmp = ~(tmp>>31)-0x8000;
            short_sample_buffer[1] = tmp;
	    short_sample_buffer += channels;
        }
        return sample_buffer;
    }

    /* Copy output to a standard PCM buffer */
    for(i = 0; i < frame_len; i++)
    {
        for (ch = 0; ch < channels; ch++)
        {
            int32_t tmp = input[hDecoder->internal_channel[ch]][i];
            tmp += (1 << (REAL_BITS-1));
            tmp >>= REAL_BITS;
	    if ((tmp+0x8000) & ~0xffff) tmp = ~(tmp>>31)-0x8000;
            *(short_sample_buffer++) = tmp;
        }
    }

    return sample_buffer;
}

#endif
