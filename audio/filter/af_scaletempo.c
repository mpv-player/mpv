/*
 * scaletempo audio filter
 *
 * scale tempo while maintaining pitch
 * (WSOLA technique with cross correlation)
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

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

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
    float percent_overlap;
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
    void *buf_pre_corr;
    void *table_window;
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
    assert(bytes_needed >= 0);

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

#define UNROLL_PADDING (4 * 4)

static int best_overlap_offset_float(struct priv *s)
{
    float best_corr = INT_MIN;
    int best_off = 0;

    float *pw  = s->table_window;
    float *po  = s->buf_overlap;
    po += s->num_channels;
    float *ppc = s->buf_pre_corr;
    for (int i = s->num_channels; i < s->samples_overlap; i++)
        *ppc++ = *pw++ **po++;

    float *search_start = (float *)s->buf_queue + s->num_channels;
    for (int off = 0; off < s->frames_search; off++) {
        float corr = 0;
        float *ps = search_start;
        ppc = s->buf_pre_corr;
        for (int i = s->num_channels; i < s->samples_overlap; i++)
            corr += *ppc++ **ps++;
        if (corr > best_corr) {
            best_corr = corr;
            best_off  = off;
        }
        search_start += s->num_channels;
    }

    return best_off * 4 * s->num_channels;
}

static int best_overlap_offset_s16(struct priv *s)
{
    int64_t best_corr = INT64_MIN;
    int best_off = 0;

    int32_t *pw  = s->table_window;
    int16_t *po  = s->buf_overlap;
    po += s->num_channels;
    int32_t *ppc = s->buf_pre_corr;
    for (long i = s->num_channels; i < s->samples_overlap; i++)
        *ppc++ = (*pw++ **po++) >> 15;

    int16_t *search_start = (int16_t *)s->buf_queue + s->num_channels;
    for (int off = 0; off < s->frames_search; off++) {
        int64_t corr = 0;
        int16_t *ps = search_start;
        ppc = s->buf_pre_corr;
        ppc += s->samples_overlap - s->num_channels;
        ps  += s->samples_overlap - s->num_channels;
        long i  = -(s->samples_overlap - s->num_channels);
        do {
            corr += ppc[i + 0] * ps[i + 0];
            corr += ppc[i + 1] * ps[i + 1];
            corr += ppc[i + 2] * ps[i + 2];
            corr += ppc[i + 3] * ps[i + 3];
            i += 4;
        } while (i < 0);
        if (corr > best_corr) {
            best_corr = corr;
            best_off  = off;
        }
        search_start += s->num_channels;
    }

    return best_off * 2 * s->num_channels;
}

static void output_overlap_float(struct priv *s, void *buf_out,
                                 int bytes_off)
{
    float *pout = buf_out;
    float *pb   = s->table_blend;
    float *po   = s->buf_overlap;
    float *pin  = (float *)(s->buf_queue + bytes_off);
    for (int i = 0; i < s->samples_overlap; i++) {
        *pout++ = *po - *pb++ *(*po - *pin++);
        po++;
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
        *pout++ = *po - ((*pb++ *(*po - *pin++)) >> 16);
        po++;
    }
}

static void process(struct mp_filter *f)
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

    int frames_overlap = s->frames_stride * s->opts->percent_overlap;
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
            int64_t blend = 0;
            for (int i = 0; i < frames_overlap; i++) {
                int32_t v = blend / frames_overlap;
                for (int j = 0; j < nch; j++)
                    *pb++ = v;
                blend += 65536; // 2^16
            }
            s->output_overlap = output_overlap_s16;
        } else {
            float *pb = s->table_blend;
            for (int i = 0; i < frames_overlap; i++) {
                float v = i / (float)frames_overlap;
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
            int64_t t = frames_overlap;
            int32_t n = 8589934588LL / (t * t); // 4 * (2^31 - 1) / t^2
            s->buf_pre_corr = realloc(s->buf_pre_corr,
                                        s->bytes_overlap * 2 + UNROLL_PADDING);
            s->table_window = realloc(s->table_window,
                                        s->bytes_overlap * 2 - nch * bps * 2);
            if (!s->buf_pre_corr || !s->table_window) {
                MP_FATAL(f, "Out of memory\n");
                return false;
            }
            memset((char *)s->buf_pre_corr + s->bytes_overlap * 2, 0,
                    UNROLL_PADDING);
            int32_t *pw = s->table_window;
            for (int i = 1; i < frames_overlap; i++) {
                int32_t v = (i * (t - i) * n) >> 15;
                for (int j = 0; j < nch; j++)
                    *pw++ = v;
            }
            s->best_overlap_offset = best_overlap_offset_s16;
        } else {
            s->buf_pre_corr = realloc(s->buf_pre_corr, s->bytes_overlap);
            s->table_window = realloc(s->table_window,
                                        s->bytes_overlap - nch * bps);
            if (!s->buf_pre_corr || !s->table_window) {
                MP_FATAL(f, "Out of memory\n");
                return false;
            }
            float *pw = s->table_window;
            for (int i = 1; i < frames_overlap; i++) {
                float v = i * (frames_overlap - i);
                for (int j = 0; j < nch; j++)
                    *pw++ = v;
            }
            s->best_overlap_offset = best_overlap_offset_float;
        }
    }

    s->bytes_per_frame = bps * nch;
    s->num_channels    = nch;

    s->bytes_queue = (s->frames_search + s->frames_stride + frames_overlap)
                        * bps * nch;
    s->buf_queue = realloc(s->buf_queue, s->bytes_queue + UNROLL_PADDING);
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

static bool command(struct mp_filter *f, struct mp_filter_command *cmd)
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

static void reset(struct mp_filter *f)
{
    struct priv *s = f->priv;

    s->current_pts = MP_NOPTS_VALUE;
    s->bytes_queued = 0;
    s->bytes_to_slide = 0;
    s->frames_stride_error = 0;
    memset(s->buf_overlap, 0, s->bytes_overlap);
    TA_FREEP(&s->in);
}

static void destroy(struct mp_filter *f)
{
    struct priv *s = f->priv;
    free(s->buf_queue);
    free(s->buf_overlap);
    free(s->buf_pre_corr);
    free(s->table_blend);
    free(s->table_window);
    TA_FREEP(&s->in);
    mp_filter_free_children(f);
}

static const struct mp_filter_info af_scaletempo_filter = {
    .name = "scaletempo",
    .priv_size = sizeof(struct priv),
    .process = process,
    .command = command,
    .reset = reset,
    .destroy = destroy,
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
            .percent_overlap = .20,
            .ms_search = 14,
            .speed_opt = SCALE_TEMPO,
            .scale_nominal = 1.0,
        },
        .options = (const struct m_option[]) {
            OPT_FLOAT("scale", scale_nominal, M_OPT_MIN, .min = 0.01),
            OPT_FLOAT("stride", ms_stride, M_OPT_MIN, .min = 0.01),
            OPT_FLOAT("overlap", percent_overlap, M_OPT_RANGE, .min = 0, .max = 1),
            OPT_FLOAT("search", ms_search, M_OPT_MIN, .min = 0),
            OPT_CHOICE("speed", speed_opt, 0,
                       ({"pitch", SCALE_PITCH},
                        {"tempo", SCALE_TEMPO},
                        {"none", 0},
                        {"both", SCALE_TEMPO | SCALE_PITCH})),
            {0}
        },
    },
    .create = af_scaletempo_create,
};
