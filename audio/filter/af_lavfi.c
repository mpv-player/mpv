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

#include "mpvcore/m_option.h"
#include "mpvcore/av_opts.h"

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

    // Guarantee that the data stays valid until next filter call
    char *out_buffer;

    struct mp_audio data;
    struct mp_audio temp;

    int64_t bytes_in;
    int64_t bytes_out;

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
        mp_msg(MSGT_AFILTER, MSGL_FATAL, "lavfi: no filter graph set\n");
        return false;
    }

    destroy_graph(af);
    mp_msg(MSGT_AFILTER, MSGL_V, "lavfi: create graph: '%s'\n", p->cfg_graph);

    AVFilterGraph *graph = avfilter_graph_alloc();
    if (!graph)
        goto error;

    if (parse_avopts(graph, p->cfg_avopts) < 0) {
        mp_msg(MSGT_VFILTER, MSGL_FATAL, "lavfi: could not set opts: '%s'\n",
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
    mp_msg(MSGT_AFILTER, MSGL_FATAL, "Can't configure libavfilter graph.\n");
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
            mp_audio_set_format(in, AF_FORMAT_FLOAT_NE);

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

        p->timebase_out = l_out->time_base;

        af->mul = (double) (out->rate * out->nch) / (in->rate * in->nch);

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    case AF_CONTROL_COMMAND_LINE: {
        talloc_free(p->cfg_graph);
        p->cfg_graph = talloc_strdup(p, (char *)arg);
        return AF_OK;
    }
    }
    return AF_UNKNOWN;
}

static struct mp_audio *play(struct af_instance *af, struct mp_audio *data)
{
    struct priv *p = af->priv;

    AVFilterLink *l_in = p->in->outputs[0];

    struct mp_audio *r = &p->temp;
    *r = *af->data;

    int in_frame_size = data->bps * data->channels.num;
    int out_frame_size = r->bps * r->channels.num;

    AVFrame *frame = av_frame_alloc();
    frame->nb_samples = data->len / in_frame_size;
    frame->format = l_in->format;

    // Timebase is 1/sample_rate
    frame->pts = p->bytes_in / in_frame_size;

    av_frame_set_channels(frame, l_in->channels);
    av_frame_set_channel_layout(frame, l_in->channel_layout);
    av_frame_set_sample_rate(frame, l_in->sample_rate);

    frame->data[0] = data->audio;
    frame->extended_data = frame->data;

    if (av_buffersrc_add_frame(p->in, frame) < 0) {
        av_frame_free(&frame);
        return NULL;
    }
    av_frame_free(&frame);

    int64_t out_pts = AV_NOPTS_VALUE;
    size_t out_len = 0;
    for (;;) {
        frame = av_frame_alloc();
        if (av_buffersink_get_frame(p->out, frame) < 0) {
            // Not an error situation - no more output buffers in queue.
            av_frame_free(&frame);
            break;
        }

        size_t new_len = out_len + frame->nb_samples * out_frame_size;
        if (new_len > talloc_get_size(p->out_buffer))
            p->out_buffer = talloc_realloc(p, p->out_buffer, char, new_len);
        memcpy(p->out_buffer + out_len, frame->data[0], new_len - out_len);
        out_len = new_len;
        if (out_pts == AV_NOPTS_VALUE)
            out_pts = frame->pts;

        av_frame_free(&frame);
    }

    r->audio = p->out_buffer;
    r->len = out_len;

    p->bytes_in += data->len;
    p->bytes_out += r->len;

    if (out_pts != AV_NOPTS_VALUE) {
        int64_t num_in_frames = p->bytes_in / in_frame_size;
        double in_time = num_in_frames / (double)data->rate;

        double out_time = out_pts * av_q2d(p->timebase_out);
        // Need pts past the last output sample.
        int out_frames = r->len / out_frame_size;
        out_time += out_frames / (double)r->rate;

        af->delay = (in_time - out_time) * r->rate * out_frame_size;
    }

    return r;
}

static void uninit(struct af_instance *af)
{
}

static int af_open(struct af_instance *af)
{
    af->control = control;
    af->uninit = uninit;
    af->play = play;
    af->mul = 1;
    struct priv *priv = af->priv;
    af->data = &priv->data;
    return AF_OK;
}

#define OPT_BASE_STRUCT struct priv

struct af_info af_info_lavfi = {
    "libavfilter bridge",
    "lavfi",
    "",
    "",
    0,
    af_open,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_STRING("graph", cfg_graph, 0),
        OPT_STRING("o", cfg_avopts, 0),
        {0}
    },
};
