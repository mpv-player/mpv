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
#include <string.h>

#include "af.h"
#include "subopt-helper.h"

/// Internal specific data of the filter
struct af_bs2b {
    int fcut;           ///< cut frequency in Hz
    int feed;           ///< feed level for low frequencies in 0.1*dB
    char *profile;      ///< profile (available crossfeed presets)
    t_bs2bdp filter;    ///< instance of a library filter
};

#define PLAY(name, type) \
static af_data_t *play_##name(struct af_instance_s *af, af_data_t *data) \
{ \
    /* filter is called for all pairs of samples available in the buffer */ \
    bs2b_cross_feed_##name(((struct af_bs2b*)(af->setup))->filter, \
        (type*)(data->audio), data->len/data->bps/2); \
\
    return data; \
}

PLAY(f, float)
PLAY(fbe, float)
PLAY(fle, float)
PLAY(s32be, int32_t)
PLAY(u32be, uint32_t)
PLAY(s32le, int32_t)
PLAY(u32le, uint32_t)
PLAY(s24be, bs2b_int24_t)
PLAY(u24be, bs2b_uint24_t)
PLAY(s24le, bs2b_int24_t)
PLAY(u24le, bs2b_uint24_t)
PLAY(s16be, int16_t)
PLAY(u16be, uint16_t)
PLAY(s16le, int16_t)
PLAY(u16le, uint16_t)
PLAY(s8, int8_t)
PLAY(u8, uint8_t)

/// Sanity check for fcut value
static int test_fcut(void *par)
{
    const int val = *(int*)par;
    if (val >= BS2B_MINFCUT && val <= BS2B_MAXFCUT)
        return 1;

    mp_msg(MSGT_AFILTER, MSGL_ERR,
           "[bs2b] Cut frequency must be in range [%d..%d], but current value is %d.\n",
           BS2B_MINFCUT, BS2B_MAXFCUT, val);
    return 0;
}

/// Sanity check for feed value
static int test_feed(void *par)
{
    const int val = *(int*)par;
    if (val >= BS2B_MINFEED && val <= BS2B_MAXFEED)
        return 1;

    mp_msg(MSGT_AFILTER, MSGL_ERR,
           "[bs2b] Feed level must be in range [%d..%d], but current value is %d.\n",
           BS2B_MINFEED, BS2B_MAXFEED, val);
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
            case AF_FORMAT_U32_BE:
                af->play = play_u32be;
                break;
            case AF_FORMAT_S32_LE:
                af->play = play_s32le;
                break;
            case AF_FORMAT_U32_LE:
                af->play = play_u32le;
                break;
            case AF_FORMAT_S24_BE:
                af->play = play_s24be;
                break;
            case AF_FORMAT_U24_BE:
                af->play = play_u24be;
                break;
            case AF_FORMAT_S24_LE:
                af->play = play_s24le;
                break;
            case AF_FORMAT_U24_LE:
                af->play = play_u24le;
                break;
            case AF_FORMAT_S16_BE:
                af->play = play_s16be;
                break;
            case AF_FORMAT_U16_BE:
                af->play = play_u16be;
                break;
            case AF_FORMAT_S16_LE:
                af->play = play_s16le;
                break;
            case AF_FORMAT_U16_LE:
                af->play = play_u16le;
                break;
            case AF_FORMAT_S8:
                af->play = play_s8;
                break;
            case AF_FORMAT_U8:
                af->play = play_u8;
                break;
            default:
                af->play = play_f;
                af->data->format = AF_FORMAT_FLOAT_NE;
                af->data->bps = 4;
                break;
        }

        // bs2b have srate limits, try to resample if needed
        if (af->data->rate > BS2B_MAXSRATE || af->data->rate < BS2B_MINSRATE) {
            af->data->rate = BS2B_DEFAULT_SRATE;
            mp_msg(MSGT_AFILTER, MSGL_WARN,
                   "[bs2b] Requested sample rate %d Hz is out of bounds [%d..%d] Hz.\n"
                   "[bs2b] Trying to resample to %d Hz.\n",
                   af->data->rate, BS2B_MINSRATE, BS2B_MAXSRATE, BS2B_DEFAULT_SRATE);
        }
        bs2b_set_srate(s->filter, (long)af->data->rate);
        mp_msg(MSGT_AFILTER, MSGL_V, "[bs2b] using format %s\n",
               af_fmt2str(af->data->format,buf,256));

        return af_test_output(af,(af_data_t*)arg);
    }
    case AF_CONTROL_COMMAND_LINE: {
        const opt_t subopts[] = {
            {"fcut",    OPT_ARG_INT,   &s->fcut,    test_fcut},
            {"feed",    OPT_ARG_INT,   &s->feed,    test_feed},
            {"profile", OPT_ARG_MSTRZ, &s->profile, NULL},
            {NULL}
        };
        if (subopt_parse(arg, subopts) != 0) {
            mp_msg(MSGT_AFILTER, MSGL_ERR, "[bs2b] Invalid option specified.\n");
            free(s->profile);
            return AF_ERROR;
        }
        // parse profile if specified
        if (s->profile) {
            if (!strcmp(s->profile, "default"))
                bs2b_set_level(s->filter, BS2B_DEFAULT_CLEVEL);
            else if (!strcmp(s->profile, "cmoy"))
                bs2b_set_level(s->filter, BS2B_CMOY_CLEVEL);
            else if (!strcmp(s->profile, "jmeier"))
                bs2b_set_level(s->filter, BS2B_JMEIER_CLEVEL);
            else {
                mp_msg(MSGT_AFILTER, MSGL_ERR,
                       "[bs2b] Invalid profile specified: %s.\n"
                       "[bs2b] Available profiles are: default, cmoy, jmeier.\n",
                       s->profile);
                free(s->profile);
                return AF_ERROR;
            }
        }
        // set fcut and feed only if specified, otherwise defaults will be used
        if (s->fcut)
            bs2b_set_level_fcut(s->filter, s->fcut);
        if (s->feed)
            bs2b_set_level_feed(s->filter, s->feed);

        mp_msg(MSGT_AFILTER, MSGL_V,
               "[bs2b] using cut frequency %d, LF feed level %d\n",
               bs2b_get_level_fcut(s->filter), bs2b_get_level_feed(s->filter));
        free(s->profile);
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
    if (!(af->data = calloc(1, sizeof(af_data_t))))
        return AF_ERROR;
    if (!(af->setup = s = calloc(1, sizeof(struct af_bs2b)))) {
        free(af->data);
        return AF_ERROR;
    }

    // NULL means failed initialization
    if (!(s->filter = bs2b_open())) {
        free(af->data);
        free(af->setup);
        return AF_ERROR;
    }
    // Set zero defaults indicating no option was specified.
    s->profile = NULL;
    s->fcut    = 0;
    s->feed    = 0;
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
