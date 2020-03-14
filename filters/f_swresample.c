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

#include "config.h"

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
    struct mp_aframe *avrctx_fmt; // output format of avrctx
    struct mp_aframe *pool_fmt; // format used to allocate frames for avrctx output
    struct mp_aframe *pre_out_fmt; // format before final conversion
    struct SwrContext *avrctx_out; // for output channel reordering
    struct mp_resample_opts *opts; // opts requested by the user
    // At least libswresample keeps a pointer around for this:
    int reorder_in[MP_NUM_CHANNELS];
    int reorder_out[MP_NUM_CHANNELS];
    struct mp_aframe_pool *reorder_buffer;
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
        {"audio-resample-linear", OPT_FLAG(linear)},
        {"audio-resample-cutoff", OPT_DOUBLE(cutoff), M_RANGE(0, 1)},
        {"audio-normalize-downmix", OPT_FLAG(normalize)},
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
    swr_free(&p->avrctx_out);

    TA_FREEP(&p->pre_out_fmt);
    TA_FREEP(&p->avrctx_fmt);
    TA_FREEP(&p->pool_fmt);
}

static int rate_from_speed(int rate, double speed)
{
    return lrint(rate * speed);
}

static struct mp_chmap fudge_pairs[][2] = {
    {MP_CHMAP2(BL,  BR),  MP_CHMAP2(SL,  SR)},
    {MP_CHMAP2(SL,  SR),  MP_CHMAP2(BL,  BR)},
    {MP_CHMAP2(SDL, SDR), MP_CHMAP2(SL,  SR)},
    {MP_CHMAP2(SL,  SR),  MP_CHMAP2(SDL, SDR)},
};

