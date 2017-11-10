/*
 * Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
 *
 * Based on Michael Niedermayer's lavcresample.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

#include "common/common.h"
#include "config.h"

#include "common/av_common.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "audio/filter/af.h"
#include "audio/fmt-conversion.h"
#include "osdep/endian.h"
#include "audio/aconverter.h"

struct af_resample {
    int allow_detach;
    double playback_speed;
    struct mp_resample_opts opts;
    struct mp_aconverter *converter;
};

static int control(struct af_instance *af, int cmd, void *arg)
{
    struct af_resample *s = af->priv;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio *out = af->data;
        struct mp_audio orig_in = *in;

        if (((out->rate    == in->rate) || (out->rate == 0)) &&
            (out->format   == in->format) &&
            (mp_chmap_equals(&out->channels, &in->channels) || out->nch == 0) &&
            s->allow_detach && s->playback_speed == 1.0)
            return AF_DETACH;

        if (out->rate == 0)
            out->rate = in->rate;

        if (mp_chmap_is_empty(&out->channels))
            mp_audio_set_channels(out, &in->channels);

        if (af_to_avformat(in->format) == AV_SAMPLE_FMT_NONE)
            mp_audio_set_format(in, AF_FORMAT_FLOAT);
        if (af_to_avformat(out->format) == AV_SAMPLE_FMT_NONE)
            mp_audio_set_format(out, in->format);

        int r = ((in->format == orig_in.format) &&
                mp_chmap_equals(&in->channels, &orig_in.channels))
                ? AF_OK : AF_FALSE;

        if (r == AF_OK) {
            if (!mp_aconverter_reconfig(s->converter,
                    in->rate, in->format, in->channels,
                    out->rate, out->format, out->channels))
                r = AF_ERROR;
        }
        return r;
    }
    case AF_CONTROL_SET_PLAYBACK_SPEED_RESAMPLE: {
        s->playback_speed = *(double *)arg;
        return AF_OK;
    }
    case AF_CONTROL_RESET:
        mp_aconverter_flush(s->converter);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

static void uninit(struct af_instance *af)
{
    struct af_resample *s = af->priv;

    talloc_free(s->converter);
}

static int filter(struct af_instance *af, struct mp_audio *in)
{
    struct af_resample *s = af->priv;

    mp_aconverter_set_speed(s->converter, s->playback_speed);

    af->filter_out(af);

    struct mp_aframe *aframe = mp_audio_to_aframe(in);
    if (!aframe && in)
        return -1;
    talloc_free(in);
    bool ok = mp_aconverter_write_input(s->converter, aframe);
    if (!ok)
        talloc_free(aframe);

    return ok ? 0 : -1;
}

static int filter_out(struct af_instance *af)
{
    struct af_resample *s = af->priv;
    bool eof;
    struct mp_aframe *out = mp_aconverter_read_output(s->converter, &eof);
    if (out)
        af_add_output_frame(af, mp_audio_from_aframe(out));
    talloc_free(out);
    af->delay = mp_aconverter_get_latency(s->converter);
    return 0;
}

static int af_open(struct af_instance *af)
{
    struct af_resample *s = af->priv;

    af->control = control;
    af->uninit  = uninit;
    af->filter_frame = filter;
    af->filter_out = filter_out;

    s->converter = mp_aconverter_create(af->global, af->log, &s->opts);

    return AF_OK;
}

#define OPT_BASE_STRUCT struct af_resample

const struct af_info af_info_lavrresample = {
    .info = "Sample frequency conversion using libavresample",
    .name = "lavrresample",
    .open = af_open,
    .priv_size = sizeof(struct af_resample),
    .priv_defaults = &(const struct af_resample) {
        .opts = MP_RESAMPLE_OPTS_DEF,
        .playback_speed = 1.0,
        .allow_detach = 1,
    },
    .options = (const struct m_option[]) {
        OPT_INTRANGE("filter-size", opts.filter_size, 0, 0, 32),
        OPT_INTRANGE("phase-shift", opts.phase_shift, 0, 0, 30),
        OPT_FLAG("linear", opts.linear, 0),
        OPT_DOUBLE("cutoff", opts.cutoff, M_OPT_RANGE, .min = 0, .max = 1),
        OPT_FLAG("detach", allow_detach, 0),
        OPT_CHOICE("normalize", opts.normalize, 0,
                   ({"no", 0}, {"yes", 1}, {"auto", -1})),
        OPT_KEYVALUELIST("o", opts.avopts, 0),
        {0}
    },
};
