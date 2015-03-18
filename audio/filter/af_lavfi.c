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

#include "common/av_common.h"

#include "options/m_option.h"

// FFmpeg and Libav have slightly different APIs, just enough to cause us
// unnecessary pain. <Expletive deleted.>
#if LIBAVFILTER_VERSION_MICRO < 100
#define graph_parse(graph, filters, inputs, outputs, log_ctx) \
    avfilter_graph_parse(graph, filters, inputs, outputs, log_ctx)
#else
#define graph_parse(graph, filters, inputs, outputs, log_ctx) \
    avfilter_graph_parse_ptr(graph, filters, &(inputs), &(outputs), log_ctx)
#endif

struct priv {
    AVFilterGraph *graph;
    AVFilterContext *in;
    AVFilterContext *out;

    int64_t samples_in;

    AVRational timebase_out;

    bool eof;

    // options
    char *cfg_graph;
    char **cfg_avopts;
};

static void destroy_graph(struct af_instance *af)
{
    struct priv *p = af->priv;
    avfilter_graph_free(&p->graph);
    p->in = p->out = NULL;
    p->samples_in = 0;
    p->eof = false;
}

static bool recreate_graph(struct af_instance *af, struct mp_audio *config)
{
    void *tmp = talloc_new(NULL);
    struct priv *p = af->priv;
    AVFilterContext *in = NULL, *out = NULL, *f_format = NULL;

    if (bstr0(p->cfg_graph).len == 0) {
        MP_FATAL(af, "lavfi: no filter graph set\n");
        return false;
    }

    destroy_graph(af);
    MP_VERBOSE(af, "lavfi: create graph: '%s'\n", p->cfg_graph);

    AVFilterGraph *graph = avfilter_graph_alloc();
    if (!graph)
        goto error;

    if (mp_set_avopts(af->log, graph, p->cfg_avopts) < 0)
        goto error;

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    if (!outputs || !inputs)
        goto error;

    // Build list of acceptable output sample formats. libavfilter will insert
    // conversion filters if needed.
    static const enum AVSampleFormat sample_fmts[] = {
        AV_SAMPLE_FMT_U8, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S32,
        AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_DBL,
        AV_SAMPLE_FMT_U8P, AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_S32P,
        AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_DBLP,
        AV_SAMPLE_FMT_NONE
    };
    char *fmtstr = talloc_strdup(tmp, "");
    for (int n = 0; sample_fmts[n] != AV_SAMPLE_FMT_NONE; n++) {
        const char *name = av_get_sample_fmt_name(sample_fmts[n]);
        if (name) {
            const char *s = fmtstr[0] ? "|" : "";
            fmtstr = talloc_asprintf_append_buffer(fmtstr, "%s%s", s, name);
        }
    }

    char *src_args = talloc_asprintf(tmp,
        "sample_rate=%d:sample_fmt=%s:time_base=%d/%d:"
        "channel_layout=0x%"PRIx64,  config->rate,
        av_get_sample_fmt_name(af_to_avformat(config->format)),
        1, config->rate, mp_chmap_to_lavc(&config->channels));

    if (avfilter_graph_create_filter(&in, avfilter_get_by_name("abuffer"),
                                     "src", src_args, NULL, graph) < 0)
        goto error;

    if (avfilter_graph_create_filter(&out, avfilter_get_by_name("abuffersink"),
                                     "out", NULL, NULL, graph) < 0)
        goto error;

    if (avfilter_graph_create_filter(&f_format, avfilter_get_by_name("aformat"),
                                     "format", fmtstr, NULL, graph) < 0)
        goto error;

    if (avfilter_link(f_format, 0, out, 0) < 0)
        goto error;

    outputs->name = av_strdup("in");
    outputs->filter_ctx = in;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = f_format;

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

static void reset(struct af_instance *af)
{
    if (!recreate_graph(af, &af->fmt_in))
        MP_FATAL(af, "Can't recreate libavfilter filter after a seek reset.\n");
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
        mp_audio_set_channels(out, &out_cm);

        if (!mp_audio_config_valid(out))
            return AF_ERROR;

        p->timebase_out = l_out->time_base;

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    case AF_CONTROL_RESET:
        reset(af);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

static int filter_frame(struct af_instance *af, struct mp_audio *data)
{
    struct priv *p = af->priv;
    AVFrame *frame = NULL;

    if (p->eof && data)
        reset(af);

    if (!p->graph)
        goto error;

    AVFilterLink *l_in = p->in->outputs[0];

    if (data) {
        frame = av_frame_alloc();
        if (!frame)
            goto error;

        frame->nb_samples = data->samples;
        frame->format = l_in->format;

        // Timebase is 1/sample_rate
        frame->pts = p->samples_in;

        frame->channel_layout = l_in->channel_layout;
        frame->sample_rate = l_in->sample_rate;
#if LIBAVFILTER_VERSION_MICRO >= 100
        // FFmpeg being a stupid POS
        frame->channels = l_in->channels;
#endif

        frame->extended_data = frame->data;
        for (int n = 0; n < data->num_planes; n++)
            frame->data[n] = data->planes[n];
        frame->linesize[0] = frame->nb_samples * data->sstride;

        p->samples_in += data->samples;
    }

    if (av_buffersrc_add_frame(p->in, frame) < 0)
        goto error;

    av_frame_free(&frame);
    talloc_free(data);
    return 0;
error:
    av_frame_free(&frame);
    talloc_free(data);
    return -1;
}

static int filter_out(struct af_instance *af)
{
    struct priv *p = af->priv;

    if (!p->graph)
        goto error;

    AVFrame *frame = av_frame_alloc();
    if (!frame)
        goto error;

    int err = av_buffersink_get_frame(p->out, frame);
    if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
        // Not an error situation - no more output buffers in queue.
        // AVERROR_EOF means we shouldn't even give the filter more
        // input, but we don't handle that completely correctly.
        av_frame_free(&frame);
        p->eof |= err == AVERROR_EOF;
        return 0;
    }

    struct mp_audio *out = mp_audio_from_avframe(frame);
    if (!out)
        goto error;

    mp_audio_copy_config(out, af->data);

    if (frame->pts != AV_NOPTS_VALUE) {
        double in_time = p->samples_in / (double)af->fmt_in.rate;
        double out_time = frame->pts * av_q2d(p->timebase_out);
        // Need pts past the last output sample.
        out_time += out->samples / (double)out->rate;

        af->delay = in_time - out_time;
    }

    af_add_output_frame(af, out);
    av_frame_free(&frame);
    return 0;
error:
    av_frame_free(&frame);
    return -1;
}

static void uninit(struct af_instance *af)
{
    destroy_graph(af);
}

static int af_open(struct af_instance *af)
{
    af->control = control;
    af->uninit = uninit;
    af->filter_frame = filter_frame;
    af->filter_out = filter_out;
    // Removing this requires fixing AVFrame.data vs. AVFrame.extended_data
    assert(MP_NUM_CHANNELS <= AV_NUM_DATA_POINTERS);
    return AF_OK;
}

#define OPT_BASE_STRUCT struct priv

const struct af_info af_info_lavfi = {
    .info = "libavfilter bridge",
    .name = "lavfi",
    .open = af_open,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_STRING("graph", cfg_graph, 0),
        OPT_KEYVALUELIST("o", cfg_avopts, 0),
        {0}
    },
};
