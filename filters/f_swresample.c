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

#include <libavutil/opt.h>
#include <libavutil/common.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>

#include "audio/aframe.h"
#include "audio/fmt-conversion.h"
#include "audio/format.h"
#include "common/common.h"
#include "common/av_common.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "options/m_option.h"

#include "f_swresample.h"
#include "filter_internal.h"

struct priv {
    struct mp_log *log;
    bool is_resampling;
    struct SwrContext *avrctx;
    int channel_map[MP_NUM_CHANNELS]; // avrctx keeps a pointer to it
    struct mp_aframe *out_fmt; // output format of avrctx and of the filter
    struct mp_resample_opts *opts; // opts requested by the user
    struct mp_aframe_pool *out_pool;

    int in_rate_user; // user input sample rate
    int in_rate;      // actual rate (used by lavr), adjusted for playback speed
    int in_format;
    struct mp_chmap in_channels;
    int out_rate;
    int out_format;
    struct mp_chmap out_channels;

    double current_pts;
    struct mp_aframe *input;

    double cmd_speed;
    double speed;

    struct mp_swresample public;
};

#define OPT_BASE_STRUCT struct mp_resample_opts
const struct m_sub_options resample_conf = {
    .opts = (const m_option_t[]) {
        {"audio-resample-filter-size", OPT_INT(filter_size), M_RANGE(0, 32)},
        {"audio-resample-phase-shift", OPT_INT(phase_shift), M_RANGE(0, 30)},
        {"audio-resample-linear", OPT_BOOL(linear)},
        {"audio-resample-cutoff", OPT_DOUBLE(cutoff), M_RANGE(0, 1)},
        {"audio-normalize-downmix", OPT_BOOL(normalize)},
        {"audio-resample-max-output-size", OPT_DOUBLE(max_output_frame_size)},
        {"audio-swresample-o", OPT_KEYVALUELIST(avopts)},
        {0}
    },
    .size = sizeof(struct mp_resample_opts),
    .defaults = &(const struct mp_resample_opts)MP_RESAMPLE_OPTS_DEF,
    .change_flags = UPDATE_AUDIO,
};

static double get_delay(struct priv *p)
{
    int64_t base = p->in_rate * (int64_t)p->out_rate;
    return swr_get_delay(p->avrctx, base) / (double)base;
}
static int get_out_samples(struct priv *p, int in_samples)
{
    return swr_get_out_samples(p->avrctx, in_samples);
}

static void close_lavrr(struct priv *p)
{
    swr_free(&p->avrctx);

    TA_FREEP(&p->out_fmt);
}

static int rate_from_speed(int rate, double speed)
{
    return lrint(rate * speed);
}

static int resample_frame(struct SwrContext *r,
                          struct mp_aframe *out, struct mp_aframe *in,
                          int consume_in)
{
    // The channel layout and count can be different for in and out frames,
    // libswresample remixes between them. The sample rates can differ too.
    AVFrame *av_i = in ? mp_aframe_get_raw_avframe(in) : NULL;
    AVFrame *av_o = out ? mp_aframe_get_raw_avframe(out) : NULL;
    return swr_convert(r,
        av_o ? av_o->extended_data : NULL,
        av_o ? av_o->nb_samples : 0,
        (const uint8_t **)(av_i ? av_i->extended_data : NULL),
        av_i ? MPMIN(av_i->nb_samples, consume_in) : 0);
}

#define SWR_MATRIX_CHANNELS 64

// Get the gain from each channel of map_in to each channel of map_out.
static bool get_mix_matrix(struct priv *p, double *matrix,
                           const struct mp_chmap *map_in,
                           const struct mp_chmap *map_out,
                           enum AVSampleFormat out_samplefmt)
{
    double clev, slev, lfe_mix_level, maxval, volume;
    int64_t matrix_encoding;
    enum AVSampleFormat internal_samplefmt;
    if (av_opt_get_double(p->avrctx, "clev", 0, &clev) < 0 ||
        av_opt_get_double(p->avrctx, "slev", 0, &slev) < 0 ||
        av_opt_get_double(p->avrctx, "lfe_mix_level", 0, &lfe_mix_level) < 0 ||
        av_opt_get_double(p->avrctx, "rematrix_maxval", 0, &maxval) < 0 ||
        av_opt_get_double(p->avrctx, "rmvol", 0, &volume) < 0 ||
        av_opt_get_int(p->avrctx, "matrix_encoding", 0, &matrix_encoding) < 0 ||
        av_opt_get_sample_fmt(p->avrctx, "internal_sample_fmt", 0,
                              &internal_samplefmt) < 0)
        return false;
    // Without a limit, libswresample picks one that suits the sample formats.
    // The format it mixes in is an integer one only if the output is, or if
    // it was set.
    if (maxval <= 0) {
        bool integer =
            av_get_packed_sample_fmt(out_samplefmt) < AV_SAMPLE_FMT_FLT ||
            (internal_samplefmt != AV_SAMPLE_FMT_NONE &&
             av_get_packed_sample_fmt(internal_samplefmt) < AV_SAMPLE_FMT_FLT);
        maxval = integer ? 1 : INT_MAX;
    }

