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

#include "config.h"

#include "common/common.h"
#include "common/av_common.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "aconverter.h"
#include "aframe.h"
#include "fmt-conversion.h"
#include "format.h"

#define HAVE_LIBSWRESAMPLE HAVE_FFMPEG_MPV
#define HAVE_LIBAVRESAMPLE HAVE_LIBAV

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
#define avresample_set_compensation swr_set_compensation
#else
#error "config.h broken or no resampler found"
#endif

struct mp_aconverter {
    struct mp_log *log;
    struct mpv_global *global;
    double playback_speed;
    bool is_resampling;
    bool passthrough_mode;
    struct AVAudioResampleContext *avrctx;
    struct mp_aframe *avrctx_fmt; // output format of avrctx
    struct mp_aframe *pool_fmt; // format used to allocate frames for avrctx output
    struct mp_aframe *pre_out_fmt; // format before final conversion
    struct AVAudioResampleContext *avrctx_out; // for output channel reordering
    const struct mp_resample_opts *opts; // opts requested by the user
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

    struct mp_aframe *input;    // queued input frame
    bool input_eof;             // queued input EOF
    struct mp_aframe *output;   // queued output frame
    bool output_eof;            // queued output EOF
};

#if HAVE_LIBAVRESAMPLE
static double get_delay(struct mp_aconverter *p)
{
    return avresample_get_delay(p->avrctx) / (double)p->in_rate +
           avresample_available(p->avrctx) / (double)p->out_rate;
}
static int get_out_samples(struct mp_aconverter *p, int in_samples)
{
    return avresample_get_out_samples(p->avrctx, in_samples);
}
#else
static double get_delay(struct mp_aconverter *p)
{
    int64_t base = p->in_rate * (int64_t)p->out_rate;
    return swr_get_delay(p->avrctx, base) / (double)base;
}
static int get_out_samples(struct mp_aconverter *p, int in_samples)
{
    return swr_get_out_samples(p->avrctx, in_samples);
}
#endif

