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
#include "audio/chmap_avchannel.h"
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
    struct mp_aframe *out_fmt; // output format of the filter
    struct mp_aframe *avrctx_fmt; // format of the frames avrctx writes into
    struct mp_resample_opts *opts; // opts requested by the user
    struct mp_aframe_pool *out_pool;
#if LIBSWRESAMPLE_VERSION_INT < AV_VERSION_INT(7, 3, 100)
    struct SwrContext *avrctx_out; // sample format conversion in fixup_output()
    // At least libswresample keeps a pointer around for this:
    int reorder_in[MP_NUM_CHANNELS];
    int reorder_out[MP_NUM_CHANNELS];
    struct mp_aframe_pool *reorder_buffer;
#endif

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
    TA_FREEP(&p->avrctx_fmt);
#if LIBSWRESAMPLE_VERSION_INT < AV_VERSION_INT(7, 3, 100)
    swr_free(&p->avrctx_out);
#endif
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

// libswresample remixes custom channel orders since FFmpeg 8.0 and accepts
// AV_CHAN_UNUSED in such layouts since 7.3.100. From there on it is given
// mpv's exact channel maps on both sides, and produces the output order and
// the NA channels itself. Older versions are given native layouts, and the
// input order, the output order and the NA channels are handled here.
#if LIBSWRESAMPLE_VERSION_INT < AV_VERSION_INT(7, 3, 100)

// mp_chmap_get_reorder() performs:
//  to->speaker[n] = from->speaker[src[n]]
// but libswresample does:
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

static bool needs_reorder(const int *reorder, int num)
{
    for (int n = 0; n < num; n++) {
        if (reorder[n] != n)
            return true;
    }
    return false;
}

// Give libswresample map_in in its native order, by reordering the input
// channels with swr_set_channel_mapping(). Sets in_layout accordingly.
static bool setup_native_input(struct priv *p, const struct mp_chmap *map_in,
                               AVChannelLayout *in_layout)
{
    struct mp_chmap in_lavc = *map_in;
    mp_chmap_reorder_to_lavc(&in_lavc);
    if (in_lavc.num != map_in->num) {
        // Dropping NA channels would need a planarization step.
        MP_FATAL(p, "Unsupported input channel layout %s.\n",
                 mp_chmap_to_str(map_in));
        return false;
    }
    mp_chmap_to_av_layout(in_layout, &in_lavc);

    mp_chmap_get_reorder(p->reorder_in, map_in, &in_lavc);
    transpose_order(p->reorder_in, map_in->num);
    // A channel mapping disables the direct conversion path in libswresample,
    // so set one only if the order really differs.
    if (needs_reorder(p->reorder_in, map_in->num))
        swr_set_channel_mapping(p->avrctx, p->reorder_in);
    return true;
}

// Prepare fixup_output() for avrctx producing the channels of swr_out, which
// is either map_out itself, or map_out without NA channels in native order.
// If any channel has to be touched, avrctx has to output planar samples, so
// planes can be swapped and added without copying, and avrctx_out converts
// to the final sample format afterwards. Returns the sample format avrctx
// has to output, or AV_SAMPLE_FMT_NONE on error.
static enum AVSampleFormat setup_output_fixup(struct priv *p,
                                              const struct mp_chmap *swr_out,
                                              const struct mp_chmap *map_out,
                                              enum AVSampleFormat out_samplefmt)
{
    enum AVSampleFormat swr_samplefmt = out_samplefmt;

    if (mp_chmap_equals(swr_out, map_out)) {
        // Nothing to touch, NA channels included.
        for (int n = 0; n < MP_NUM_CHANNELS; n++)
            p->reorder_out[n] = n < map_out->num ? n : -1;
    } else {
        mp_chmap_get_reorder(p->reorder_out, swr_out, map_out);
        swr_samplefmt = av_get_planar_sample_fmt(out_samplefmt);
        if (swr_samplefmt == AV_SAMPLE_FMT_NONE)
            return swr_samplefmt;
    }

    // The frames avrctx writes into have the final channel count, so the NA
    // channels it does not produce can be added without copying.
    mp_aframe_set_format(p->avrctx_fmt, af_from_avformat(swr_samplefmt));

    if (swr_samplefmt == out_samplefmt)
        return swr_samplefmt;