    // Channels keep their position if either side has no speaker information,
    // or if both sides are the very same map, for example fl-fr-na -> fl-fr-na.
    // If the number of channels differs, the first ones that fit are kept.
    bool by_position = mp_chmap_is_unknown(map_in) ||
                       mp_chmap_is_unknown(map_out) ||
                       mp_chmap_equals(map_in, map_out);

    // The same speakers on both sides only change their places. Nothing is
    // mixed, so this works for all speakers, also for those that libswresample
    // has no mix for.
    uint64_t in_mask = mp_chmap_to_lavc_unchecked(map_in);
    uint64_t out_mask = mp_chmap_to_lavc_unchecked(map_out);
    bool copy = by_position || in_mask == out_mask;

    // Different speakers are mixed by libswresample. It builds the matrix
    // between the speakers of both maps in their native order. A channel that
    // is copied is one speaker on both sides.
    AVChannelLayout in = AV_CHANNEL_LAYOUT_MONO;
    AVChannelLayout out = AV_CHANNEL_LAYOUT_MONO;
    if (!copy && (av_channel_layout_from_mask(&in, in_mask) < 0 ||
                  av_channel_layout_from_mask(&out, out_mask) < 0))
        return false;

    // Channels that keep their position stay as they are, unless the volume
    // is set. This is what libswresample does with equal layouts.
    double *native = talloc_zero_array(NULL, double, SWR_MATRIX_CHANNELS *
                                                     SWR_MATRIX_CHANNELS);
    native[0] = 1;
    int r = 0;
    if (!by_position || volume != 1) {
        r = swr_build_matrix2(&in, &out, clev, slev, lfe_mix_level, maxval,
                              volume, native, SWR_MATRIX_CHANNELS,
                              matrix_encoding, p->avrctx);
    }

    // Moved into the channel order of the maps, the matrix puts all channels
    // in their place and leaves the NA channels silent. Copied channels all
    // take the one coefficient that a single speaker has.
    for (int o = 0; o < map_out->num; o++) {
        for (int i = 0; i < map_in->num; i++) {
            int no = 0, ni = 0;
            if (by_position) {
                ni = o == i ? 0 : -1;
            } else if (copy) {
                ni = map_out->speaker[o] != MP_SPEAKER_ID_NA &&
                     map_out->speaker[o] == map_in->speaker[i] ? 0 : -1;
            } else {
                // Negative for NA channels, which have no place in a native
                // layout.
                no = av_channel_layout_index_from_channel(&out,
                                                          map_out->speaker[o]);
                ni = av_channel_layout_index_from_channel(&in,
                                                          map_in->speaker[i]);
            }
            if (no >= 0 && ni >= 0) {
                matrix[o * map_in->num + i] =
                    native[no * SWR_MATRIX_CHANNELS + ni];
            }
        }
    }
    talloc_free(native);
    return r >= 0;
}

// Tell libswresample what to do with the channels.
static bool set_channels(struct priv *p, enum AVSampleFormat in_samplefmt,
                         enum AVSampleFormat out_samplefmt, bool verbose)
{
    const struct mp_chmap *map_in = &p->in_channels;
    const struct mp_chmap *map_out = &p->out_channels;
    double *matrix = talloc_zero_array(NULL, double,
                                       map_out->num * map_in->num);
    bool ok = get_mix_matrix(p, matrix, map_in, map_out, out_samplefmt);

    // The volume is in the matrix. libswresample would mix on its own to
    // apply it, also if there is nothing to mix.
    av_opt_set_double(p->avrctx, "rmvol", 1, 0);

    // Without a mix, every output channel is one input channel, or silent.
    bool mix = false;
    bool in_place = map_in->num == map_out->num;
    for (int o = 0; o < map_out->num; o++) {
        p->channel_map[o] = -1;
        for (int i = 0; i < map_in->num; i++) {
            double gain = matrix[o * map_in->num + i];
            if (gain) {
                mix |= gain != 1 || p->channel_map[o] >= 0;
                p->channel_map[o] = i;
            }
        }
        in_place &= p->channel_map[o] == o;
    }

