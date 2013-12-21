/*
 * This file is part of mpv.
 *
 * Filter graph creation code taken from FFmpeg ffplay.c (LGPL 2.1 or later)
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
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include <libavutil/avstring.h>
#include <libavutil/mem.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

#include "audio/format.h"
#include "audio/fmt-conversion.h"
#include "af.h"

#include "options/m_option.h"
#include "common/av_opts.h"

#define IS_LIBAV_FORK (LIBAVFILTER_VERSION_MICRO < 100)

// FFmpeg and Libav have slightly different APIs, just enough to cause us
// unnecessary pain. <Expletive deleted.>
#if IS_LIBAV_FORK
#define graph_parse(graph, filters, inputs, outputs, log_ctx) \
    avfilter_graph_parse(graph, filters, inputs, outputs, log_ctx)
#else
#define graph_parse(graph, filters, inputs, outputs, log_ctx) \
    avfilter_graph_parse(graph, filters, &(inputs), &(outputs), log_ctx)
#endif

struct priv {
    AVFilterGraph *graph;
    AVFilterContext *in;
    AVFilterContext *out;

    int64_t samples_in;

    AVRational timebase_out;

    // options
    char *cfg_graph;
    char *cfg_avopts;
};

static void destroy_graph(struct af_instance *af)
{
    struct priv *p = af->priv;
    avfilter_graph_free(&p->graph);
    p->in = p->out = NULL;
}

static bool recreate_graph(struct af_instance *af, struct mp_audio *config)
{
    void *tmp = talloc_new(NULL);
    struct priv *p = af->priv;
    AVFilterContext *in = NULL, *out = NULL;
    int r;

    if (bstr0(p->cfg_graph).len == 0) {
        MP_FATAL(af, "lavfi: no filter graph set\n");
        return false;
    }

    destroy_graph(af);
    MP_VERBOSE(af, "lavfi: create graph: '%s'\n", p->cfg_graph);

    AVFilterGraph *graph = avfilter_graph_alloc();
    if (!graph)
        goto error;

    if (parse_avopts(graph, p->cfg_avopts) < 0) {
        MP_FATAL(af, "lavfi: could not set opts: '%s'\n",
               p->cfg_avopts);
        goto error;
    }

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    if (!outputs || !inputs)
        goto error;

    char *src_args = talloc_asprintf(tmp,
        "sample_rate=%d:sample_fmt=%s:channels=%d:time_base=%d/%d:"
        "channel_layout=0x%"PRIx64,  config->rate,
        av_get_sample_fmt_name(af_to_avformat(config->format)),
        config->channels.num, 1, config->rate,
        mp_chmap_to_lavc(&config->channels));

    if (avfilter_graph_create_filter(&in, avfilter_get_by_name("abuffer"),
                                     "src", src_args, NULL, graph) < 0)
        goto error;

    if (avfilter_graph_create_filter(&out, avfilter_get_by_name("abuffersink"),
                                     "out", NULL, NULL, graph) < 0)
        goto error;

    static const enum AVSampleFormat sample_fmts[] = {
        AV_SAMPLE_FMT_U8, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S32,
        AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_DBL,
        AV_SAMPLE_FMT_U8P, AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_S32P,
        AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_DBLP,
        AV_SAMPLE_FMT_NONE
    };
    r = av_opt_set_int_list(out, "sample_fmts", sample_fmts,
                            AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (r < 0)
        goto error;

    r = av_opt_set_int(out, "all_channel_counts", 1, AV_OPT_SEARCH_CHILDREN);
    if (r < 0)
        goto error;

    outputs->name = av_strdup("in");
    outputs->filter_ctx = in;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = out;

    if (graph_parse(graph, p->cfg_graph, inputs, outputs, NULL) < 0)
        goto error;

    if (avfilter_graph_config(graph, NULL) < 0)
        goto error;

    p->in = in;
    p->out = out;
    p->graph = graph;

    assert(out->nb_inputs == 1);
    assert(in->nb_outputs == 1);

    talloc_free(tmp);
    return true;

error:
    MP_FATAL(af, "Can't configure libavfilter graph.\n");
    avfilter_graph_free(&graph);
    talloc_free(tmp);
    return false;
}

static int control(struct af_instance *af, int cmd, void *arg)
{
    struct priv *p = af->priv;

    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio orig_in = *in;
        struct mp_audio *out = af->data;

        if (af_to_avformat(in->format) == AV_SAMPLE_FMT_NONE)
            mp_audio_set_format(in, AF_FORMAT_FLOAT);

        if (!mp_chmap_is_lavc(&in->channels))
            mp_chmap_reorder_to_lavc(&in->channels); // will always work

        if (!recreate_graph(af, in))
            return AF_ERROR;

        AVFilterLink *l_out = p->out->inputs[0];

        out->rate = l_out->sample_rate;

        mp_audio_set_format(out, af_from_avformat(l_out->format));

        struct mp_chmap out_cm;
        mp_chmap_from_lavc(&out_cm, l_out->channel_layout);
        if (!out_cm.num || out_cm.num != l_out->channels)
            mp_chmap_from_channels(&out_cm, l_out->channels);
        mp_audio_set_channels(out, &out_cm);

        if (!mp_audio_config_valid(out))
            return AF_ERROR;

        p->timebase_out = l_out->time_base;

        // Blatantly incorrect; we don't know what the filters do.
        af->mul = out->rate / (double)in->rate;

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    }
    return AF_UNKNOWN;
}

static int filter(struct af_instance *af, struct mp_audio *data, int flags)
{
    struct priv *p = af->priv;
    struct mp_audio *r = af->data;
    bool eof = data->samples == 0 && (flags & AF_FILTER_FLAG_EOF);
    AVFilterLink *l_in = p->in->outputs[0];

    AVFrame *frame = av_frame_alloc();
    frame->nb_samples = data->samples;
    frame->format = l_in->format;

    // Timebase is 1/sample_rate
    frame->pts = p->samples_in;

    av_frame_set_channels(frame, l_in->channels);
    av_frame_set_channel_layout(frame, l_in->channel_layout);
    av_frame_set_sample_rate(frame, l_in->sample_rate);

    frame->extended_data = frame->data;
    for (int n = 0; n < data->num_planes; n++)
        frame->data[n] = data->planes[n];
    frame->linesize[0] = frame->nb_samples * data->sstride;

    if (av_buffersrc_add_frame(p->in, eof ? NULL : frame) < 0) {
        av_frame_free(&frame);
        return -1;
    }
    av_frame_free(&frame);

    int64_t out_pts = AV_NOPTS_VALUE;
    r->samples = 0;
    for (;;) {
        frame = av_frame_alloc();
        if (av_buffersink_get_frame(p->out, frame) < 0) {
            // Not an error situation - no more output buffers in queue.
            av_frame_free(&frame);
            break;
        }

        mp_audio_realloc_min(r, r->samples + frame->nb_samples);
        for (int n = 0; n < r->num_planes; n++) {
            memcpy((char *)r->planes[n] + r->samples * r->sstride,
                   frame->extended_data[n], frame->nb_samples * r->sstride);
        }
        r->samples += frame->nb_samples;

        if (out_pts == AV_NOPTS_VALUE)
            out_pts = frame->pts;

        av_frame_free(&frame);
    }

    p->samples_in += data->samples;

    if (out_pts != AV_NOPTS_VALUE) {
        double in_time = p->samples_in / (double)data->rate;
        double out_time = out_pts * av_q2d(p->timebase_out);
        // Need pts past the last output sample.
        out_time += r->samples / (double)r->rate;

        af->delay = in_time - out_time;
    }

    *data = *r;
    return 0;
}

static void uninit(struct af_instance *af)
{
    destroy_graph(af);
}

static int af_open(struct af_instance *af)
{
    af->control = control;
    af->uninit = uninit;
    af->filter = filter;
    // Removing this requires fixing AVFrame.data vs. AVFrame.extended_data
    assert(MP_NUM_CHANNELS <= AV_NUM_DATA_POINTERS);
    return AF_OK;
}

#define OPT_BASE_STRUCT struct priv

struct af_info af_info_lavfi = {
    .info = "libavfilter bridge",
    .name = "lavfi",
    .open = af_open,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_STRING("graph", cfg_graph, 0),
        OPT_STRING("o", cfg_avopts, 0),
        {0}
    },
};
