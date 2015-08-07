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
#include "osdep/endian.h"

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
    struct AVAudioResampleContext *avrctx;
    struct mp_audio avrctx_fmt; // output format of avrctx
    struct mp_audio pool_fmt; // format used to allocate frames for avrctx output
    struct mp_audio pre_out_fmt; // format before final conversion (S24)
    struct AVAudioResampleContext *avrctx_out; // for output channel reordering
    struct af_resample_opts ctx;   // opts in the context
    struct af_resample_opts opts;  // opts requested by the user
    // At least libswresample keeps a pointer around for this:
    int reorder_in[MP_NUM_CHANNELS];
    int reorder_out[MP_NUM_CHANNELS];
    struct mp_audio_pool *reorder_buffer;
};

#if HAVE_LIBAVRESAMPLE
static double get_delay(struct af_resample *s)
{
    return avresample_get_delay(s->avrctx) / (double)s->ctx.in_rate +
           avresample_available(s->avrctx) / (double)s->ctx.out_rate;
}
static void drop_all_output(struct af_resample *s)
{
    while (avresample_read(s->avrctx, NULL, 1000) > 0) {}
}
static int get_out_samples(struct af_resample *s, int in_samples)
{
    return avresample_get_out_samples(s->avrctx, in_samples);
}
#else
static double get_delay(struct af_resample *s)
{
    int64_t base = s->ctx.in_rate * (int64_t)s->ctx.out_rate;
    return swr_get_delay(s->avrctx, base) / (double)base;
}
static void drop_all_output(struct af_resample *s)
{
    while (swr_drop_output(s->avrctx, 1000) > 0) {}
}
static int get_out_samples(struct af_resample *s, int in_samples)
{
#if LIBSWRESAMPLE_VERSION_MAJOR > 1 || LIBSWRESAMPLE_VERSION_MINOR >= 2
    return swr_get_out_samples(s->avrctx, in_samples);
#else
    return av_rescale_rnd(in_samples, s->ctx.out_rate, s->ctx.in_rate, AV_ROUND_UP)
           + swr_get_delay(s->avrctx, s->ctx.out_rate);
#endif
}
#endif

static void close_lavrr(struct af_instance *af)
{
    struct af_resample *s = af->priv;

    if (s->avrctx)
        avresample_close(s->avrctx);
    avresample_free(&s->avrctx);
    if (s->avrctx_out)
        avresample_close(s->avrctx_out);
    avresample_free(&s->avrctx_out);
}

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

// Return the format libavresample should convert to, given the final output
// format mp_format. In some cases (S24) we perform an extra conversion step,
// and signal here what exactly libavresample should output. It will be the
// input to the final conversion to mp_format.
static int check_output_conversion(int mp_format)
{
    if (mp_format == AF_FORMAT_S24)
        return AV_SAMPLE_FMT_S32;
    return af_to_avformat(mp_format);
}

bool af_lavrresample_test_conversion(int src_format, int dst_format)
{
    return af_to_avformat(src_format) != AV_SAMPLE_FMT_NONE &&
           check_output_conversion(dst_format) != AV_SAMPLE_FMT_NONE;
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
                           struct mp_audio *out, bool verbose)
{
    struct af_resample *s = af->priv;

    close_lavrr(af);

    s->avrctx = avresample_alloc_context();
    s->avrctx_out = avresample_alloc_context();
    if (!s->avrctx || !s->avrctx_out)
        goto error;

    enum AVSampleFormat in_samplefmt = af_to_avformat(in->format);
    enum AVSampleFormat out_samplefmt = check_output_conversion(out->format);
    enum AVSampleFormat out_samplefmtp = av_get_planar_sample_fmt(out_samplefmt);

    if (in_samplefmt == AV_SAMPLE_FMT_NONE ||
        out_samplefmt == AV_SAMPLE_FMT_NONE ||
        out_samplefmtp == AV_SAMPLE_FMT_NONE)
        goto error;

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
        goto error;

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

    struct mp_chmap in_lavc, out_lavc;
    mp_chmap_from_lavc(&in_lavc, in_ch_layout);
    mp_chmap_from_lavc(&out_lavc, out_ch_layout);

    if (verbose && !mp_chmap_equals(&in_lavc, &out_lavc)) {
        MP_VERBOSE(af, "Remix: %s -> %s\n", mp_chmap_to_str(&in_lavc),
                                            mp_chmap_to_str(&out_lavc));
    }

    if (in_lavc.num != map_in.num) {
        // For handling NA channels, we would have to add a planarization step.
        MP_FATAL(af, "Unsupported channel remapping.\n");
        goto error;
    }

    mp_chmap_get_reorder(s->reorder_in, &map_in, &in_lavc);
    transpose_order(s->reorder_in, map_in.num);

    if (mp_chmap_equals(&out_lavc, &map_out)) {
        // No intermediate step required - output new format directly.
        out_samplefmtp = out_samplefmt;
    } else {
        // Verify that we really just reorder and/or insert NA channels.
        struct mp_chmap withna = out_lavc;
        mp_chmap_fill_na(&withna, map_out.num);
        if (withna.num != map_out.num)
            goto error;
    }
    mp_chmap_get_reorder(s->reorder_out, &out_lavc, &map_out);

    s->avrctx_fmt = *out;
    mp_audio_set_channels(&s->avrctx_fmt, &out_lavc);
    mp_audio_set_format(&s->avrctx_fmt, af_from_avformat(out_samplefmtp));