    if (ok && verbose && (mix || !in_place)) {
        MP_VERBOSE(p, "%s: %s -> %s\n", mix ? "Remix" : "Remap",
                   mp_chmap_to_str(map_in), mp_chmap_to_str(map_out));
    }

    // A channel map copies s32 as it is, a matrix takes it through floating
    // point. Other formats come out the same, and faster without a map.
    bool s32 = av_get_packed_sample_fmt(in_samplefmt) == AV_SAMPLE_FMT_S32 &&
               av_get_packed_sample_fmt(out_samplefmt) == AV_SAMPLE_FMT_S32;

    if (ok && (mix || (!in_place && !s32))) {
        ok = swr_set_matrix(p->avrctx, matrix, map_in->num) >= 0;
    } else if (ok && !in_place) {
        // The map has an entry for every channel that is used after it.
        AVChannelLayout used = {
            .order = AV_CHANNEL_ORDER_UNSPEC,
            .nb_channels = map_out->num,
        };
        ok = av_opt_set_chlayout(p->avrctx, "used_chlayout", &used, 0) >= 0 &&
             swr_set_channel_mapping(p->avrctx, p->channel_map) >= 0;
    }

    talloc_free(matrix);
    return ok;
}

static bool configure_lavrr(struct priv *p, bool verbose)
{
    close_lavrr(p);

    p->in_rate = rate_from_speed(p->in_rate_user, p->speed);

    MP_VERBOSE(p, "%dHz %s %s -> %dHz %s %s\n",
               p->in_rate, mp_chmap_to_str(&p->in_channels),
               af_fmt_to_str(p->in_format),
               p->out_rate, mp_chmap_to_str(&p->out_channels),
               af_fmt_to_str(p->out_format));

    p->avrctx = swr_alloc();
    if (!p->avrctx)
        goto error;

    enum AVSampleFormat in_samplefmt = af_to_avformat(p->in_format);
    enum AVSampleFormat out_samplefmt = af_to_avformat(p->out_format);

    if (in_samplefmt == AV_SAMPLE_FMT_NONE ||
        out_samplefmt == AV_SAMPLE_FMT_NONE)
    {
        MP_ERR(p, "unsupported conversion: %s -> %s\n",
               af_fmt_to_str(p->in_format), af_fmt_to_str(p->out_format));
        goto error;
    }

    av_opt_set_int(p->avrctx, "filter_size",        p->opts->filter_size, 0);
    av_opt_set_int(p->avrctx, "phase_shift",        p->opts->phase_shift, 0);
    av_opt_set_int(p->avrctx, "linear_interp",      p->opts->linear, 0);

    double cutoff = p->opts->cutoff;
    if (cutoff <= 0.0)
        cutoff = MPMAX(1.0 - 6.5 / (p->opts->filter_size + 8), 0.80);
    av_opt_set_double(p->avrctx, "cutoff",          cutoff, 0);

    int normalize = p->opts->normalize;
    av_opt_set_double(p->avrctx, "rematrix_maxval", normalize ? 1 : 1000, 0);

    if (mp_set_avopts(p->log, p->avrctx, p->opts->avopts) < 0)
        goto error;

    p->out_fmt = mp_aframe_create();
    mp_aframe_set_rate(p->out_fmt, p->out_rate);
    mp_aframe_set_chmap(p->out_fmt, &p->out_channels);
    mp_aframe_set_format(p->out_fmt, p->out_format);

    // libswresample is only told the number of channels on both sides. Two
    // such layouts with the same number of channels compare equal, so it
    // does nothing with the channels on its own.
    AVChannelLayout in_layout = {
        .order = AV_CHANNEL_ORDER_UNSPEC,
        .nb_channels = p->in_channels.num,
    };
    AVChannelLayout out_layout = {
        .order = AV_CHANNEL_ORDER_UNSPEC,
        .nb_channels = p->out_channels.num,
    };
    av_opt_set_chlayout(p->avrctx, "in_chlayout",  &in_layout, 0);
    av_opt_set_chlayout(p->avrctx, "out_chlayout", &out_layout, 0);
    av_opt_set_int(p->avrctx, "in_sample_rate",     p->in_rate, 0);
    av_opt_set_int(p->avrctx, "out_sample_rate",    p->out_rate, 0);
    av_opt_set_int(p->avrctx, "in_sample_fmt",      in_samplefmt, 0);
    av_opt_set_int(p->avrctx, "out_sample_fmt",     out_samplefmt, 0);

    // What is set here survives swr_close(), see swresample_reset().
    if (!set_channels(p, in_samplefmt, out_samplefmt, verbose))
        goto error;

    p->is_resampling = false;

    if (swr_init(p->avrctx) < 0) {
        MP_ERR(p, "Cannot open Libavresample context.\n");
        goto error;
    }
    return true;

error:
    close_lavrr(p);
    mp_filter_internal_mark_failed(p->public.f);
    MP_FATAL(p, "libswresample failed to initialize.\n");
    return false;
}

