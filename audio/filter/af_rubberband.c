/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <assert.h>

#include <rubberband/rubberband-c.h>

#include "audio/aframe.h"
#include "audio/format.h"
#include "common/common.h"
#include "filters/f_autoconvert.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_option.h"

// command line options
struct f_opts {
    int transients, detector, phase, window,
        smoothing, formant, pitch, channels;
    double scale;
};

struct priv {
    struct f_opts *opts;

    struct mp_pin *in_pin;
    struct mp_aframe *cur_format;
    struct mp_aframe_pool *out_pool;
    bool sent_final;
    RubberBandState rubber;
    double speed;
    double pitch;
    struct mp_aframe *pending;
    // Estimate how much librubberband has buffered internally.
    // I could not find a way to do this with the librubberband API.
    double rubber_delay;
};

static void update_speed(struct priv *p, double new_speed)
{
    p->speed = new_speed;
    if (p->rubber)
        rubberband_set_time_ratio(p->rubber, 1.0 / p->speed);
}

static bool update_pitch(struct priv *p, double new_pitch)
{
    if (new_pitch < 0.01 || new_pitch > 100.0)
        return false;

    p->pitch = new_pitch;
    if (p->rubber)
        rubberband_set_pitch_scale(p->rubber, p->pitch);
    return true;
}

static bool init_rubberband(struct mp_filter *f)
{
    struct priv *p = f->priv;

    assert(!p->rubber);
    assert(p->pending);

    int opts = p->opts->transients | p->opts->detector | p->opts->phase |
               p->opts->window | p->opts->smoothing | p->opts->formant |
               p->opts->pitch | p-> opts->channels |
               RubberBandOptionProcessRealTime;

    int rate = mp_aframe_get_rate(p->pending);
    int channels = mp_aframe_get_channels(p->pending);
    if (mp_aframe_get_format(p->pending) != AF_FORMAT_FLOATP)
        return false;

    p->rubber = rubberband_new(rate, channels, opts, 1.0, 1.0);
    if (!p->rubber) {
        MP_FATAL(f, "librubberband initialization failed.\n");
        return false;
    }

    mp_aframe_config_copy(p->cur_format, p->pending);

    update_speed(p, p->speed);
    update_pitch(p, p->pitch);

    return true;
}

static void process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    while (!p->rubber || !p->pending || rubberband_available(p->rubber) <= 0) {
        const float *dummy[MP_NUM_CHANNELS] = {0};
        const float **in_data = dummy;
        size_t in_samples = 0;

        bool eof = false;
        if (!p->pending || !mp_aframe_get_size(p->pending)) {
            struct mp_frame frame = mp_pin_out_read(p->in_pin);
            if (frame.type == MP_FRAME_AUDIO) {
                TA_FREEP(&p->pending);
                p->pending = frame.data;
            } else if (frame.type == MP_FRAME_EOF) {
                eof = true;
            } else if (frame.type) {
                MP_ERR(f, "unexpected frame type\n");
                goto error;
            } else {
                return; // no new data yet
            }
        }
        assert(p->pending || eof);

        if (!p->rubber) {
            if (!p->pending) {
                mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
                return;
            }
            if (!init_rubberband(f))
                goto error;
        }

        bool format_change =
            p->pending && !mp_aframe_config_equals(p->pending, p->cur_format);

        if (p->pending && !format_change) {
            size_t needs = rubberband_get_samples_required(p->rubber);
            uint8_t **planes = mp_aframe_get_data_ro(p->pending);
            int num_planes = mp_aframe_get_planes(p->pending);
            for (int n = 0; n < num_planes; n++)
                in_data[n] = (void *)planes[n];
            in_samples = MPMIN(mp_aframe_get_size(p->pending), needs);
        }

        bool final = format_change || eof;
        if (!p->sent_final)
            rubberband_process(p->rubber, in_data, in_samples, final);
        p->sent_final |= final;

        p->rubber_delay += in_samples;

        if (p->pending && !format_change)
            mp_aframe_skip_samples(p->pending, in_samples);

        if (rubberband_available(p->rubber) > 0) {
            if (eof)
                mp_pin_out_repeat_eof(p->in_pin); // drain more next time
        } else {
            if (eof) {
                mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
                rubberband_reset(p->rubber);
                p->rubber_delay = 0;
                TA_FREEP(&p->pending);
                p->sent_final = false;
                return;
            } else if (format_change) {
                // go on with proper reinit on the next iteration
                rubberband_delete(p->rubber);
                p->sent_final = false;
                p->rubber = NULL;
            }
        }
    }

    assert(p->pending);

    int out_samples = rubberband_available(p->rubber);
    if (out_samples > 0) {
        struct mp_aframe *out = mp_aframe_new_ref(p->cur_format);
        if (mp_aframe_pool_allocate(p->out_pool, out, out_samples) < 0) {
            talloc_free(out);
            goto error;
        }

        mp_aframe_copy_attributes(out, p->pending);

        float *out_data[MP_NUM_CHANNELS] = {0};
        uint8_t **planes = mp_aframe_get_data_rw(out);
        assert(planes);
        int num_planes = mp_aframe_get_planes(out);
        for (int n = 0; n < num_planes; n++)
            out_data[n] = (void *)planes[n];

        out_samples = rubberband_retrieve(p->rubber, out_data, out_samples);

        if (!out_samples) {
            mp_filter_internal_mark_progress(f); // unexpected, just try again
            talloc_free(out);
            return;
        }

        mp_aframe_set_size(out, out_samples);

        p->rubber_delay -= out_samples * p->speed;

        double pts = mp_aframe_get_pts(p->pending);
        if (pts != MP_NOPTS_VALUE) {
            // Note: rubberband_get_latency() does not do what you'd expect.
            double delay = p->rubber_delay / mp_aframe_get_effective_rate(out);
            mp_aframe_set_pts(out, pts - delay);
        }

        mp_aframe_mul_speed(out, p->speed);

        mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_AUDIO, out));
    }

    return;
