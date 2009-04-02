/*
 * The Bauer stereophonic-to-binaural DSP using bs2b library:
 * http://bs2b.sourceforge.net/
 *
 * Copyright (c) 2009 Andrew Savchenko
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <bs2b.h>
#include <inttypes.h>
#include <stdlib.h>

#include "af.h"
#include "subopt-helper.h"

/// Internal specific data of the filter
struct af_bs2b {
    int level;          ///< crossfeed level
    int profile;        ///< profile (easy or normal)
    t_bs2bdp filter;    ///< instance of a library filter
};

#define PLAY(name,type) \
static af_data_t *play_##name(struct af_instance_s *af, af_data_t *data) \
{ \
    /* filter is called for all pairs of samples available in the buffer */ \
    bs2b_cross_feed_##name(((struct af_bs2b*)(af->setup))->filter, \
        (type*)(data->audio), data->len/data->bps/2); \
\
    return data; \
}

PLAY(fbe,float)
PLAY(fle,float)
PLAY(s32be,int32_t)
PLAY(s32le,int32_t)
PLAY(s24be,bs2b_int24_t)
PLAY(s24le,bs2b_int24_t)
PLAY(s16be,int16_t)
PLAY(s16le,int16_t)
PLAY(s8,int8_t)
PLAY(u8,uint8_t)

/// Sanity check for level value
static int test_level(void *par)
{
    const int val = *(int*)par;
    if (val >= 1 && val <= BS2B_CLEVELS)
        return 1;

    mp_msg(MSGT_AFILTER,MSGL_ERR, "[bs2b] Level must be in range 1..%i, but "
           "current value is %i.\n", BS2B_CLEVELS, val);
    return 0;
}

/// Sanity check for profile value
static int test_profile(void *par)
{
    const int val = *(int*)par;
    if (val >= 0 && val <= 1)
        return 1;

    mp_msg(MSGT_AFILTER,MSGL_ERR, "[bs2b] Profile must be either 0 or 1, but "
           "current value is %i.\n", val);
    return 0;
}

/// Initialization and runtime control
static int control(struct af_instance_s *af, int cmd, void *arg)
{
    struct af_bs2b *s = af->setup;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        int format;
        char buf[256];
        // Sanity check
        if (!arg) return AF_ERROR;

        format           = ((af_data_t*)arg)->format;
        af->data->rate   = ((af_data_t*)arg)->rate;
        af->data->nch    = 2;     // bs2b is useful only for 2ch audio
        af->data->bps    = ((af_data_t*)arg)->bps;
        af->data->format = format;

        /* check for formats supported by libbs2b
           and assign corresponding handlers */
        switch (format) {
            case AF_FORMAT_FLOAT_BE:
                af->play = play_fbe;
                break;
            case AF_FORMAT_FLOAT_LE:
                af->play = play_fle;
                break;
            case AF_FORMAT_S32_BE:
                af->play = play_s32be;
                break;
            case AF_FORMAT_S32_LE:
                af->play = play_s32le;
                break;
            case AF_FORMAT_S24_BE:
                af->play = play_s24be;
                break;
            case AF_FORMAT_S24_LE:
                af->play = play_s24le;
                break;
            case AF_FORMAT_S16_BE:
                af->play = play_s16be;
                break;
            case AF_FORMAT_S16_LE:
                af->play = play_s16le;
                break;
            case AF_FORMAT_S8:
                af->play = play_s8;
                break;
            case AF_FORMAT_U8:
                af->play = play_u8;
                break;
            default:
#ifdef WORDS_BIGENDIAN
                af->play = play_fbe;
#else
                af->play = play_fle;
#endif //WORDS_BIGENDIAN
                af->data->format = AF_FORMAT_FLOAT_NE;
                af->data->bps=4;
                break;
        }

        bs2b_set_srate(s->filter, (long)af->data->rate);
        mp_msg(MSGT_AFILTER,MSGL_V, "[bs2b] using format %s\n",
               af_fmt2str(af->data->format,buf,256));

        return af_test_output(af,(af_data_t*)arg);
    }
    case AF_CONTROL_COMMAND_LINE: {
        const opt_t subopts[] = {
            {"level",   OPT_ARG_INT, &s->level,   test_level},
            {"profile", OPT_ARG_INT, &s->profile, test_profile},
            {NULL}
        };
        if (subopt_parse(arg, subopts) != 0) {
            mp_msg(MSGT_AFILTER,MSGL_ERR, "[bs2b] Invalid option specified.\n");
            return AF_ERROR;
        }

        bs2b_set_level(s->filter, s->level + s->profile ? BS2B_CLEVELS : 0);
        mp_msg(MSGT_AFILTER,MSGL_V, "[bs2b] using profile %i, level %i\n",
               s->profile, s->level);
        return AF_OK;
    }
    }
    return AF_UNKNOWN;
}

/// Deallocate memory and close library
static void uninit(struct af_instance_s *af)
{
    struct af_bs2b *s = af->setup;
    free(af->data);
    if (s && s->filter)
        bs2b_close(s->filter);
    free(s);
}

/// Allocate memory, set function pointers and init library
static int af_open(af_instance_t *af)
{
    struct af_bs2b *s;
    af->control = control;
    af->uninit  = uninit;
    af->mul     = 1;
    if (!(af->data = calloc(1,sizeof(af_data_t))))
        return AF_ERROR;
    if (!(af->setup = s = calloc(1,sizeof(struct af_bs2b)))) {
        free(af->data);
        return AF_ERROR;
    }

    // NULL means failed initialization
    if (!(s->filter = bs2b_open())) {
        free(af->data);
        free(af->setup);
        return AF_ERROR;
    }
    // Set defaults the same as in the library:
    s->level   = 3;
    s->profile = 1;
    return AF_OK;
}

/// Description of this filter
af_info_t af_info_bs2b = {
    "Bauer stereophonic-to-binaural audio filter",
    "bs2b",
    "Andrew Savchenko",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
