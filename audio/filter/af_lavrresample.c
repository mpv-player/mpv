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

#include <libavutil/opt.h>
#include <libavutil/audioconvert.h>
#include <libavutil/common.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>

#include "common/common.h"
#include "config.h"

#if HAVE_LIBAVRESAMPLE
#include <libavresample/avresample.h>
#elif HAVE_LIBSWRESAMPLE
#include <libswresample/swresample.h>
#define AVAudioResampleContext SwrContext
#define avresample_alloc_context swr_alloc
#define avresample_open swr_init
#define avresample_close(x) do { } while(0)
#define avresample_free swr_free
#define avresample_available(x) 0
#define avresample_convert(ctx, out, out_planesize, out_samples, in, in_planesize, in_samples) \
    swr_convert(ctx, out, out_samples, (const uint8_t**)(in), in_samples)
#define avresample_set_channel_mapping swr_set_channel_mapping
#else
#error "config.h broken or no resampler found"
#endif

#include "common/av_common.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "audio/filter/af.h"
#include "audio/fmt-conversion.h"

struct af_resample_opts {
    int filter_size;
    int phase_shift;
    int linear;
    double cutoff;

    int in_rate_af; // filter input sample rate
    int in_rate;    // actual rate (used by lavr), adjusted for playback speed
    int in_format;
    struct mp_chmap in_channels;
    int out_rate;
    int out_format;
    struct mp_chmap out_channels;
};

struct af_resample {
    int allow_detach;
    char **avopts;
    double playback_speed;
    struct mp_audio *pending;
    bool avrctx_ok;
    struct AVAudioResampleContext *avrctx;
    struct mp_audio avrctx_fmt; // output format of avrctx
    struct AVAudioResampleContext *avrctx_out; // for output channel reordering
    struct af_resample_opts ctx;   // opts in the context
    struct af_resample_opts opts;  // opts requested by the user
    // At least libswresample keeps a pointer around for this:
    int reorder_in[MP_NUM_CHANNELS];
    int reorder_out[MP_NUM_CHANNELS];
    struct mp_audio_pool *reorder_buffer;
};

#if HAVE_LIBAVRESAMPLE
static int get_delay(struct af_resample *s)
{
    return avresample_get_delay(s->avrctx);
}
static void drop_all_output(struct af_resample *s)
{
    while (avresample_read(s->avrctx, NULL, 1000) > 0) {}
}
static int get_drain_samples(struct af_resample *s)
{
    return avresample_get_out_samples(s->avrctx, 0);
}
#else
static int get_delay(struct af_resample *s)
{
    return swr_get_delay(s->avrctx, s->ctx.in_rate);
}
static void drop_all_output(struct af_resample *s)
{
    while (swr_drop_output(s->avrctx, 1000) > 0) {}
}
static int get_drain_samples(struct af_resample *s)
{
    return 4096; // libswscale does not have this
}
#endif

static int resample_frame(struct AVAudioResampleContext *r,
                          struct mp_audio *out, struct mp_audio *in)
{
    return avresample_convert(r,
        out ? (uint8_t **)out->planes : NULL,
        out ? mp_audio_get_allocated_size(out) : 0,
        out ? out->samples : 0,
        in ? (uint8_t **)in->planes : NULL,
        in ? mp_audio_get_allocated_size(in) : 0,
        in ? in->samples : 0);
}

static double af_resample_default_cutoff(int filter_size)
{
    return FFMAX(1.0 - 6.5 / (filter_size + 8), 0.80);
}

static int rate_from_speed(int rate, double speed)
{
    return lrint(rate * speed);
}

static bool needs_lavrctx_reconfigure(struct af_resample *s,
                                      struct mp_audio *in,
                                      struct mp_audio *out)
{
    return s->ctx.in_rate_af  != in->rate ||
           s->ctx.in_format   != in->format ||
           !mp_chmap_equals(&s->ctx.in_channels, &in->channels) ||
           s->ctx.out_rate    != out->rate ||
           s->ctx.out_format  != out->format ||
           !mp_chmap_equals(&s->ctx.out_channels, &out->channels) ||
           s->ctx.filter_size != s->opts.filter_size ||
           s->ctx.phase_shift != s->opts.phase_shift ||
           s->ctx.linear      != s->opts.linear ||
           s->ctx.cutoff      != s->opts.cutoff;

}