static void close_lavrr(struct mp_aconverter *p)
{
    if (p->avrctx)
        avresample_close(p->avrctx);
    avresample_free(&p->avrctx);
    if (p->avrctx_out)
        avresample_close(p->avrctx_out);
    avresample_free(&p->avrctx_out);

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
static uint64_t fudge_layout_conversion(struct mp_aconverter *p,
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

static bool configure_lavrr(struct mp_aconverter *p, bool verbose)
{
    close_lavrr(p);

    p->in_rate = rate_from_speed(p->in_rate_user, p->playback_speed);

    p->passthrough_mode = p->opts->allow_passthrough &&
                          p->in_rate == p->out_rate &&
                          p->in_format == p->out_format &&
                          mp_chmap_equals(&p->in_channels, &p->out_channels);

    if (p->passthrough_mode)
        return true;

    p->avrctx = avresample_alloc_context();
    p->avrctx_out = avresample_alloc_context();
    if (!p->avrctx || !p->avrctx_out)
        goto error;

    enum AVSampleFormat in_samplefmt = af_to_avformat(p->in_format);
    enum AVSampleFormat out_samplefmt = af_to_avformat(p->out_format);
    enum AVSampleFormat out_samplefmtp = av_get_planar_sample_fmt(out_samplefmt);

    if (in_samplefmt == AV_SAMPLE_FMT_NONE ||
        out_samplefmt == AV_SAMPLE_FMT_NONE ||
        out_samplefmtp == AV_SAMPLE_FMT_NONE)
        goto error;

    av_opt_set_int(p->avrctx, "filter_size",        p->opts->filter_size, 0);
    av_opt_set_int(p->avrctx, "phase_shift",        p->opts->phase_shift, 0);
    av_opt_set_int(p->avrctx, "linear_interp",      p->opts->linear, 0);

    double cutoff = p->opts->cutoff;
    if (cutoff <= 0.0)
        cutoff = MPMAX(1.0 - 6.5 / (p->opts->filter_size + 8), 0.80);
    av_opt_set_double(p->avrctx, "cutoff",          cutoff, 0);

    int global_normalize;
    mp_read_option_raw(p->global, "audio-normalize-downmix", &m_option_type_flag,
                       &global_normalize);
    int normalize = p->opts->normalize;
    if (normalize < 0)
        normalize = global_normalize;
#if HAVE_LIBSWRESAMPLE
    av_opt_set_double(p->avrctx, "rematrix_maxval", normalize ? 1 : 1000, 0);
#else
    av_opt_set_int(p->avrctx, "normalize_mix_level", !!normalize, 0);
#endif

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
    avresample_set_channel_mapping(p->avrctx, p->reorder_in);

    p->is_resampling = false;

    if (avresample_open(p->avrctx) < 0 || avresample_open(p->avrctx_out) < 0) {
        MP_ERR(p, "Cannot open Libavresample Context. \n");
        goto error;
    }
    return true;

error:
    close_lavrr(p);
    return false;
}

bool mp_aconverter_reconfig(struct mp_aconverter *p,
                    int in_rate, int in_format, struct mp_chmap in_channels,
                    int out_rate, int out_format, struct mp_chmap out_channels)
{
    close_lavrr(p);

    TA_FREEP(&p->input);
    TA_FREEP(&p->output);
    p->input_eof = p->output_eof = false;

    p->playback_speed = 1.0;

    p->in_rate_user = in_rate;
    p->in_format    = in_format;
    p->in_channels  = in_channels;
    p->out_rate     = out_rate;
    p->out_format   = out_format;
    p->out_channels = out_channels;

    return configure_lavrr(p, true);
}

void mp_aconverter_flush(struct mp_aconverter *p)
{
    if (!p->avrctx)
        return;
#if HAVE_LIBSWRESAMPLE
    swr_close(p->avrctx);
    if (swr_init(p->avrctx) < 0)
        close_lavrr(p);
#else
    while (avresample_read(p->avrctx, NULL, 1000) > 0) {}
#endif
}

void mp_aconverter_set_speed(struct mp_aconverter *p, double speed)
{
    p->playback_speed = speed;
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

    int num_planes = newmap->num;
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

static int resample_frame(struct AVAudioResampleContext *r,
                          struct mp_aframe *out, struct mp_aframe *in)
{
    // Be aware that the channel layout and count can be different for in and
    // out frames. In some situations the caller will fix up the frames before
    // or after conversion. The sample rates can also be different.
    AVFrame *av_i = in ? mp_aframe_get_raw_avframe(in) : NULL;
    AVFrame *av_o = out ? mp_aframe_get_raw_avframe(out) : NULL;
    return avresample_convert(r,
        av_o ? av_o->extended_data : NULL,
        av_o ? av_o->linesize[0] : 0,
        av_o ? av_o->nb_samples : 0,
        av_i ? av_i->extended_data : NULL,
        av_i ? av_i->linesize[0] : 0,
        av_i ? av_i->nb_samples : 0);
}

static void filter_resample(struct mp_aconverter *p, struct mp_aframe *in)
{
    struct mp_aframe *out = NULL;

    if (!p->avrctx)
        goto error;

    int samples = get_out_samples(p, in ? mp_aframe_get_size(in) : 0);
    out = mp_aframe_create();
    mp_aframe_config_copy(out, p->pool_fmt);
    if (mp_aframe_pool_allocate(p->out_pool, out, samples) < 0)
        goto error;

    int out_samples = 0;
    if (samples) {
        out_samples = resample_frame(p->avrctx, out, in);
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
            got = resample_frame(p->avrctx_out, new, out);
        talloc_free(out);
        out = new;
        if (got != out_samples)
            goto error;
    }

    extra_output_conversion(out);

    if (in)
        mp_aframe_copy_attributes(out, in);

    if (out_samples) {
        p->output = out;
    } else {
        talloc_free(out);
    }
    p->output_eof = !in; // we've read everything

    return;
error:
    talloc_free(out);
    MP_ERR(p, "Error on resampling.\n");
}

static void filter(struct mp_aconverter *p)
{
    if (p->output || p->output_eof || !(p->input || p->input_eof))
        return;

    int new_rate = rate_from_speed(p->in_rate_user, p->playback_speed);

    if (p->passthrough_mode && new_rate != p->in_rate)
        configure_lavrr(p, false);

    if (p->passthrough_mode) {
        p->output = p->input;
        p->input = NULL;
        p->output_eof = p->input_eof;
        p->input_eof = false;
        return;
    }

    if (p->avrctx && !(!p->is_resampling && new_rate == p->in_rate)) {
        AVRational r = av_d2q(p->playback_speed * p->in_rate_user / p->in_rate,
                              INT_MAX / 2);
        // Essentially, swr/avresample_set_compensation() does 2 things:
        // - adjust output sample rate by sample_delta/compensation_distance
        // - reset the adjustment after compensation_distance output samples
        // Increase the compensation_distance to avoid undesired reset
        // semantics - we want to keep the ratio for the whole frame we're
        // feeding it, until the next filter() call.
        int mult = INT_MAX / 2 / MPMAX(MPMAX(abs(r.num), abs(r.den)), 1);
        r = (AVRational){ r.num * mult, r.den * mult };
        if (avresample_set_compensation(p->avrctx, r.den - r.num, r.den) >= 0) {
            new_rate = p->in_rate;
            p->is_resampling = true;
        }
    }

    bool need_reinit = fabs(new_rate / (double)p->in_rate - 1) > 0.01;
    if (need_reinit && new_rate != p->in_rate) {
        // Before reconfiguring, drain the audio that is still buffered
        // in the resampler.
        filter_resample(p, NULL);
        // Reinitialize resampler.
        configure_lavrr(p, false);
        p->output_eof = false;
        if (p->output)
            return; // need to read output before continuing filtering
    }

    filter_resample(p, p->input);
    TA_FREEP(&p->input);
    p->input_eof = false;
}

// Queue input. If true, ownership of in passes to mp_aconverted and the input
// was accepted. Otherwise, return false and reject in.
// in==NULL means trigger EOF.
bool mp_aconverter_write_input(struct mp_aconverter *p, struct mp_aframe *in)
{
    if (p->input || p->input_eof)
        return false;

    p->input = in;
    p->input_eof = !in;
    return true;
}

// Return output frame, or NULL if nothing available.
// *eof is set to true if NULL is returned, and it was due to EOF.
struct mp_aframe *mp_aconverter_read_output(struct mp_aconverter *p, bool *eof)
{
    *eof = false;

    filter(p);

    if (p->output) {
        struct mp_aframe *out = p->output;
        p->output = NULL;
        return out;
    }

    *eof = p->output_eof;
    p->output_eof = false;
    return NULL;
}

double mp_aconverter_get_latency(struct mp_aconverter *p)
{
    double delay = get_delay(p);

    if (p->input)
        delay += mp_aframe_duration(p->input);

    // In theory this is influenced by playback speed, but other parts of the
    // player get it wrong anyway.
    if (p->output)
        delay += mp_aframe_duration(p->output);

    return delay;
}

static void destroy_aconverter(void *ptr)
{
    struct mp_aconverter *p = ptr;

    close_lavrr(p);

    talloc_free(p->input);
    talloc_free(p->output);
}

// If opts is not NULL, the pointer must be valid for the lifetime of the
// mp_aconverter.
struct mp_aconverter *mp_aconverter_create(struct mpv_global *global,
                                           struct mp_log *log,
                                           const struct mp_resample_opts *opts)
{
    struct mp_aconverter *p = talloc_zero(NULL, struct mp_aconverter);
    p->log = log;
    p->global = global;

    static const struct mp_resample_opts defs = MP_RESAMPLE_OPTS_DEF;

    p->opts = opts ? opts : &defs;

    p->reorder_buffer = mp_aframe_pool_create(p);
    p->out_pool = mp_aframe_pool_create(p);

    talloc_set_destructor(p, destroy_aconverter);

    return p;
}