static void swresample_reset(struct mp_filter *f)
{
    struct priv *p = f->priv;

    p->current_pts = MP_NOPTS_VALUE;
    TA_FREEP(&p->input);

    if (!p->avrctx)
        return;
    swr_close(p->avrctx);
    if (swr_init(p->avrctx) < 0)
        close_lavrr(p);
}

static struct mp_frame filter_resample_output(struct priv *p,
                                              struct mp_aframe *in)
{
    struct mp_aframe *out = NULL;

    if (!p->avrctx)
        goto error;

    // Limit the filtered data size for better latency when changing speed.
    // Avoid buffering data within the resampler => restrict input size.
    // p->in_rate already includes the speed factor.
    double s = p->opts->max_output_frame_size / 1000 * p->in_rate;
    int max_in = lrint(MPCLAMP(s, 128, INT_MAX));
    int consume_in = in ? mp_aframe_get_size(in) : 0;
    consume_in = MPMIN(consume_in, max_in);

    int samples = get_out_samples(p, consume_in);
    out = mp_aframe_create();
    mp_aframe_config_copy(out, p->out_fmt);
    if (mp_aframe_pool_allocate(p->out_pool, out, samples) < 0)
        goto error;

    int out_samples = 0;
    if (samples) {
        out_samples = resample_frame(p->avrctx, out, in, consume_in);
        if (out_samples < 0 || out_samples > samples)
            goto error;
        mp_aframe_set_size(out, out_samples);
    }

    if (in) {
        mp_aframe_copy_attributes(out, in);
        p->current_pts = mp_aframe_end_pts(in);
        mp_aframe_skip_samples(in, consume_in);
    }

    if (out_samples) {
        if (p->current_pts != MP_NOPTS_VALUE) {
            double delay = get_delay(p) * mp_aframe_get_speed(out) +
                           mp_aframe_duration(out) +
                           (p->input ? mp_aframe_duration(p->input) : 0);
            mp_aframe_set_pts(out, p->current_pts - delay);
            mp_aframe_mul_speed(out, p->speed);
        }
    } else {
        TA_FREEP(&out);
    }

    return out ? MAKE_FRAME(MP_FRAME_AUDIO, out) : MP_NO_FRAME;
error:
    talloc_free(out);
    MP_ERR(p, "Error on resampling.\n");
    mp_filter_internal_mark_failed(p->public.f);
    return MP_NO_FRAME;
}

