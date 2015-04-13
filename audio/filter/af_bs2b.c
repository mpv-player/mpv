/*
 * The Bauer stereophonic-to-binaural DSP using bs2b library:
 * http://bs2b.sourceforge.net/
 *
 * Copyright (c) 2009 Andrew Savchenko
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <bs2b.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "af.h"
#include "options/m_option.h"

/// Internal specific data of the filter
struct af_bs2b {
    int fcut;           ///< cut frequency in Hz
    int feed;           ///< feed level for low frequencies in 0.1*dB
    int profile  ;      ///< profile (available crossfeed presets)
    t_bs2bdp filter;    ///< instance of a library filter
};

#define DEF_FILTER(fmt, name) \
static int filter_##name(struct af_instance *af, struct mp_audio *data) \
{ \
    if (!data) \
        return 0; \
    if (af_make_writeable(af, data) < 0) { \
        talloc_free(data); \
        return -1; \
    } \
    bs2b_cross_feed_##name(((struct af_bs2b*)(af->priv))->filter, \
        (data->planes[0]), data->samples); \
    af_add_output_frame(af, data); \
    return 0; \
}

#define GET_FILTER(fmt, name) \
    case AF_FORMAT_##fmt: return filter_##name;

#define FILTERS \
    FILTER(FLOAT,   f)      \
    FILTER(S32,     s32)    \
    FILTER(U32,     u32)    \
    FILTER(S24,     s24)    \
    FILTER(U24,     u24)    \
    FILTER(S16,     s16)    \
    FILTER(U16,     u16)    \
    FILTER(S8,      s8)     \
    FILTER(U8,      u8)

#define FILTER DEF_FILTER
FILTERS
#undef FILTER

typedef int (*filter)(struct af_instance *af, struct mp_audio *d);
static filter get_filter(int fmt)
{
    switch (fmt) {
#define FILTER GET_FILTER
FILTERS
#undef FILTER
    default: return NULL;
    }
}

/// Initialization and runtime control
static int control(struct af_instance *af, int cmd, void *arg)
{
    struct af_bs2b *s = af->priv;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        int format;
        // Sanity check
        if (!arg) return AF_ERROR;

        format           = ((struct mp_audio*)arg)->format;
        af->data->rate   = ((struct mp_audio*)arg)->rate;
        mp_audio_set_num_channels(af->data, 2);     // bs2b is useful only for 2ch audio
        mp_audio_set_format(af->data, format);

        /* check for formats supported by libbs2b
           and assign corresponding handlers */
        af->filter_frame = get_filter(format);
        if (!af->filter_frame) {
            af->filter_frame = filter_f;
            mp_audio_set_format(af->data, AF_FORMAT_FLOAT);
        }

        // bs2b have srate limits, try to resample if needed
        if (af->data->rate > BS2B_MAXSRATE || af->data->rate < BS2B_MINSRATE) {
            af->data->rate = BS2B_DEFAULT_SRATE;
            MP_WARN(af, "Requested sample rate %d Hz is out of bounds [%d..%d] Hz.\n"
                   "Trying to resample to %d Hz.\n",
                   af->data->rate, BS2B_MINSRATE, BS2B_MAXSRATE, BS2B_DEFAULT_SRATE);
        }
        bs2b_set_srate(s->filter, (long)af->data->rate);
        MP_VERBOSE(af, "using format %s\n",
               af_fmt_to_str(af->data->format));

        return af_test_output(af,(struct mp_audio*)arg);
    }
    }
    return AF_UNKNOWN;
}

/// Deallocate memory and close library
static void uninit(struct af_instance *af)
{
    struct af_bs2b *s = af->priv;
    if (s->filter)
        bs2b_close(s->filter);
}

/// Allocate memory, set function pointers and init library
static int af_open(struct af_instance *af)
{
    struct af_bs2b *s = af->priv;
    af->control = control;
    af->uninit  = uninit;

    // NULL means failed initialization
    if (!(s->filter = bs2b_open())) {
        return AF_ERROR;
    }

    if (s->profile)
        bs2b_set_level(s->filter, s->profile);
    // set fcut and feed only if specified, otherwise defaults will be used
    if (s->fcut)
        bs2b_set_level_fcut(s->filter, s->fcut);
    if (s->feed)
        bs2b_set_level_feed(s->filter, s->feed);
    return AF_OK;
}

#define OPT_BASE_STRUCT struct af_bs2b

/// Description of this filter
const struct af_info af_info_bs2b = {
    .info = "Bauer stereophonic-to-binaural audio filter",
    .name = "bs2b",
    .open = af_open,
    .priv_size = sizeof(struct af_bs2b),
    .options = (const struct m_option[]) {
        OPT_INTRANGE("fcut", fcut, 0, BS2B_MINFCUT, BS2B_MAXFCUT),
        OPT_INTRANGE("feed", feed, 0, BS2B_MINFEED, BS2B_MAXFEED),
        OPT_CHOICE("profile", profile, 0,
                   ({"unset", 0},
                    {"default", BS2B_DEFAULT_CLEVEL},
                    {"cmoy", BS2B_CMOY_CLEVEL},
                    {"jmeier", BS2B_JMEIER_CLEVEL})),
        {0}
    },
};