error:
    mp_filter_internal_mark_failed(f);
}

static bool command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct priv *p = f->priv;

    switch (cmd->type) {
    case MP_FILTER_COMMAND_TEXT: {
        char *endptr = NULL;
        double pitch = p->pitch;
        if (!strcmp(cmd->cmd, "set-pitch")) {
            pitch = strtod(cmd->arg, &endptr);
            if (*endptr)
                return false;
            return update_pitch(p, pitch);
        } else if (!strcmp(cmd->cmd, "multiply-pitch")) {
            double mult = strtod(cmd->arg, &endptr);
            if (*endptr || mult <= 0)
                return false;
            pitch *= mult;
            return update_pitch(p, pitch);
        }
        return false;
    }
    case MP_FILTER_COMMAND_SET_SPEED:
        update_speed(p, cmd->speed);
        return true;
    }

    return false;
}

static void reset(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (p->rubber)
        rubberband_reset(p->rubber);
    p->rubber_delay = 0;
    p->sent_final = false;
    TA_FREEP(&p->pending);
}

static void destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (p->rubber)
        rubberband_delete(p->rubber);
    talloc_free(p->pending);
}

static const struct mp_filter_info af_rubberband_filter = {
    .name = "rubberband",
    .priv_size = sizeof(struct priv),
    .process = process,
    .command = command,
    .reset = reset,
    .destroy = destroy,
};

static struct mp_filter *af_rubberband_create(struct mp_filter *parent,
                                              void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &af_rubberband_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);
    p->speed = 1.0;
    p->pitch = p->opts->scale;
    p->cur_format = talloc_steal(p, mp_aframe_create());
    p->out_pool = mp_aframe_pool_create(p);

    struct mp_autoconvert *conv = mp_autoconvert_create(f);
    if (!conv)
        abort();

    mp_autoconvert_add_afmt(conv, AF_FORMAT_FLOATP);

    mp_pin_connect(conv->f->pins[0], f->ppins[0]);
    p->in_pin = conv->f->pins[1];

    return f;
}

#define OPT_BASE_STRUCT struct f_opts

const struct mp_user_filter_entry af_rubberband = {
    .desc = {
        .description = "Pitch conversion with librubberband",
        .name = "rubberband",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .scale = 1.0,
            .pitch = RubberBandOptionPitchHighConsistency,
            .transients = RubberBandOptionTransientsMixed,
            .formant = RubberBandOptionFormantPreserved,
            .channels = RubberBandOptionChannelsTogether,
        },
        .options = (const struct m_option[]) {
            {"transients", OPT_CHOICE(transients,
                {"crisp", RubberBandOptionTransientsCrisp},
                {"mixed", RubberBandOptionTransientsMixed},
                {"smooth", RubberBandOptionTransientsSmooth})},
            {"detector", OPT_CHOICE(detector,
                {"compound", RubberBandOptionDetectorCompound},
                {"percussive", RubberBandOptionDetectorPercussive},
                {"soft", RubberBandOptionDetectorSoft})},
            {"phase", OPT_CHOICE(phase,
                {"laminar", RubberBandOptionPhaseLaminar},
                {"independent", RubberBandOptionPhaseIndependent})},
            {"window", OPT_CHOICE(window,
                {"standard", RubberBandOptionWindowStandard},
                {"short", RubberBandOptionWindowShort},
                {"long", RubberBandOptionWindowLong})},
            {"smoothing", OPT_CHOICE(smoothing,
                {"off", RubberBandOptionSmoothingOff},
                {"on", RubberBandOptionSmoothingOn})},
            {"formant", OPT_CHOICE(formant,
                {"shifted", RubberBandOptionFormantShifted},
                {"preserved", RubberBandOptionFormantPreserved})},
            {"pitch", OPT_CHOICE(pitch,
                {"quality", RubberBandOptionPitchHighQuality},
                {"speed", RubberBandOptionPitchHighSpeed},
                {"consistency", RubberBandOptionPitchHighConsistency})},
            {"channels", OPT_CHOICE(channels,
                {"apart", RubberBandOptionChannelsApart},
                {"together", RubberBandOptionChannelsTogether})},
            {"pitch-scale", OPT_DOUBLE(scale), M_RANGE(0.01, 100)},
            {0}
        },
    },
    .create = af_rubberband_create,
};