static void swresample_process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    p->speed = p->cmd_speed * p->public.speed;

    struct mp_aframe *input = NULL;
    if (!p->input) {
        struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

        if (frame.type == MP_FRAME_AUDIO) {
            input = frame.data;
        } else if (!frame.type) {
            return; // no new data
        } else if (frame.type != MP_FRAME_EOF) {
            MP_ERR(p, "Unsupported frame type.\n");
            mp_frame_unref(&frame);
            mp_filter_internal_mark_failed(f);
            return;
        }

        if (!input && !p->avrctx) {
            // Obviously no draining needed.
            mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
            return;
        }
    }

    if (input) {
        mp_assert(!p->input);

        struct mp_swresample *s = &p->public;

        int in_rate = mp_aframe_get_rate(input);
        int in_format = mp_aframe_get_format(input);
        struct mp_chmap in_channels = {0};
        mp_aframe_get_chmap(input, &in_channels);

        if (!in_rate || !in_format || !in_channels.num) {
            MP_ERR(p, "Frame with invalid format unsupported\n");
            talloc_free(input);
            mp_filter_internal_mark_failed(f);
            return;
        }

        int out_rate = s->out_rate ? s->out_rate : in_rate;
        int out_format = s->out_format ? s->out_format : in_format;
        struct mp_chmap out_channels =
            s->out_channels.num ? s->out_channels : in_channels;

        if (p->in_rate_user != in_rate ||
            p->in_format != in_format ||
            !mp_chmap_equals(&p->in_channels, &in_channels) ||
            p->out_rate != out_rate ||
            p->out_format != out_format ||
            !mp_chmap_equals(&p->out_channels, &out_channels) ||
            !p->avrctx)
        {
            if (p->avrctx) {
                // drain remaining audio
                struct mp_frame out = filter_resample_output(p, NULL);
                if (out.type) {
                    mp_pin_in_write(f->ppins[1], out);
                    // continue filtering next time.
                    mp_pin_out_unread(f->ppins[0],
                                      MAKE_FRAME(MP_FRAME_AUDIO, input));
                    input = NULL;
                }
            }

            MP_VERBOSE(p, "format change, reinitializing resampler\n");

            p->in_rate_user = in_rate;
            p->in_format = in_format;
            p->in_channels = in_channels;
            p->out_rate = out_rate;
            p->out_format = out_format;
            p->out_channels = out_channels;

            if (!configure_lavrr(p, true)) {
                talloc_free(input);
                return;
            }

            if (!input) {
                // continue filtering next time
                mp_filter_internal_mark_progress(f);
                return;
            }
        }

        p->input = input;
    }

    int new_rate = rate_from_speed(p->in_rate_user, p->speed);
    bool exact_rate = new_rate == p->in_rate;
    bool use_comp = fabs(new_rate / (double)p->in_rate - 1) <= 0.01;
    // If we've never used compensation, avoid setting it - even if it's in
    // theory a NOP, libswresample will enable resampling. _If_ we're
    // resampling, we might have to disable previously enabled compensation.
    if (exact_rate && !p->is_resampling)
        use_comp = false;
    if (p->avrctx && use_comp) {
        AVRational r =
            av_d2q(p->speed * p->in_rate_user / p->in_rate, INT_MAX / 2);
        // Essentially, swr_set_compensation() does 2 things:
        // - adjust output sample rate by sample_delta/compensation_distance
        // - reset the adjustment after compensation_distance output samples
        // Increase the compensation_distance to avoid undesired reset
        // semantics - we want to keep the ratio for the whole frame we're
        // feeding it, until the next filter() call.
        int mult = INT_MAX / 2 / MPMAX(MPMAX(abs(r.num), abs(r.den)), 1);
        r = (AVRational){ r.num * mult, r.den * mult };
        if (r.den == r.num)
            r = (AVRational){0}; // fully disable
        if (swr_set_compensation(p->avrctx, r.den - r.num, r.den) >= 0) {
            exact_rate = true;
            p->is_resampling = true; // libswresample can auto-enable it
        }
    }

    if (!exact_rate) {
        // Before reconfiguring, drain the audio that is still buffered
        // in the resampler.
        struct mp_frame out = filter_resample_output(p, NULL);
        bool need_drain = !!out.type;
        if (need_drain)
            mp_pin_in_write(f->ppins[1], out);
        // Reinitialize resampler.
        configure_lavrr(p, false);
        // If we've written output, we must continue filtering next time.
        if (need_drain)
            return;
    }

    struct mp_frame out = filter_resample_output(p, p->input);

    if (out.type) {
        mp_pin_in_write(f->ppins[1], out);
        if (!p->input)
            mp_pin_out_repeat_eof(f->ppins[0]);
    } else if (p->input) {
        mp_filter_internal_mark_progress(f); // try to consume more input
    } else {
        mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
    }

    if (p->input && !mp_aframe_get_size(p->input))
        TA_FREEP(&p->input);
}

double mp_swresample_get_delay(struct mp_swresample *s)
{
    struct priv *p = s->f->priv;

    return get_delay(p);
}

static bool swresample_command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct priv *p = f->priv;

    if (cmd->type == MP_FILTER_COMMAND_SET_SPEED_RESAMPLE) {
        p->cmd_speed = cmd->speed;
        return true;
    }

    return false;
}

static void swresample_destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;

    close_lavrr(p);
    TA_FREEP(&p->input);
}

static const struct mp_filter_info swresample_filter = {
    .name = "swresample",
    .priv_size = sizeof(struct priv),
    .process = swresample_process,
    .command = swresample_command,
    .reset = swresample_reset,
    .destroy = swresample_destroy,
};

struct mp_swresample *mp_swresample_create(struct mp_filter *parent,
                                           struct mp_resample_opts *opts)
{
    struct mp_filter *f = mp_filter_create(parent, &swresample_filter);
    if (!f)
        return NULL;

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->public.f = f;
    p->public.speed = 1.0;
    p->cmd_speed = 1.0;
    p->log = f->log;

    if (opts) {
        p->opts = talloc_dup(p, opts);
        p->opts->avopts = mp_dup_str_array(p, p->opts->avopts);
    } else {
        p->opts = mp_get_config_group(p, f->global, &resample_conf);
    }

    p->out_pool = mp_aframe_pool_create(p);

    return &p->public;
}
