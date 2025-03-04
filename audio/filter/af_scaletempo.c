/*
 * scaletempo audio filter
 *
 * scale tempo while maintaining pitch
 * (WSOLA technique with taxicab distance)
 * inspired by SoundTouch library by Olli Parviainen
 *
 * basic algorithm
 *   - produce 'stride' output samples per loop
 *   - consume stride*scale input samples per loop
 *
 * to produce smoother transitions between strides, blend next overlap
 * samples from last stride with correlated samples of current input
 *
 * Copyright (c) 2007 Robert Juliano
 *
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

#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <math.h>

#include "audio/aframe.h"
#include "audio/format.h"
#include "common/common.h"
#include "filters/f_autoconvert.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_option.h"

struct f_opts {
    float scale_nominal;
    float ms_stride;
    float ms_search;
    float factor_overlap;
#define SCALE_TEMPO 1
#define SCALE_PITCH 2
    int speed_opt;
};

struct priv {
    struct f_opts *opts;

    struct mp_pin *in_pin;
    struct mp_aframe *cur_format;
    struct mp_aframe_pool *out_pool;
    double current_pts;
    struct mp_aframe *in;

    // stride
    float scale;
    float speed;
    int frames_stride;
    float frames_stride_scaled;
    float frames_stride_error;
    int bytes_per_frame;
    int bytes_stride;
    int bytes_queue;
    int bytes_queued;
    int bytes_to_slide;
    int8_t *buf_queue;
    // overlap
    int samples_overlap;
    int samples_standing;
    int bytes_overlap;
    int bytes_standing;
    void *buf_overlap;
    void *table_blend;
    void (*output_overlap)(struct priv *s, void *out_buf,
                           int bytes_off);
    // best overlap
    int frames_search;
    int num_channels;
    int (*best_overlap_offset)(struct priv *s);
};

static bool reinit(struct mp_filter *f);

// Return whether it got enough data for filtering.
static bool fill_queue(struct priv *s)
{
    int bytes_in = s->in ? mp_aframe_get_size(s->in) * s->bytes_per_frame : 0;
    int offset = 0;

    if (s->bytes_to_slide > 0) {
        if (s->bytes_to_slide < s->bytes_queued) {
            int bytes_move = s->bytes_queued - s->bytes_to_slide;
            memmove(s->buf_queue, s->buf_queue + s->bytes_to_slide, bytes_move);
            s->bytes_to_slide = 0;
            s->bytes_queued = bytes_move;
        } else {
            int bytes_skip;
            s->bytes_to_slide -= s->bytes_queued;
            bytes_skip = MPMIN(s->bytes_to_slide, bytes_in);
            s->bytes_queued = 0;
            s->bytes_to_slide -= bytes_skip;
            offset += bytes_skip;
            bytes_in -= bytes_skip;
        }
    }

    int bytes_needed = s->bytes_queue - s->bytes_queued;
    mp_assert(bytes_needed >= 0);

    int bytes_copy = MPMIN(bytes_needed, bytes_in);
    if (bytes_copy > 0) {
        uint8_t **planes = mp_aframe_get_data_ro(s->in);
        memcpy(s->buf_queue + s->bytes_queued, planes[0] + offset, bytes_copy);
        s->bytes_queued += bytes_copy;
        offset += bytes_copy;
        bytes_needed -= bytes_copy;
    }

    if (s->in)
        mp_aframe_skip_samples(s->in, offset / s->bytes_per_frame);

    return bytes_needed == 0;
}

// Fit the curve f(x) = a * x^2 + b * x + c such that
//   f(-1) = y[0]
//   f(0) = y[1]
//   f(1) = y[2]
// and return the extremum position and value
// assuming y[0] <= y[1] >= y[2] || y[0] >= y[1] <= y[2]
static void quadratic_interpolation_float(
    const float* y_values, float* x, float* value)
{
    const float b = (y_values[2] - y_values[0]) * 0.5f;
    const float c = y_values[1];
    const float a = y_values[0] + b - c;

    if (a == 0.f) {
        // it's a flat line
        *x = 0;
        *value = c;
    } else {
        const float pos = -b / (2.f * a);
        *x = pos;
        *value = a * pos * pos + b * pos + c;
    }
}

static void quadratic_interpolation_s16(
    const int32_t* y_values, float* x, int32_t* value)
{
    const float b = (y_values[2] - y_values[0]) * 0.5f;
    const float c = y_values[1];
    const float a = y_values[0] + b - c;

    if (a == 0.f) {
        // it's a flat line
        *x = 0;
        *value = c;
    } else {
        const float pos = -b / (2.f * a);
        *x = pos;
        *value = a * pos * pos + b * pos + c;
    }
}

static int best_overlap_offset_float(struct priv *s)
{
    int num_channels = s->num_channels, frames_search = s->frames_search;
    float *source = (float *)s->buf_queue + num_channels;
    float *target = (float *)s->buf_overlap + num_channels;
    int num_samples = s->samples_overlap - num_channels;
    int step_size = 3;
    float history[3] = {};

    float best_distance = FLT_MAX;
    int best_offset_approx = 0;
    for (int offset = 0; offset < frames_search; offset += step_size) {
        float distance = 0;
        for (int i = 0; i < num_samples; i++)
            distance += fabsf(target[i] - source[offset * num_channels + i]);

        int offset_approx = offset;
        history[0] = history[1];
        history[1] = history[2];
        history[2] = distance;
        if(offset >= 2 && history[0] >= history[1] && history[1] <= history[2]) {
            float extremum;
            quadratic_interpolation_float(history, &extremum, &distance);
            offset_approx = offset - step_size + (int)(extremum * step_size + 0.5f);
        }

        if (distance < best_distance) {
            best_distance = distance;
            best_offset_approx  = offset_approx;
        }
    }

    best_distance = FLT_MAX;
    int best_offset = 0;
    int min_offset = MPMAX(0, best_offset_approx - step_size + 1);
    int max_offset = MPMIN(frames_search, best_offset_approx + step_size);
    for (int offset = min_offset; offset < max_offset; offset++) {
        float distance = 0;
        for (int i = 0; i < num_samples; i++)
            distance += fabsf(target[i] - source[offset * num_channels + i]);
        if (distance < best_distance) {
            best_distance = distance;
            best_offset  = offset;
        }
    }

    return best_offset * 4 * num_channels;
}

static int best_overlap_offset_s16(struct priv *s)
{
    int num_channels = s->num_channels, frames_search = s->frames_search;
    int16_t *source = (int16_t *)s->buf_queue + num_channels;
    int16_t *target = (int16_t *)s->buf_overlap + num_channels;
    int num_samples = s->samples_overlap - num_channels;
    int step_size = 3;
    int32_t history[3] = {};

    int32_t best_distance = INT32_MAX;
    int best_offset_approx = 0;
    for (int offset = 0; offset < frames_search; offset += step_size) {
        int32_t distance = 0;
        for (int i = 0; i < num_samples; i++)
            distance += abs((int32_t)target[i] - source[offset * num_channels + i]);

        int offset_approx = offset;
        history[0] = history[1];
        history[1] = history[2];
        history[2] = distance;
        if(offset >= 2 && history[0] >= history[1] && history[1] <= history[2]) {
            float extremum;
            quadratic_interpolation_s16(history, &extremum, &distance);
            offset_approx = offset - step_size + (int)(extremum * step_size + 0.5f);
        }

        if (distance < best_distance) {
            best_distance = distance;
            best_offset_approx  = offset_approx;
        }
    }

    best_distance = INT32_MAX;
    int best_offset = 0;
    int min_offset = MPMAX(0, best_offset_approx - step_size + 1);
    int max_offset = MPMIN(frames_search, best_offset_approx + step_size);
    for (int offset = min_offset; offset < max_offset; offset++) {
        int32_t distance = 0;
        for (int i = 0; i < num_samples; i++)
            distance += abs((int32_t)target[i] - source[offset * num_channels + i]);
        if (distance < best_distance) {
            best_distance = distance;
            best_offset  = offset;
        }
    }

    return best_offset * 2 * s->num_channels;
}

static void output_overlap_float(struct priv *s, void *buf_out,
                                 int bytes_off)
{
    float *pout = buf_out;
    float *pb   = s->table_blend;
    float *po   = s->buf_overlap;
    float *pin  = (float *)(s->buf_queue + bytes_off);
    for (int i = 0; i < s->samples_overlap; i++) {
        // the math is equal to *po * (1 - *pb) + *pin * *pb
        float o = *po++;
        *pout++ = o - *pb++ * (o - *pin++);
    }
}

static void output_overlap_s16(struct priv *s, void *buf_out,
                               int bytes_off)
{
    int16_t *pout = buf_out;
    int32_t *pb   = s->table_blend;
    int16_t *po   = s->buf_overlap;
    int16_t *pin  = (int16_t *)(s->buf_queue + bytes_off);
    for (int i = 0; i < s->samples_overlap; i++) {
        // the math is equal to *po * (1 - *pb) + *pin * *pb
        int32_t o = *po++;
        *pout++ = o - ((*pb++ *(o - *pin++)) >> 16);
    }
}

static void af_scaletempo_process(struct mp_filter *f)
{
    struct priv *s = f->priv;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    struct mp_aframe *out = NULL;

    bool drain = false;
    bool is_eof = false;
    if (!s->in) {
        struct mp_frame frame = mp_pin_out_read(s->in_pin);
        if (!frame.type)
            return; // no input yet
        if (frame.type != MP_FRAME_AUDIO && frame.type != MP_FRAME_EOF) {
            MP_ERR(f, "unexpected frame type\n");
            goto error;
        }

        s->in = frame.type == MP_FRAME_AUDIO ? frame.data : NULL;
        is_eof = drain = !s->in;

        // EOF before it was even initialized once.
        if (is_eof && !mp_aframe_config_is_valid(s->cur_format)) {
            mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
            return;
        }

        if (s->in && !mp_aframe_config_equals(s->in, s->cur_format)) {
            if (s->bytes_queued) {
                // Drain remaining data before executing the format change.
                MP_VERBOSE(f, "draining\n");
                mp_pin_out_unread(s->in_pin, frame);
                s->in = NULL;
                drain = true;
            } else {
                if (!reinit(f)) {
                    MP_ERR(f, "initialization failed\n");
                    goto error;
                }
            }
        }

        if (s->in)
            s->current_pts = mp_aframe_end_pts(s->in);
    }

    if (!fill_queue(s) && !drain) {
        TA_FREEP(&s->in);
        mp_pin_out_request_data_next(s->in_pin);
        return;
    }

    int max_out_samples = s->bytes_stride / s->bytes_per_frame;
    if (drain)
        max_out_samples += s->bytes_queued;

    out = mp_aframe_new_ref(s->cur_format);
    if (mp_aframe_pool_allocate(s->out_pool, out, max_out_samples) < 0)
        goto error;

    if (s->in)
        mp_aframe_copy_attributes(out, s->in);

    uint8_t **out_planes = mp_aframe_get_data_rw(out);
    if (!out_planes)
        goto error;
    int8_t *pout = out_planes[0];
    int out_offset = 0;
    if (s->bytes_queued >= s->bytes_queue) {
        int ti;
        float tf;
        int bytes_off = 0;

        // output stride
        if (s->output_overlap) {
            if (s->best_overlap_offset)
                bytes_off = s->best_overlap_offset(s);
            s->output_overlap(s, pout + out_offset, bytes_off);
        }
        memcpy(pout + out_offset + s->bytes_overlap,
               s->buf_queue + bytes_off + s->bytes_overlap,
               s->bytes_standing);
        out_offset += s->bytes_stride;

        // input stride
        memcpy(s->buf_overlap,
               s->buf_queue + bytes_off + s->bytes_stride,
               s->bytes_overlap);
        tf = s->frames_stride_scaled + s->frames_stride_error;
        ti = (int)tf;
        s->frames_stride_error = tf - ti;
        s->bytes_to_slide = ti * s->bytes_per_frame;
    }
    // Drain remaining buffered data.
    if (drain && s->bytes_queued) {
        memcpy(pout + out_offset, s->buf_queue, s->bytes_queued);
        out_offset += s->bytes_queued;
        s->bytes_queued = 0;
    }
    mp_aframe_set_size(out, out_offset / s->bytes_per_frame);

    // This filter can have a negative delay when scale > 1:
    // output corresponding to some length of input can be decided and written
    // after receiving only a part of that input.
    float delay = (out_offset * s->speed + s->bytes_queued - s->bytes_to_slide) /
                    s->bytes_per_frame / mp_aframe_get_effective_rate(out)
                  + (s->in ? mp_aframe_duration(s->in) : 0);

    if (s->current_pts != MP_NOPTS_VALUE)
        mp_aframe_set_pts(out, s->current_pts - delay);

    mp_aframe_mul_speed(out, s->speed);

    if (!mp_aframe_get_size(out))
        TA_FREEP(&out);

    if (is_eof && out) {
        mp_pin_out_repeat_eof(s->in_pin);
    } else if (is_eof && !out) {
        mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
    } else if (!is_eof && !out) {
        mp_pin_out_request_data_next(s->in_pin);
    }

    if (out)
        mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_AUDIO, out));

    return;

error:
    TA_FREEP(&s->in);
    talloc_free(out);
    mp_filter_internal_mark_failed(f);
}

static void update_speed(struct priv *s, float speed)
{
    s->speed = speed;

    double factor = (s->opts->speed_opt & SCALE_PITCH) ? 1.0 / s->speed : s->speed;
    s->scale = factor * s->opts->scale_nominal;

    s->frames_stride_scaled = s->scale * s->frames_stride;
    s->frames_stride_error = MPMIN(s->frames_stride_error, s->frames_stride_scaled);
}

static bool reinit(struct mp_filter *f)
{
    struct priv *s = f->priv;

    mp_aframe_reset(s->cur_format);

    float srate  = mp_aframe_get_rate(s->in) / 1000.0;
    int nch = mp_aframe_get_channels(s->in);
    int format = mp_aframe_get_format(s->in);

    int use_int = 0;
    if (format == AF_FORMAT_S16) {
        use_int = 1;
    } else if (format != AF_FORMAT_FLOAT) {
        return false;
    }
    int bps = use_int ? 2 : 4;

    s->frames_stride        = srate * s->opts->ms_stride;
    s->bytes_stride         = s->frames_stride * bps * nch;

    update_speed(s, s->speed);

    int frames_overlap = s->frames_stride * s->opts->factor_overlap;
    if (frames_overlap <= 0) {
        s->bytes_standing   = s->bytes_stride;
        s->samples_standing = s->bytes_standing / bps;
        s->output_overlap   = NULL;
        s->bytes_overlap    = 0;
    } else {
        s->samples_overlap  = frames_overlap * nch;
        s->bytes_overlap    = frames_overlap * nch * bps;
        s->bytes_standing   = s->bytes_stride - s->bytes_overlap;
        s->samples_standing = s->bytes_standing / bps;
        s->buf_overlap      = realloc(s->buf_overlap, s->bytes_overlap);
        s->table_blend      = realloc(s->table_blend, s->bytes_overlap * 4);
        if (!s->buf_overlap || !s->table_blend) {
            MP_FATAL(f, "Out of memory\n");
            return false;
        }
        memset(s->buf_overlap, 0, s->bytes_overlap);
        if (use_int) {
            int32_t *pb = s->table_blend;
            const float scale = M_PI / frames_overlap;
            for (int i = 0; i < frames_overlap; i++) {
                // Hann function
                const int32_t v = 0.5f * (1.0f - cosf(i * scale)) * 65536 + 0.5;
                for (int j = 0; j < nch; j++)
                    *pb++ = v;
            }
            s->output_overlap = output_overlap_s16;
        } else {
            float *pb = s->table_blend;
            const float scale = M_PI / frames_overlap;
            for (int i = 0; i < frames_overlap; i++) {
                // Hann function
                const float v = 0.5f * (1.0f - cosf(i * scale));
                for (int j = 0; j < nch; j++)
                    *pb++ = v;
            }
            s->output_overlap = output_overlap_float;
        }
    }

    s->frames_search = (frames_overlap > 1) ? srate * s->opts->ms_search : 0;
    if (s->frames_search <= 0)
        s->best_overlap_offset = NULL;
    else {
        if (use_int) {
            s->best_overlap_offset = best_overlap_offset_s16;
        } else {
            s->best_overlap_offset = best_overlap_offset_float;
        }
    }

    s->bytes_per_frame = bps * nch;
    s->num_channels    = nch;

    s->bytes_queue = (s->frames_search + s->frames_stride + frames_overlap)
                        * bps * nch;
    s->buf_queue = realloc(s->buf_queue, s->bytes_queue);
    if (!s->buf_queue) {
        MP_FATAL(f, "Out of memory\n");
        return false;
    }

    s->bytes_queued = 0;
    s->bytes_to_slide = 0;

    MP_DBG(f, ""
           "%.2f stride_in, %i stride_out, %i standing, "
           "%i overlap, %i search, %i queue, %s mode\n",
           s->frames_stride_scaled,
           (int)(s->bytes_stride / nch / bps),
           (int)(s->bytes_standing / nch / bps),
           (int)(s->bytes_overlap / nch / bps),
           s->frames_search,
           (int)(s->bytes_queue / nch / bps),
           (use_int ? "s16" : "float"));

    mp_aframe_config_copy(s->cur_format, s->in);

    return true;
}

static bool af_scaletempo_command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct priv *s = f->priv;

    if (cmd->type == MP_FILTER_COMMAND_SET_SPEED) {
        if (s->opts->speed_opt & SCALE_TEMPO) {
            if (s->opts->speed_opt & SCALE_PITCH)
                return false;
            update_speed(s, cmd->speed);
            return true;
        } else if (s->opts->speed_opt & SCALE_PITCH) {
            update_speed(s, cmd->speed);
            return false; // do not signal OK
        }
    }

    return false;
}

static void af_scaletempo_reset(struct mp_filter *f)
{
    struct priv *s = f->priv;

    s->current_pts = MP_NOPTS_VALUE;
    s->bytes_queued = 0;
    s->bytes_to_slide = 0;
    s->frames_stride_error = 0;
    if (s->buf_overlap && s->bytes_overlap)
        memset(s->buf_overlap, 0, s->bytes_overlap);
    TA_FREEP(&s->in);
}

static void af_scaletempo_destroy(struct mp_filter *f)
{
    struct priv *s = f->priv;
    free(s->buf_queue);
    free(s->buf_overlap);
    free(s->table_blend);
    TA_FREEP(&s->in);
    mp_filter_free_children(f);
}

static const struct mp_filter_info af_scaletempo_filter = {
    .name = "scaletempo",
    .priv_size = sizeof(struct priv),
    .process = af_scaletempo_process,
    .command = af_scaletempo_command,
    .reset = af_scaletempo_reset,
    .destroy = af_scaletempo_destroy,
};

static struct mp_filter *af_scaletempo_create(struct mp_filter *parent,
                                              void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &af_scaletempo_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *s = f->priv;
    s->opts = talloc_steal(s, options);
    s->speed = 1.0;
    s->cur_format = talloc_steal(s, mp_aframe_create());
    s->out_pool = mp_aframe_pool_create(s);

    struct mp_autoconvert *conv = mp_autoconvert_create(f);
    if (!conv)
        abort();

    mp_autoconvert_add_afmt(conv, AF_FORMAT_S16);
    mp_autoconvert_add_afmt(conv, AF_FORMAT_FLOAT);

    mp_pin_connect(conv->f->pins[0], f->ppins[0]);
    s->in_pin = conv->f->pins[1];

    return f;
}

#define OPT_BASE_STRUCT struct f_opts

const struct mp_user_filter_entry af_scaletempo = {
    .desc = {
        .description = "Scale audio tempo while maintaining pitch",
        .name = "scaletempo",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .ms_stride = 60,
            .factor_overlap = .20,
            .ms_search = 14,
            .speed_opt = SCALE_TEMPO,
            .scale_nominal = 1.0,
        },
        .options = (const struct m_option[]) {
            {"scale", OPT_FLOAT(scale_nominal), M_RANGE(0.01, FLT_MAX)},
            {"stride", OPT_FLOAT(ms_stride), M_RANGE(0.01, FLT_MAX)},
            {"overlap", OPT_FLOAT(factor_overlap), M_RANGE(0, 1)},
            {"search", OPT_FLOAT(ms_search), M_RANGE(0, FLT_MAX)},
            {"speed", OPT_CHOICE(speed_opt,
                {"pitch", SCALE_PITCH},
                {"tempo", SCALE_TEMPO},
                {"none", 0},
                {"both", SCALE_TEMPO | SCALE_PITCH})},
            {0}
        },
    },
    .create = af_scaletempo_create,
};