// Modify out_layout and return the new value. The intention is reducing the
// loss libswresample's rematrixing will cause by exchanging similar, but
// strictly speaking incompatible channel pairs. For example, 7.1 should be
// changed to 7.1(wide) without dropping the SL/SR channels. (We still leave
// it to libswresample to create the remix matrix.)
static uint64_t fudge_layout_conversion(struct priv *p,
                                        uint64_t in, uint64_t out)
{
    for (int n = 0; n < MP_ARRAY_SIZE(fudge_pairs); n++) {
        uint64_t a = mp_chmap_to_lavc(&fudge_pairs[n][0]);
        uint64_t b = mp_chmap_to_lavc(&fudge_pairs[n][1]);
        if ((in & a) == a && (in & b) == 0 &&
            (out & a) == 0 && (out & b) == b)
        {
            out = (out & ~b) | a;

            MP_VERBOSE(p, "Fudge: %s -> %s\n",
                       mp_chmap_to_str(&fudge_pairs[n][0]),
                       mp_chmap_to_str(&fudge_pairs[n][1]));
        }
    }
    return out;
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
    p->avrctx_out = swr_alloc();
    if (!p->avrctx || !p->avrctx_out)
        goto error;

    enum AVSampleFormat in_samplefmt = af_to_avformat(p->in_format);
    enum AVSampleFormat out_samplefmt = af_to_avformat(p->out_format);
    enum AVSampleFormat out_samplefmtp = av_get_planar_sample_fmt(out_samplefmt);

    if (in_samplefmt == AV_SAMPLE_FMT_NONE ||
        out_samplefmt == AV_SAMPLE_FMT_NONE ||
        out_samplefmtp == AV_SAMPLE_FMT_NONE)
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

    struct mp_chmap map_in = p->in_channels;
    struct mp_chmap map_out = p->out_channels;

    // Try not to do any remixing if at least one is "unknown". Some corner
    // cases also benefit from disabling all channel handling logic if the
    // src/dst layouts are the same (like fl-fr-na -> fl-fr-na).
    if (mp_chmap_is_unknown(&map_in) || mp_chmap_is_unknown(&map_out) ||
        mp_chmap_equals(&map_in, &map_out))
    {
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
        MP_VERBOSE(p, "Remix: %s -> %s\n", mp_chmap_to_str(&in_lavc),
                                            mp_chmap_to_str(&out_lavc));
    }

    if (in_lavc.num != map_in.num) {
        // For handling NA channels, we would have to add a planarization step.
        MP_FATAL(p, "Unsupported input channel layout %s.\n",
                 mp_chmap_to_str(&map_in));
        goto error;
    }

    mp_chmap_get_reorder(p->reorder_in, &map_in, &in_lavc);
    transpose_order(p->reorder_in, map_in.num);

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
    mp_chmap_get_reorder(p->reorder_out, &out_lavc, &map_out);

    p->pre_out_fmt = mp_aframe_create();
    mp_aframe_set_rate(p->pre_out_fmt, p->out_rate);
    mp_aframe_set_chmap(p->pre_out_fmt, &p->out_channels);
    mp_aframe_set_format(p->pre_out_fmt, p->out_format);

    p->avrctx_fmt = mp_aframe_create();
    mp_aframe_config_copy(p->avrctx_fmt, p->pre_out_fmt);
    mp_aframe_set_chmap(p->avrctx_fmt, &out_lavc);
    mp_aframe_set_format(p->avrctx_fmt, af_from_avformat(out_samplefmtp));

    // If there are NA channels, the final output will have more channels than
    // the avrctx output. Also, avrctx will output planar (out_samplefmtp was
    // not overwritten). Allocate the output frame with more channels, so the
    // NA channels can be trivially added.
    p->pool_fmt = mp_aframe_create();
    mp_aframe_config_copy(p->pool_fmt, p->avrctx_fmt);
    if (map_out.num > out_lavc.num)
        mp_aframe_set_chmap(p->pool_fmt, &map_out);

    out_ch_layout = fudge_layout_conversion(p, in_ch_layout, out_ch_layout);

    // Real conversion; output is input to avrctx_out.
    av_opt_set_int(p->avrctx, "in_channel_layout",  in_ch_layout, 0);
    av_opt_set_int(p->avrctx, "out_channel_layout", out_ch_layout, 0);
    av_opt_set_int(p->avrctx, "in_sample_rate",     p->in_rate, 0);
    av_opt_set_int(p->avrctx, "out_sample_rate",    p->out_rate, 0);
    av_opt_set_int(p->avrctx, "in_sample_fmt",      in_samplefmt, 0);
    av_opt_set_int(p->avrctx, "out_sample_fmt",     out_samplefmtp, 0);

    // Just needs the correct number of channels for deplanarization.
    struct mp_chmap fake_chmap;
    mp_chmap_set_unknown(&fake_chmap, map_out.num);
    uint64_t fake_out_ch_layout = mp_chmap_to_lavc_unchecked(&fake_chmap);
    if (!fake_out_ch_layout)
        goto error;
    av_opt_set_int(p->avrctx_out, "in_channel_layout",  fake_out_ch_layout, 0);
    av_opt_set_int(p->avrctx_out, "out_channel_layout", fake_out_ch_layout, 0);

    av_opt_set_int(p->avrctx_out, "in_sample_fmt",      out_samplefmtp, 0);
    av_opt_set_int(p->avrctx_out, "out_sample_fmt",     out_samplefmt, 0);
    av_opt_set_int(p->avrctx_out, "in_sample_rate",     p->out_rate, 0);
    av_opt_set_int(p->avrctx_out, "out_sample_rate",    p->out_rate, 0);

    // API has weird requirements, quoting avresample.h:
    //  * This function can only be called when the allocated context is not open.
    //  * Also, the input channel layout must have already been set.
    swr_set_channel_mapping(p->avrctx, p->reorder_in);

    p->is_resampling = false;

    if (swr_init(p->avrctx) < 0 || swr_init(p->avrctx_out) < 0) {
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

static void reset(struct mp_filter *f)
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

static void extra_output_conversion(struct mp_aframe *mpa)
{
    int format = af_fmt_from_planar(mp_aframe_get_format(mpa));
    int num_planes = mp_aframe_get_planes(mpa);
    uint8_t **planes = mp_aframe_get_data_rw(mpa);
    if (!planes)
        return;
    for (int p = 0; p < num_planes; p++) {
        void *ptr = planes[p];
        int total = mp_aframe_get_total_plane_samples(mpa);
        if (format == AF_FORMAT_FLOAT) {
            for (int s = 0; s < total; s++)
                ((float *)ptr)[s] = av_clipf(((float *)ptr)[s], -1.0f, 1.0f);
        } else if (format == AF_FORMAT_DOUBLE) {
            for (int s = 0; s < total; s++)
                ((double *)ptr)[s] = MPCLAMP(((double *)ptr)[s], -1.0, 1.0);
        }
    }
}

// This relies on the tricky way mpa was allocated.
static bool reorder_planes(struct mp_aframe *mpa, int *reorder,
                           struct mp_chmap *newmap)
{
    if (!mp_aframe_set_chmap(mpa, newmap))
        return false;

    int num_planes = mp_aframe_get_planes(mpa);
    uint8_t **planes = mp_aframe_get_data_rw(mpa);
    uint8_t *old_planes[MP_NUM_CHANNELS];
    assert(num_planes <= MP_NUM_CHANNELS);
    for (int n = 0; n < num_planes; n++)
        old_planes[n] = planes[n];

    int next_na = 0;
    for (int n = 0; n < num_planes; n++)
        next_na += newmap->speaker[n] != MP_SPEAKER_ID_NA;

    for (int n = 0; n < num_planes; n++) {
        int src = reorder[n];
        assert(src >= -1 && src < num_planes);
        if (src >= 0) {
            planes[n] = old_planes[src];
        } else {
            assert(next_na < num_planes);
            planes[n] = old_planes[next_na++];
            // The NA planes were never written by avrctx, so clear them.
            af_fill_silence(planes[n],
                            mp_aframe_get_sstride(mpa) * mp_aframe_get_size(mpa),
                            mp_aframe_get_format(mpa));
        }
    }

    return true;
}

static int resample_frame(struct SwrContext *r,
                          struct mp_aframe *out, struct mp_aframe *in,
                          int consume_in)
{
    // Be aware that the channel layout and count can be different for in and
    // out frames. In some situations the caller will fix up the frames before
    // or after conversion. The sample rates can also be different.
    AVFrame *av_i = in ? mp_aframe_get_raw_avframe(in) : NULL;
    AVFrame *av_o = out ? mp_aframe_get_raw_avframe(out) : NULL;
    return swr_convert(r,
        av_o ? av_o->extended_data : NULL,
        av_o ? av_o->nb_samples : 0,
        (const uint8_t **)(av_i ? av_i->extended_data : NULL),
        av_i ? MPMIN(av_i->nb_samples, consume_in) : 0);
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
    mp_aframe_config_copy(out, p->pool_fmt);
    if (mp_aframe_pool_allocate(p->out_pool, out, samples) < 0)
        goto error;

    int out_samples = 0;
    if (samples) {
        out_samples = resample_frame(p->avrctx, out, in, consume_in);
        if (out_samples < 0 || out_samples > samples)
            goto error;
        mp_aframe_set_size(out, out_samples);
    }

    struct mp_chmap out_chmap;
    if (!mp_aframe_get_chmap(p->pool_fmt, &out_chmap))
        goto error;
    if (!reorder_planes(out, p->reorder_out, &out_chmap))
        goto error;

    if (!mp_aframe_config_equals(out, p->pre_out_fmt)) {
        struct mp_aframe *new = mp_aframe_create();
        mp_aframe_config_copy(new, p->pre_out_fmt);
        if (mp_aframe_pool_allocate(p->reorder_buffer, new, out_samples) < 0) {
            talloc_free(new);
            goto error;
        }
        int got = 0;
        if (out_samples)
            got = resample_frame(p->avrctx_out, new, out, out_samples);
        talloc_free(out);
        out = new;
        if (got != out_samples)
            goto error;
    }

    extra_output_conversion(out);

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

static void process(struct mp_filter *f)
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
        assert(!p->input);

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

static bool command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct priv *p = f->priv;

    if (cmd->type == MP_FILTER_COMMAND_SET_SPEED_RESAMPLE) {
        p->cmd_speed = cmd->speed;
        return true;
    }

    return false;
}

static void destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;

    close_lavrr(p);
    TA_FREEP(&p->input);
}

static const struct mp_filter_info swresample_filter = {
    .name = "swresample",
    .priv_size = sizeof(struct priv),
    .process = process,
    .command = command,
    .reset = reset,
    .destroy = destroy,
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

    p->reorder_buffer = mp_aframe_pool_create(p);
    p->out_pool = mp_aframe_pool_create(p);

    return &p->public;
}
