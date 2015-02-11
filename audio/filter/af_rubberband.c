/*
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

#include <stdlib.h>
#include <assert.h>

#include <rubberband/rubberband-c.h>

#include "common/common.h"
#include "af.h"

struct priv {
    RubberBandState rubber;
    double speed;
    struct mp_audio *pending;
    bool needs_reset;
};

static void update_speed(struct af_instance *af, double new_speed)
{
    struct priv *p = af->priv;

    p->speed = new_speed;
    rubberband_set_time_ratio(p->rubber, 1.0 / p->speed);
}

static int control(struct af_instance *af, int cmd, void *arg)
{
    struct priv *p = af->priv;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio orig_in = *in;
        struct mp_audio *out = af->data;

        in->format = AF_FORMAT_FLOATP;
        mp_audio_copy_config(out, in);

        if (p->rubber)
            rubberband_delete(p->rubber);

        int opts = RubberBandOptionProcessRealTime
                 | RubberBandOptionStretchPrecise
                 | RubberBandOptionSmoothingOn
                 | RubberBandOptionPitchHighConsistency;

        p->rubber = rubberband_new(in->rate, in->channels.num, opts, 1.0, 1.0);
        if (!p->rubber) {
            MP_FATAL(af, "librubberband initialization failed.\n");
            return AF_ERROR;
        }

        update_speed(af, p->speed);

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    case AF_CONTROL_SET_PLAYBACK_SPEED: {
        update_speed(af, *(double *)arg);
        return AF_OK;
    }
    case AF_CONTROL_RESET:
        if (p->rubber)
            rubberband_reset(p->rubber);
        talloc_free(p->pending);
        p->pending = NULL;
        return AF_OK;
    }
    return AF_UNKNOWN;
}

static int filter_frame(struct af_instance *af, struct mp_audio *data)
{
    struct priv *p = af->priv;

    talloc_free(p->pending);
    p->pending = data;

    return 0;
}

static int filter_out(struct af_instance *af)
{
    struct priv *p = af->priv;

    while (!rubberband_available(p->rubber)) {
        const float *dummy[MP_NUM_CHANNELS] = {0};
        const float **in_data = dummy;
        size_t in_samples = 0;
        if (p->pending) {
            if (!p->pending->samples)
                break;

            // recover from previous EOF
            if (p->needs_reset)
                rubberband_reset(p->rubber);
            p->needs_reset = false;

            size_t needs = rubberband_get_samples_required(p->rubber);
            in_data = (void *)&p->pending->planes;
            in_samples = MPMIN(p->pending->samples, needs);
        }

        if (p->needs_reset)
            break; // previous EOF
        p->needs_reset = !p->pending; // EOF

        rubberband_process(p->rubber, in_data, in_samples, p->needs_reset);
        if (!p->pending)
            break;
        mp_audio_skip_samples(p->pending, in_samples);
    }

    size_t out_samples = rubberband_available(p->rubber);
    if (!out_samples)
        return 0;

    struct mp_audio *out =
        mp_audio_pool_get(af->out_pool, af->data, out_samples);
    if (!out)
        return -1;
    if (p->pending)
        mp_audio_copy_config(out, p->pending);
    float **out_data = (void *)&out->planes;
    out->samples = rubberband_retrieve(p->rubber, out_data, out->samples);
    af_add_output_frame(af, out);

    int delay = rubberband_get_latency(p->rubber);
    delay += p->pending ? p->pending->samples : 0;
    af->delay = delay / (double)af->data->rate;

    return 0;
}

static void uninit(struct af_instance *af)
{
    struct priv *p = af->priv;

    if (p->rubber)
        rubberband_delete(p->rubber);
    talloc_free(p->pending);
}

static int af_open(struct af_instance *af)
{
    af->control = control;
    af->filter_frame = filter_frame;
    af->filter_out = filter_out;
    af->uninit = uninit;
    return AF_OK;
}

const struct af_info af_info_rubberband = {
    .info = "Pitch conversion with librubberband",
    .name = "rubberband",
    .open = af_open,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .speed = 1.0,
    },
};
