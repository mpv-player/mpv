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
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "common/common.h"

#include "af.h"
#include "options/m_option.h"

// Data for specific instances of this filter
typedef struct af_scaletempo_s
{
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
    void (*output_overlap)(struct af_scaletempo_s *s, void *out_buf,
                           int bytes_off);
    // best overlap
    int frames_search;
    int num_channels;
    void *buf_pre_corr;
    void *table_window;
    int (*best_overlap_offset)(struct af_scaletempo_s *s);
    // command line
    float scale_nominal;
    float ms_stride;
    float percent_overlap;
    float ms_search;
#define SCALE_TEMPO 1
#define SCALE_PITCH 2
    int speed_opt;
} af_scaletempo_t;

static int fill_queue(struct af_instance *af, struct mp_audio *data, int offset)
{
    af_scaletempo_t *s = af->priv;
    int bytes_in = (data ? mp_audio_psize(data) : 0) - offset;
    int offset_unchanged = offset;

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

    if (bytes_in > 0) {
        int bytes_copy = MPMIN(s->bytes_queue - s->bytes_queued, bytes_in);
        assert(bytes_copy >= 0);
        memcpy(s->buf_queue + s->bytes_queued,
               (int8_t *)data->planes[0] + offset, bytes_copy);
        s->bytes_queued += bytes_copy;
        offset += bytes_copy;
    }

    return offset - offset_unchanged;
}

#define UNROLL_PADDING (4 * 4)

static int best_overlap_offset_float(af_scaletempo_t *s)
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

static int best_overlap_offset_s16(af_scaletempo_t *s)
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