static bool test_conversion(int src_format, int dst_format)
{
    return af_to_avformat(src_format) != AV_SAMPLE_FMT_NONE &&
           af_to_avformat(dst_format) != AV_SAMPLE_FMT_NONE;
}

// mp_chmap_get_reorder() performs:
//  to->speaker[n] = from->speaker[src[n]]
// but libavresample does:
//  to->speaker[dst[n]] = from->speaker[n]
static void transpose_order(int *map, int num)
{
    int nmap[MP_NUM_CHANNELS] = {0};
    for (int n = 0; n < num; n++) {
        for (int i = 0; i < num; i++) {
            if (map[n] == i)
                nmap[i] = n;
        }
    }
    memcpy(map, nmap, sizeof(nmap));
}

static int configure_lavrr(struct af_instance *af, struct mp_audio *in,
                           struct mp_audio *out)
{
    struct af_resample *s = af->priv;

    s->avrctx_ok = false;

    enum AVSampleFormat in_samplefmt = af_to_avformat(in->format);
    enum AVSampleFormat out_samplefmt = af_to_avformat(out->format);
    enum AVSampleFormat out_samplefmtp =
        af_to_avformat(af_fmt_to_planar(out->format));

    if (in_samplefmt == AV_SAMPLE_FMT_NONE ||
        out_samplefmt == AV_SAMPLE_FMT_NONE ||
        out_samplefmtp == AV_SAMPLE_FMT_NONE)
        return AF_ERROR;

    avresample_close(s->avrctx);
    avresample_close(s->avrctx_out);

    talloc_free(s->pending);
    s->pending = NULL;

    s->ctx.out_rate    = out->rate;
    s->ctx.in_rate_af  = in->rate;
    s->ctx.in_rate     = rate_from_speed(in->rate, s->playback_speed);
    s->ctx.out_format  = out->format;
    s->ctx.in_format   = in->format;
    s->ctx.out_channels= out->channels;
    s->ctx.in_channels = in->channels;
    s->ctx.filter_size = s->opts.filter_size;
    s->ctx.phase_shift = s->opts.phase_shift;
    s->ctx.linear      = s->opts.linear;
    s->ctx.cutoff      = s->opts.cutoff;

    av_opt_set_int(s->avrctx, "filter_size",        s->ctx.filter_size, 0);
    av_opt_set_int(s->avrctx, "phase_shift",        s->ctx.phase_shift, 0);
    av_opt_set_int(s->avrctx, "linear_interp",      s->ctx.linear, 0);

    av_opt_set_double(s->avrctx, "cutoff",          s->ctx.cutoff, 0);

#if HAVE_LIBSWRESAMPLE
    av_opt_set_double(s->avrctx, "rematrix_maxval", 1.0, 0);
#endif

    if (mp_set_avopts(af->log, s->avrctx, s->avopts) < 0)
        return AF_ERROR;

    struct mp_chmap map_in = in->channels;
    struct mp_chmap map_out = out->channels;

    // Try not to do any remixing if at least one is "unknown".
    if (mp_chmap_is_unknown(&map_in) || mp_chmap_is_unknown(&map_out)) {
        mp_chmap_set_unknown(&map_in, map_in.num);
        mp_chmap_set_unknown(&map_out, map_out.num);
    }

    // unchecked: don't take any channel reordering into account
    uint64_t in_ch_layout = mp_chmap_to_lavc_unchecked(&map_in);
    uint64_t out_ch_layout = mp_chmap_to_lavc_unchecked(&map_out);

    struct mp_chmap in_lavc;
    mp_chmap_from_lavc(&in_lavc, in_ch_layout);
    if (in_lavc.num != map_in.num) {
        // For handling NA channels, we would have to add a planarization step.
        MP_FATAL(af, "Unsupported channel remapping.\n");
        return AF_ERROR;
    }

    mp_chmap_get_reorder(s->reorder_in, &map_in, &in_lavc);
    transpose_order(s->reorder_in, map_in.num);

    struct mp_chmap out_lavc;
    mp_chmap_from_lavc(&out_lavc, out_ch_layout);
    if (mp_chmap_equals(&out_lavc, &map_out)) {
        // No intermediate step required - output new format directly.
        out_samplefmtp = out_samplefmt;
    } else {
        // Verify that we really just reorder and/or insert NA channels.
        struct mp_chmap withna = out_lavc;
        mp_chmap_fill_na(&withna, map_out.num);
        if (withna.num != map_out.num)
            return AF_ERROR;
        mp_chmap_get_reorder(s->reorder_out, &out_lavc, &map_out);
    }