    s->pre_out_fmt = *out;
    mp_audio_set_format(&s->pre_out_fmt, af_from_avformat(out_samplefmt));

    // If there are NA channels, the final output will have more channels than
    // the avrctx output. Also, avrctx will output planar (out_samplefmtp was
    // not overwritten). Allocate the output frame with more channels, so the
    // NA channels can be trivially added.
    s->pool_fmt = s->avrctx_fmt;
    if (map_out.num > out_lavc.num)
        mp_audio_set_channels(&s->pool_fmt, &map_out);

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
        goto error;

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

    if (avresample_open(s->avrctx) < 0 || avresample_open(s->avrctx_out) < 0) {
        MP_ERR(af, "Cannot open Libavresample Context. \n");
        goto error;
    }
    return AF_OK;

error:
    close_lavrr(af);
    return AF_ERROR;
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
        if (check_output_conversion(out->format) == AV_SAMPLE_FMT_NONE)
            mp_audio_set_format(out, in->format);

        int r = ((in->format == orig_in.format) &&
                mp_chmap_equals(&in->channels, &orig_in.channels))
                ? AF_OK : AF_FALSE;

        if (r == AF_OK)
            r = configure_lavrr(af, in, out, true);
        return r;
    }
    case AF_CONTROL_SET_FORMAT: {
        int format = *(int *)arg;
        if (format && check_output_conversion(format) == AV_SAMPLE_FMT_NONE)
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
        if (new_rate != s->ctx.in_rate && s->avrctx && af->fmt_out.format) {
            // Before reconfiguring, drain the audio that is still buffered
            // in the resampler.
            af->filter_frame(af, NULL);
            // Reinitialize resampler.
            configure_lavrr(af, &af->fmt_in, &af->fmt_out, false);
        }
        return AF_OK;
    }
    case AF_CONTROL_RESET:
        if (s->avrctx)
            drop_all_output(s);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

static void uninit(struct af_instance *af)
{
    close_lavrr(af);
}

// The LSB is always ignored.
#if BYTE_ORDER == BIG_ENDIAN
#define SHIFT24(x) ((3-(x))*8)
#else
#define SHIFT24(x) (((x)+1)*8)
#endif

static void extra_output_conversion(struct af_instance *af, struct mp_audio *mpa)
{
    if (mpa->format == AF_FORMAT_S32 && af->data->format == AF_FORMAT_S24) {
        size_t len = mp_audio_psize(mpa) / mpa->bps;
        for (int s = 0; s < len; s++) {
            uint32_t val = *((uint32_t *)mpa->planes[0] + s);
            uint8_t *ptr = (uint8_t *)mpa->planes[0] + s * 3;
            ptr[0] = val >> SHIFT24(0);
            ptr[1] = val >> SHIFT24(1);
            ptr[2] = val >> SHIFT24(2);
        }
        mp_audio_set_format(mpa, AF_FORMAT_S24);
    }
}

// This relies on the tricky way mpa was allocated.
static void reorder_planes(struct mp_audio *mpa, int *reorder,
                           struct mp_chmap *newmap)
{
    struct mp_audio prev = *mpa;
    mp_audio_set_channels(mpa, newmap);

    // The trailing planes were never written by avrctx, they're the NA channels.
    int next_na = prev.num_planes;

    for (int n = 0; n < mpa->num_planes; n++) {
        int src = reorder[n];
        assert(src >= -1 && src < prev.num_planes);
        if (src >= 0) {
            mpa->planes[n] = prev.planes[src];
        } else {
            assert(next_na < mpa->num_planes);
            mpa->planes[n] = prev.planes[next_na++];
            af_fill_silence(mpa->planes[n], mpa->sstride * mpa->samples,
                            mpa->format);
        }
    }
}

static int filter(struct af_instance *af, struct mp_audio *in)
{
    struct af_resample *s = af->priv;

    int samples = get_out_samples(s, in ? in->samples : 0);

    struct mp_audio out_format = s->pool_fmt;
    struct mp_audio *out = mp_audio_pool_get(af->out_pool, &out_format, samples);
    if (!out)
        goto error;
    if (in)
        mp_audio_copy_attributes(out, in);

    if (!s->avrctx)
        goto error;

    if (out->samples) {
        out->samples = resample_frame(s->avrctx, out, in);
        if (out->samples < 0)
            goto error;
    }

    struct mp_audio real_out = *out;
    mp_audio_copy_config(out, &s->avrctx_fmt);

    if (out->samples && !mp_audio_config_equals(out, &s->pre_out_fmt)) {
        assert(af_fmt_is_planar(out->format) && out->format == real_out.format);
        reorder_planes(out, s->reorder_out, &s->pool_fmt.channels);
        if (!mp_audio_config_equals(out, &s->pre_out_fmt)) {
            struct mp_audio *new = mp_audio_pool_get(s->reorder_buffer,
                                                     &s->pre_out_fmt,
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

    extra_output_conversion(af, out);

    talloc_free(in);
    if (out->samples) {
        af_add_output_frame(af, out);
    } else {
        talloc_free(out);
    }

    af->delay = get_delay(s);

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

    s->reorder_buffer = mp_audio_pool_create(s);

    return AF_OK;
}

#define OPT_BASE_STRUCT struct af_resample

const struct af_info af_info_lavrresample = {
    .info = "Sample frequency conversion using libavresample",
    .name = "lavrresample",
    .open = af_open,
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