    // The channels are positionally identical on both sides of avrctx_out.
    p->avrctx_out = swr_alloc();
    if (!p->avrctx_out)
        return AV_SAMPLE_FMT_NONE;
    AVChannelLayout layout = {
        .order = AV_CHANNEL_ORDER_UNSPEC,
        .nb_channels = map_out->num,
    };
    av_opt_set_chlayout(p->avrctx_out, "in_chlayout", &layout, 0);
    av_opt_set_chlayout(p->avrctx_out, "out_chlayout", &layout, 0);
    av_opt_set_int(p->avrctx_out, "in_sample_fmt",      swr_samplefmt, 0);
    av_opt_set_int(p->avrctx_out, "out_sample_fmt",     out_samplefmt, 0);
    av_opt_set_int(p->avrctx_out, "in_sample_rate",     p->out_rate, 0);
    av_opt_set_int(p->avrctx_out, "out_sample_rate",    p->out_rate, 0);
    if (swr_init(p->avrctx_out) < 0)
        return AV_SAMPLE_FMT_NONE;

    return swr_samplefmt;
}

// This relies on the tricky way mpa was allocated.
static bool reorder_planes(struct mp_aframe *mpa, int *reorder,
                           struct mp_chmap *newmap)
{
    if (!mp_aframe_set_chmap(mpa, newmap))
        return false;

    int num_planes = mp_aframe_get_planes(mpa);
    uint8_t **planes = mp_aframe_get_data_rw(mpa);
    if (num_planes && !planes)
        return false;
    uint8_t *old_planes[MP_NUM_CHANNELS];
    mp_require(num_planes <= MP_NUM_CHANNELS);
    num_planes = MPMIN(num_planes, MP_NUM_CHANNELS);
    for (int n = 0; n < num_planes; n++)
        old_planes[n] = planes[n];

    int next_na = 0;
    for (int n = 0; n < num_planes; n++)
        next_na += newmap->speaker[n] != MP_SPEAKER_ID_NA;

    for (int n = 0; n < num_planes; n++) {
        int src = reorder[n];
        mp_assert(src >= -1 && src < num_planes);
        if (src >= 0) {
            planes[n] = old_planes[src];
        } else {
            mp_assert(next_na < num_planes);
            planes[n] = old_planes[next_na++];
            // The NA planes were never written by avrctx, so clear them.
            af_fill_silence(planes[n],
                            mp_aframe_get_sstride(mpa) * mp_aframe_get_size(mpa),
                            mp_aframe_get_format(mpa));
        }
    }

    return true;
}

// Restore the requested channel order and add the NA channels, then convert
// to the final sample format if avrctx had to output planar samples for that.
// Takes ownership of out. Returns the frame in the final format, or NULL.
static struct mp_aframe *fixup_output(struct priv *p, struct mp_aframe *out,
                                      int out_samples)
{
    struct mp_chmap out_chmap;
    if (!mp_aframe_get_chmap(p->out_fmt, &out_chmap) ||
        !reorder_planes(out, p->reorder_out, &out_chmap))
        goto error;

    if (mp_aframe_config_equals(out, p->out_fmt))
        return out;

    struct mp_aframe *new = mp_aframe_create();
    mp_aframe_config_copy(new, p->out_fmt);
    if (mp_aframe_pool_allocate(p->reorder_buffer, new, out_samples) < 0) {
        talloc_free(new);
        goto error;
    }
    int got = 0;
    if (out_samples)
        got = resample_frame(p->avrctx_out, new, out, out_samples);
    talloc_free(out);
    if (got != out_samples) {
        talloc_free(new);
        return NULL;
    }
    return new;

error:
    talloc_free(out);
    return NULL;
}