    s->avrctx_fmt = *out;
    mp_audio_set_channels(&s->avrctx_fmt, &out_lavc);
    mp_audio_set_format(&s->avrctx_fmt, af_from_avformat(out_samplefmtp));

    // Real conversion; output is input to avrctx_out.
    av_opt_set_int(s->avrctx, "in_channel_layout",  in_ch_layout, 0);
    av_opt_set_int(s->avrctx, "out_channel_layout", out_ch_layout, 0);
    av_opt_set_int(s->avrctx, "in_sample_rate",     s->ctx.in_rate, 0);
    av_opt_set_int(s->avrctx, "out_sample_rate",    s->ctx.out_rate, 0);
    av_opt_set_int(s->avrctx, "in_sample_fmt",      in_samplefmt, 0);
    av_opt_set_int(s->avrctx, "out_sample_fmt",     out_samplefmtp, 0);

    // Just needs the correct number of channels.
    int fake_out_ch_layout = av_get_default_channel_layout(map_out.num);
    if (!fake_out_ch_layout)
        return AF_ERROR;

    // Deplanarize if needed.
    av_opt_set_int(s->avrctx_out, "in_channel_layout",  fake_out_ch_layout, 0);
    av_opt_set_int(s->avrctx_out, "out_channel_layout", fake_out_ch_layout, 0);
    av_opt_set_int(s->avrctx_out, "in_sample_fmt",      out_samplefmtp, 0);
    av_opt_set_int(s->avrctx_out, "out_sample_fmt",     out_samplefmt, 0);
    av_opt_set_int(s->avrctx_out, "in_sample_rate",     s->ctx.out_rate, 0);
    av_opt_set_int(s->avrctx_out, "out_sample_rate",    s->ctx.out_rate, 0);

    // API has weird requirements, quoting avresample.h:
    //  * This function can only be called when the allocated context is not open.
    //  * Also, the input channel layout must have already been set.
    avresample_set_channel_mapping(s->avrctx, s->reorder_in);

    if (avresample_open(s->avrctx) < 0 ||
        avresample_open(s->avrctx_out) < 0)
    {
        MP_ERR(af, "Cannot open Libavresample Context. \n");
        return AF_ERROR;
    }
    s->avrctx_ok = true;
    return AF_OK;
}


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

        if (r == AF_OK && needs_lavrctx_reconfigure(s, in, out))
            r = configure_lavrr(af, in, out);
        return r;
    }
    case AF_CONTROL_SET_FORMAT: {
        int format = *(int *)arg;
        if (format && af_to_avformat(format) == AV_SAMPLE_FMT_NONE)
            return AF_FALSE;

        mp_audio_set_format(af->data, format);
        return AF_OK;
    }
    case AF_CONTROL_SET_CHANNELS: {
        mp_audio_set_channels(af->data, (struct mp_chmap *)arg);
        return AF_OK;
    }
    case AF_CONTROL_SET_RESAMPLE_RATE:
        af->data->rate = *(int *)arg;
        return AF_OK;
    case AF_CONTROL_SET_PLAYBACK_SPEED_RESAMPLE: {
        s->playback_speed = *(double *)arg;
        int new_rate = rate_from_speed(s->ctx.in_rate_af, s->playback_speed);
        if (new_rate != s->ctx.in_rate && s->avrctx_ok && af->fmt_out.format) {
            // Before reconfiguring, drain the audio that is still buffered
            // in the resampler.
            struct mp_audio *pending = talloc_zero(NULL, struct mp_audio);
            mp_audio_copy_config(pending, &af->fmt_out);
            pending->samples = get_drain_samples(s);
            if (pending->samples > 0) {
                mp_audio_realloc_min(pending, pending->samples);
                int r = resample_frame(s->avrctx, pending, NULL);
                pending->samples = MPMAX(r, 0);
            }
            // Reinitialize resampler.
            configure_lavrr(af, &af->fmt_in, &af->fmt_out);
            s->pending = pending;
        }
        return AF_OK;
    }
    case AF_CONTROL_RESET:
        drop_all_output(s);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

#undef ctx_opt_set_int
#undef ctx_opt_set_dbl