static void output_overlap_float(af_scaletempo_t *s, void *buf_out,
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

static void output_overlap_s16(af_scaletempo_t *s, void *buf_out,
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

static int filter(struct af_instance *af, struct mp_audio *data)
{
    af_scaletempo_t *s = af->priv;

    if (s->scale == 1.0) {
        af->delay = 0;
        af_add_output_frame(af, data);
        return 0;
    }

    int in_samples = data ? data->samples : 0;
    struct mp_audio *out = mp_audio_pool_get(af->out_pool, af->data,
        ((int)(in_samples / s->frames_stride_scaled) + 1) * s->frames_stride);
    if (!out) {
        talloc_free(data);
        return -1;
    }
    if (data)
        mp_audio_copy_attributes(out, data);

    int offset_in = fill_queue(af, data, 0);
    int8_t *pout = out->planes[0];
    while (s->bytes_queued >= s->bytes_queue) {
        int ti;
        float tf;
        int bytes_off = 0;

        // output stride
        if (s->output_overlap) {
            if (s->best_overlap_offset)
                bytes_off = s->best_overlap_offset(s);
            s->output_overlap(s, pout, bytes_off);
        }
        memcpy(pout + s->bytes_overlap,
               s->buf_queue + bytes_off + s->bytes_overlap,
               s->bytes_standing);
        pout += s->bytes_stride;

        // input stride
        memcpy(s->buf_overlap,
               s->buf_queue + bytes_off + s->bytes_stride,
               s->bytes_overlap);
        tf = s->frames_stride_scaled + s->frames_stride_error;
        ti = (int)tf;
        s->frames_stride_error = tf - ti;
        s->bytes_to_slide = ti * s->bytes_per_frame;

        offset_in += fill_queue(af, data, offset_in);
    }

    // This filter can have a negative delay when scale > 1:
    // output corresponding to some length of input can be decided and written
    // after receiving only a part of that input.
    af->delay = (s->bytes_queued - s->bytes_to_slide) / s->scale
                / out->sstride / out->rate;

    out->samples = (pout - (int8_t *)out->planes[0]) / out->sstride;
    talloc_free(data);
    if (out->samples) {
        af_add_output_frame(af, out);
    } else {
        talloc_free(out);
    }
    return 0;
}

static void update_speed(struct af_instance *af, float speed)
{
    af_scaletempo_t *s = af->priv;

    s->speed = speed;

    double factor = (s->speed_opt & SCALE_PITCH) ? 1.0 / s->speed : s->speed;
    s->scale = factor * s->scale_nominal;

    s->frames_stride_scaled = s->scale * s->frames_stride;
    s->frames_stride_error = MPMIN(s->frames_stride_error, s->frames_stride_scaled);

    MP_VERBOSE(af, "%.3f speed * %.3f scale_nominal = %.3f\n",
               s->speed, s->scale_nominal, s->scale);
}

// Initialization and runtime control
static int control(struct af_instance *af, int cmd, void *arg)
{
    af_scaletempo_t *s = af->priv;
    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *data = (struct mp_audio *)arg;
        float srate  = data->rate / 1000.0;
        int nch = data->nch;
        int use_int = 0;

        mp_audio_force_interleaved_format(data);
        mp_audio_copy_config(af->data, data);

        if (data->format == AF_FORMAT_S16) {
            use_int = 1;
        } else {
            mp_audio_set_format(af->data, AF_FORMAT_FLOAT);
        }
        int bps = af->data->bps;

        s->frames_stride        = srate * s->ms_stride;
        s->bytes_stride         = s->frames_stride * bps * nch;
        af->delay = 0;

        update_speed(af, s->speed);

        int frames_overlap = s->frames_stride * s->percent_overlap;
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
                MP_FATAL(af, "Out of memory\n");
                return AF_ERROR;
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

        s->frames_search = (frames_overlap > 1) ? srate * s->ms_search : 0;
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
                    MP_FATAL(af, "Out of memory\n");
                    return AF_ERROR;
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
                    MP_FATAL(af, "Out of memory\n");
                    return AF_ERROR;
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
            MP_FATAL(af, "Out of memory\n");
            return AF_ERROR;
        }

        s->bytes_queued = 0;
        s->bytes_to_slide = 0;

        MP_DBG(af, ""
               "%.2f stride_in, %i stride_out, %i standing, "
               "%i overlap, %i search, %i queue, %s mode\n",
               s->frames_stride_scaled,
               (int)(s->bytes_stride / nch / bps),
               (int)(s->bytes_standing / nch / bps),
               (int)(s->bytes_overlap / nch / bps),
               s->frames_search,
               (int)(s->bytes_queue / nch / bps),
               (use_int ? "s16" : "float"));

        return af_test_output(af, (struct mp_audio *)arg);
    }
    case AF_CONTROL_SET_PLAYBACK_SPEED: {
        double speed = *(double *)arg;
        if (s->speed_opt & SCALE_TEMPO) {
            if (s->speed_opt & SCALE_PITCH)
                break;
            update_speed(af, speed);
        } else if (s->speed_opt & SCALE_PITCH) {
            update_speed(af, speed);
            break; // do not signal OK
        }
        return AF_OK;
    }
    case AF_CONTROL_RESET:
        s->bytes_queued = 0;
        s->bytes_to_slide = 0;
        s->frames_stride_error = 0;
        memset(s->buf_overlap, 0, s->bytes_overlap);
    }
    return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance *af)
{
    af_scaletempo_t *s = af->priv;
    free(s->buf_queue);
    free(s->buf_overlap);
    free(s->buf_pre_corr);
    free(s->table_blend);
    free(s->table_window);
}

// Allocate memory and set function pointers
static int af_open(struct af_instance *af)
{
    af->control   = control;
    af->uninit    = uninit;
    af->filter_frame = filter;
    return AF_OK;
}

#define OPT_BASE_STRUCT af_scaletempo_t

const struct af_info af_info_scaletempo = {
    .info = "Scale audio tempo while maintaining pitch",
    .name = "scaletempo",
    .open = af_open,
    .priv_size = sizeof(af_scaletempo_t),
    .priv_defaults = &(const af_scaletempo_t) {
        .ms_stride = 60,
        .percent_overlap = .20,
        .ms_search = 14,
        .speed_opt = SCALE_TEMPO,
        .speed = 1.0,
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
};