#endif

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

    p->avrctx_fmt = mp_aframe_create();
    mp_aframe_config_copy(p->avrctx_fmt, p->out_fmt);

    struct mp_chmap map_in = p->in_channels;
    struct mp_chmap map_out = p->out_channels;

    // Positional pass-through happens if either side has no speaker
    // information, or if both sides are the very same map, for example
    // fl-fr-na -> fl-fr-na. libswresample is then told nothing about the
    // channels on either side. Two AV_CHANNEL_ORDER_UNSPEC layouts with the
    // same channel count compare equal, so it never builds a mix matrix. If
    // the channel counts differ, the first channels that fit are copied and
    // the rest is left silent, see the explicit matrix below.
    bool passthrough = mp_chmap_is_unknown(&map_in) ||
                       mp_chmap_is_unknown(&map_out) ||
                       mp_chmap_equals(&map_in, &map_out);

    // Channels avrctx produces, in this order.
    struct mp_chmap swr_out = map_out;
    AVChannelLayout in_layout = {0}, out_layout = {0};
    if (passthrough) {
        in_layout = (AVChannelLayout){
            .order = AV_CHANNEL_ORDER_UNSPEC,
            .nb_channels = map_in.num,
        };
        out_layout = (AVChannelLayout){
            .order = AV_CHANNEL_ORDER_UNSPEC,
            .nb_channels = map_out.num,
        };
    } else {
#if LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(7, 3, 100)
        // Both layouts carry mpv's exact channel order, with NA channels as
        // AV_CHAN_UNUSED. libswresample remixes straight into the output
        // order and leaves NA channels silent.
        mp_chmap_to_av_layout_custom(&in_layout, &map_in);
        mp_chmap_to_av_layout_custom(&out_layout, &swr_out);
#else
        // Older libswresample only remixes native layouts. Reorder the input
        // channels into native order on the way in, request the speakers of
        // map_out in native order without NA channels, and let fixup_output()
        // restore the order and add the NA channels on the way out.
        if (!setup_native_input(p, &map_in, &in_layout))
            goto error;
        mp_chmap_remove_na(&swr_out);
        mp_chmap_reorder_to_lavc(&swr_out);
        mp_chmap_to_av_layout(&out_layout, &swr_out);
#endif

        if (verbose && !mp_chmap_equals_reordered(&map_in, &map_out)) {
            MP_VERBOSE(p, "Remix: %s -> %s\n", mp_chmap_to_str(&map_in),
                       mp_chmap_to_str(&map_out));
        }
    }

    enum AVSampleFormat swr_samplefmt = out_samplefmt; // what avrctx outputs
#if LIBSWRESAMPLE_VERSION_INT < AV_VERSION_INT(7, 3, 100)
    swr_samplefmt = setup_output_fixup(p, &swr_out, &map_out, out_samplefmt);
    if (swr_samplefmt == AV_SAMPLE_FMT_NONE)
        goto error;
#endif

    av_opt_set_chlayout(p->avrctx, "in_chlayout",  &in_layout, 0);
    av_opt_set_chlayout(p->avrctx, "out_chlayout", &out_layout, 0);
    av_channel_layout_uninit(&in_layout);
    av_channel_layout_uninit(&out_layout);
    av_opt_set_int(p->avrctx, "in_sample_rate",     p->in_rate, 0);
    av_opt_set_int(p->avrctx, "out_sample_rate",    p->out_rate, 0);
    av_opt_set_int(p->avrctx, "in_sample_fmt",      in_samplefmt, 0);
    av_opt_set_int(p->avrctx, "out_sample_fmt",     swr_samplefmt, 0);

    if (passthrough && map_in.num != map_out.num) {
        // Channels without meaning can only be copied by position. Keep the
        // first channels that fit and leave the rest silent. libswresample
        // needs an explicit matrix for this, as it has no layouts to derive
        // one from. The matrix survives swr_close(), see swresample_reset().
        int copied = MPMIN(map_in.num, map_out.num);
        if (verbose)
            MP_VERBOSE(p, "Copying %d of %d channels by position.\n", copied, map_in.num);
        double *matrix = talloc_zero_array(NULL, double, map_out.num * map_in.num);
        for (int n = 0; n < copied; n++)
            matrix[n * map_in.num + n] = 1.0;
        int r = swr_set_matrix(p->avrctx, matrix, map_in.num);
        talloc_free(matrix);
        if (r < 0)
            goto error;
    }

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
    mp_aframe_config_copy(out, p->avrctx_fmt);
    if (mp_aframe_pool_allocate(p->out_pool, out, samples) < 0)
        goto error;

    int out_samples = 0;
    if (samples) {
        out_samples = resample_frame(p->avrctx, out, in, consume_in);
        if (out_samples < 0 || out_samples > samples)
            goto error;
        mp_aframe_set_size(out, out_samples);
    }

#if LIBSWRESAMPLE_VERSION_INT < AV_VERSION_INT(7, 3, 100)
    out = fixup_output(p, out, out_samples);
    if (!out)
        goto error;
#endif

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
#if LIBSWRESAMPLE_VERSION_INT < AV_VERSION_INT(7, 3, 100)
    p->reorder_buffer = mp_aframe_pool_create(p);
#endif

    return &p->public;
}