static void uninit(struct af_instance *af)
{
    struct af_resample *s = af->priv;
    if (s->avrctx)
        avresample_close(s->avrctx);
    avresample_free(&s->avrctx);
    if (s->avrctx_out)
        avresample_close(s->avrctx_out);
    avresample_free(&s->avrctx_out);
    talloc_free(s->pending);
}

static void reorder_planes(struct mp_audio *mpa, int *reorder,
                           struct mp_chmap *newmap)
{
    struct mp_audio prev = *mpa;

    for (int n = 0; n < newmap->num; n++) {
        int src = reorder[n];
        if (src < 0) {
            src = 0;    // Use the first plane for padding channels.
            mpa->readonly = true;
        }
        assert(src < mpa->num_planes);
        mpa->planes[n] = prev.planes[src];
    }

    mp_audio_set_channels(mpa, newmap);
}

static int filter(struct af_instance *af, struct mp_audio *in)
{
    struct af_resample *s = af->priv;

    if (s->pending) {
        if (s->pending->samples) {
            af_add_output_frame(af, s->pending);
        } else {
            talloc_free(s->pending);
        }
        s->pending = NULL;
    }

    int samples = avresample_available(s->avrctx) +
        av_rescale_rnd(get_delay(s) + (in ? in->samples : 0),
                       s->ctx.out_rate, s->ctx.in_rate, AV_ROUND_UP);

    struct mp_audio out_format = s->avrctx_fmt;
    struct mp_audio *out = mp_audio_pool_get(af->out_pool, &out_format, samples);
    if (!out)
        goto error;
    if (in)
        mp_audio_copy_attributes(out, in);

    af->delay = get_delay(s) / (double)s->ctx.in_rate;

    if (out->samples) {
        out->samples = resample_frame(s->avrctx, out, in);
        if (out->samples < 0)
            goto error;
    }

    if (out->samples && !mp_audio_config_equals(out, af->data)) {
        assert(AF_FORMAT_IS_PLANAR(out->format));
        reorder_planes(out, s->reorder_out, &af->data->channels);
        if (!mp_audio_config_equals(out, af->data)) {
            struct mp_audio *new = mp_audio_pool_get(s->reorder_buffer, af->data,
                                                     out->samples);
            if (!new)
                goto error;
            mp_audio_copy_attributes(new, out);
            int out_samples = resample_frame(s->avrctx_out, new, out);
            talloc_free(out);
            out = new;
            if (out_samples != new->samples)
                goto error;
        }
    }

    talloc_free(in);
    if (out->samples) {
        af_add_output_frame(af, out);
    } else {
        talloc_free(out);
    }
    return 0;
error:
    talloc_free(in);
    talloc_free(out);
    return -1;
}

static int af_open(struct af_instance *af)
{
    struct af_resample *s = af->priv;

    af->control = control;
    af->uninit  = uninit;
    af->filter_frame = filter;

    if (s->opts.cutoff <= 0.0)
        s->opts.cutoff = af_resample_default_cutoff(s->opts.filter_size);

    s->avrctx = avresample_alloc_context();
    s->avrctx_out = avresample_alloc_context();
    s->reorder_buffer = mp_audio_pool_create(s);

    if (s->avrctx && s->avrctx_out) {
        return AF_OK;
    } else {
        MP_ERR(af, "Cannot initialize Libavresample Context. \n");
        uninit(af);
        return AF_ERROR;
    }
}

#define OPT_BASE_STRUCT struct af_resample

const struct af_info af_info_lavrresample = {
    .info = "Sample frequency conversion using libavresample",
    .name = "lavrresample",
    .open = af_open,
    .test_conversion = test_conversion,
    .priv_size = sizeof(struct af_resample),
    .priv_defaults = &(const struct af_resample) {
        .opts = {
            .filter_size = 16,
            .cutoff      = 0.0,
            .phase_shift = 10,
        },
        .playback_speed = 1.0,
        .allow_detach = 1,
    },
    .options = (const struct m_option[]) {
        OPT_INTRANGE("filter-size", opts.filter_size, 0, 0, 32),
        OPT_INTRANGE("phase-shift", opts.phase_shift, 0, 0, 30),
        OPT_FLAG("linear", opts.linear, 0),
        OPT_DOUBLE("cutoff", opts.cutoff, M_OPT_RANGE, .min = 0, .max = 1),
        OPT_FLAG("detach", allow_detach, 0),
        OPT_KEYVALUELIST("o", avopts, 0),
        {0}
    },
};
