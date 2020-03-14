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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <stdarg.h>
#include <assert.h>

#include <libavutil/avstring.h>
#include <libavutil/mem.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

#include "common/common.h"
#include "common/av_common.h"
#include "common/tags.h"
#include "common/msg.h"

#include "audio/format.h"
#include "audio/aframe.h"
#include "video/mp_image.h"
#include "audio/fmt-conversion.h"
#include "video/fmt-conversion.h"
#include "video/hwdec.h"

#include "f_lavfi.h"
#include "filter.h"
#include "filter_internal.h"
#include "user_filters.h"

struct lavfi {
    struct mp_log *log;
    struct mp_filter *f;

    char *graph_string;
    char **graph_opts;
    bool force_bidir;
    enum mp_frame_type force_type;
    bool direct_filter;
    char **direct_filter_opts;

    AVFilterGraph *graph;
    // Set to true once all inputs have been initialized, and the graph is
    // linked.
    bool initialized;

    bool warned_nospeed;

    // Graph is draining to either handle format changes (if input format
    // changes for one pad, recreate the graph after draining all buffered
    // frames), or undo previously sent EOF (libavfilter does not accept
    // input anymore after sending EOF, so recreate the graph to "unstuck" it).
    bool draining_recover;

    // Filter can't be put into a working state.
    bool failed;

    struct lavfi_pad **in_pads;
    int num_in_pads;

    struct lavfi_pad **out_pads;
    int num_out_pads;

    struct lavfi_pad **all_pads;
    int num_all_pads;

    AVFrame *tmp_frame;

    // Audio timestamp emulation.
    bool emulate_audio_pts;
    double in_pts;      // last input timestamps
    int64_t in_samples; // samples ever sent to the filter
    double delay;       // seconds of audio apparently buffered by filter

    struct mp_lavfi public;
};

struct lavfi_pad {
    struct lavfi *main;
    enum mp_frame_type type;
    enum mp_pin_dir dir;
    char *name; // user-given pad name

    struct mp_pin *pin; // internal pin for this (never NULL once initialized)
    int pin_index;

    AVFilterContext *filter;
    int filter_pad;
    // buffersrc or buffersink connected to filter/filter_pad
    AVFilterContext *buffer;
    AVRational timebase;
    bool buffer_is_eof; // received/sent EOF to the buffer
    bool got_eagain;

    struct mp_tags *metadata;

    // 1-frame queue for input.
    struct mp_frame pending;

    // Used to check input format changes.
    struct mp_frame in_fmt;
};

// Free the libavfilter graph (not c), reset all state.
// Does not free pending data intentionally.
static void free_graph(struct lavfi *c)
{
    avfilter_graph_free(&c->graph);
    for (int n = 0; n < c->num_all_pads; n++) {
        struct lavfi_pad *pad = c->all_pads[n];

        pad->filter = NULL;
        pad->filter_pad = -1;
        pad->buffer = NULL;
        mp_frame_unref(&pad->in_fmt);
        pad->buffer_is_eof = false;
        pad->got_eagain = false;
    }
    c->initialized = false;
    c->draining_recover = false;
    c->in_pts = MP_NOPTS_VALUE;
    c->in_samples = 0;
    c->delay = 0;
}

static void add_pad(struct lavfi *c, int dir, int index, AVFilterContext *filter,
                    int filter_pad, const char *name, bool first_init)
{
    if (c->failed)
        return;

    enum AVMediaType avmt;
    if (dir == MP_PIN_IN) {
        avmt = avfilter_pad_get_type(filter->input_pads, filter_pad);
    } else {
        avmt = avfilter_pad_get_type(filter->output_pads, filter_pad);
    }
    int type;
    switch (avmt) {
    case AVMEDIA_TYPE_VIDEO: type = MP_FRAME_VIDEO; break;
    case AVMEDIA_TYPE_AUDIO: type = MP_FRAME_AUDIO; break;
    default:
        MP_FATAL(c, "unknown media type\n");
        c->failed = true;
        return;
    }

    // For anonymous pads, just make something up. libavfilter allows duplicate
    // pad names (while we don't), so we check for collisions along with normal
    // duplicate pads below.
    char tmp[80];
    const char *dir_string = dir == MP_PIN_IN ? "in" : "out";
    if (name) {
        if (c->direct_filter) {
            // libavfilter has this very unpleasant thing that filter labels
            // don't have to be unique - in particular, both input and output
            // are usually named "default". With direct filters, the user has
            // no chance to provide better names, so do something to resolve it.
            snprintf(tmp, sizeof(tmp), "%s_%s", name, dir_string);
            name = tmp;
        }
    } else {
        snprintf(tmp, sizeof(tmp), "%s%d", dir_string, index);
        name = tmp;
    }

    struct lavfi_pad *p = NULL;
    for (int n = 0; n < c->num_all_pads; n++) {
        if (strcmp(c->all_pads[n]->name, name) == 0) {
            p = c->all_pads[n];
            break;
        }
    }

    if (p) {
        // Graph recreation case: reassociate an existing pad.
        if (p->filter) {
            // Collision due to duplicate names.
            MP_FATAL(c, "more than one pad with label '%s'\n", name);
            c->failed = true;
            return;
        }
        if (p->dir != dir || p->type != type) {
            // libavfilter graph parser behavior not deterministic.
            MP_FATAL(c, "pad '%s' changed type or direction\n", name);
            c->failed = true;
            return;
        }
    } else {
        if (!first_init) {
            MP_FATAL(c, "filter pad '%s' got added later?\n", name);
            c->failed = true;
            return;
        }
        p = talloc_zero(c, struct lavfi_pad);
        p->main = c;
        p->dir = dir;
        p->name = talloc_strdup(p, name);
        p->type = type;
        p->pin_index = -1;
        p->metadata = talloc_zero(p, struct mp_tags);
        if (p->dir == MP_PIN_IN)
            MP_TARRAY_APPEND(c, c->in_pads, c->num_in_pads, p);
        if (p->dir == MP_PIN_OUT)
            MP_TARRAY_APPEND(c, c->out_pads, c->num_out_pads, p);
        MP_TARRAY_APPEND(c, c->all_pads, c->num_all_pads, p);
    }
    p->filter = filter;
    p->filter_pad = filter_pad;
}

static void add_pads(struct lavfi *c, int dir, AVFilterInOut *l, bool first_init)
{
    int index = 0;
    for (; l; l = l->next)
        add_pad(c, dir, index++, l->filter_ctx, l->pad_idx, l->name, first_init);
}

static void add_pads_direct(struct lavfi *c, int dir, AVFilterContext *f,
                            AVFilterPad *pads, int num_pads, bool first_init)
{
    for (int n = 0; n < num_pads; n++)
        add_pad(c, dir, n, f, n, avfilter_pad_get_name(pads, n), first_init);
}

// Parse the user-provided filter graph, and populate the unlinked filter pads.
static void precreate_graph(struct lavfi *c, bool first_init)
{
    assert(!c->graph);

    c->failed = false;

    c->graph = avfilter_graph_alloc();
    if (!c->graph)
        abort();

    if (mp_set_avopts(c->log, c->graph, c->graph_opts) < 0)
        goto error;

    if (c->direct_filter) {
        AVFilterContext *filter = avfilter_graph_alloc_filter(c->graph,
                            avfilter_get_by_name(c->graph_string), "filter");
        if (!filter) {
            MP_FATAL(c, "filter '%s' not found or failed to allocate\n",
                     c->graph_string);
            goto error;
        }

        if (mp_set_avopts_pos(c->log, filter, filter->priv,
                              c->direct_filter_opts) < 0)
            goto error;

        if (avfilter_init_str(filter, NULL) < 0) {
            MP_FATAL(c, "filter failed to initialize\n");
            goto error;
        }

        add_pads_direct(c, MP_PIN_IN, filter, filter->input_pads,
                        filter->nb_inputs, first_init);
        add_pads_direct(c, MP_PIN_OUT, filter, filter->output_pads,
                        filter->nb_outputs, first_init);
    } else {
        AVFilterInOut *in = NULL, *out = NULL;
        if (avfilter_graph_parse2(c->graph, c->graph_string, &in, &out) < 0) {
            MP_FATAL(c, "parsing the filter graph failed\n");
            goto error;
        }
        add_pads(c, MP_PIN_IN, in, first_init);
        add_pads(c, MP_PIN_OUT, out, first_init);
        avfilter_inout_free(&in);
        avfilter_inout_free(&out);
    }

    for (int n = 0; n < c->num_all_pads; n++)
        c->failed |= !c->all_pads[n]->filter;

    if (c->failed)
        goto error;

    return;

error:
    free_graph(c);
    c->failed = true;
    return;
}

// Ensure to send EOF to each input pad, so the graph can be drained properly.
static void send_global_eof(struct lavfi *c)
{
    for (int n = 0; n < c->num_in_pads; n++) {
        struct lavfi_pad *pad = c->in_pads[n];
        if (!pad->buffer || pad->buffer_is_eof)
            continue;

        if (av_buffersrc_add_frame(pad->buffer, NULL) < 0)
            MP_FATAL(c, "could not send EOF to filter\n");

        pad->buffer_is_eof = true;
    }
}

// libavfilter allows changing some parameters on the fly, but not
// others.
static bool is_aformat_ok(struct mp_aframe *a, struct mp_aframe *b)
{
    struct mp_chmap ca = {0}, cb = {0};
    mp_aframe_get_chmap(a, &ca);
    mp_aframe_get_chmap(b, &cb);
    return mp_chmap_equals(&ca, &cb) &&
           mp_aframe_get_rate(a) == mp_aframe_get_rate(b) &&
           mp_aframe_get_format(a) == mp_aframe_get_format(b);
}
static bool is_vformat_ok(struct mp_image *a, struct mp_image *b)
{
    return a->imgfmt == b->imgfmt &&
           a->w == b->w && a->h && b->h &&
           a->params.p_w == b->params.p_w && a->params.p_h == b->params.p_h &&
           a->nominal_fps == b->nominal_fps;
}
static bool is_format_ok(struct mp_frame a, struct mp_frame b)
{
    if (a.type == b.type && a.type == MP_FRAME_VIDEO)
        return is_vformat_ok(a.data, b.data);
    if (a.type == b.type && a.type == MP_FRAME_AUDIO)
        return is_aformat_ok(a.data, b.data);
    return false;
}

static void read_pad_input(struct lavfi *c, struct lavfi_pad *pad)
{
    assert(pad->dir == MP_PIN_IN);

    if (pad->pending.type || c->draining_recover)
        return;

    pad->pending = mp_pin_out_read(pad->pin);

    if (pad->pending.type && pad->pending.type != MP_FRAME_EOF &&
        pad->pending.type != pad->type)
    {
        MP_FATAL(c, "unknown frame %s\n", mp_frame_type_str(pad->pending.type));
        mp_frame_unref(&pad->pending);
    }

    if (mp_frame_is_data(pad->pending) && pad->in_fmt.type &&
        !is_format_ok(pad->pending, pad->in_fmt))
    {
        if (!c->draining_recover)
            MP_VERBOSE(c, "format change on %s\n", pad->name);
        c->draining_recover = true;
        if (c->initialized)
            send_global_eof(c);
    }
}

// Attempt to initialize all pads. Return true if all are initialized, or
// false if more data is needed (or on error).
static bool init_pads(struct lavfi *c)
{
    if (!c->graph)
        goto error;

    for (int n = 0; n < c->num_out_pads; n++) {
        struct lavfi_pad *pad = c->out_pads[n];
        if (pad->buffer)
            continue;

        const AVFilter *dst_filter = NULL;
        if (pad->type == MP_FRAME_AUDIO) {
            dst_filter = avfilter_get_by_name("abuffersink");
        } else if (pad->type == MP_FRAME_VIDEO) {
            dst_filter = avfilter_get_by_name("buffersink");
        } else {
            assert(0);
        }

        if (!dst_filter)
            goto error;

        char name[256];
        snprintf(name, sizeof(name), "mpv_sink_%s", pad->name);

        if (avfilter_graph_create_filter(&pad->buffer, dst_filter,
                                         name, NULL, NULL, c->graph) < 0)
            goto error;

        if (avfilter_link(pad->filter, pad->filter_pad, pad->buffer, 0) < 0)
            goto error;
    }

    for (int n = 0; n < c->num_in_pads; n++) {
        struct lavfi_pad *pad = c->in_pads[n];
        if (pad->buffer)
            continue;

        mp_frame_unref(&pad->in_fmt);

        read_pad_input(c, pad);
        // no input data, format unknown, can't init, wait longer.
        if (!pad->pending.type)
            return false;

        if (mp_frame_is_data(pad->pending)) {
            assert(pad->pending.type == pad->type);

            pad->in_fmt = mp_frame_ref(pad->pending);
            if (!pad->in_fmt.type)
                goto error;

            if (pad->in_fmt.type == MP_FRAME_VIDEO)
                mp_image_unref_data(pad->in_fmt.data);
            if (pad->in_fmt.type == MP_FRAME_AUDIO)
                mp_aframe_unref_data(pad->in_fmt.data);
        }

        if (pad->pending.type == MP_FRAME_EOF && !pad->in_fmt.type) {
            // libavfilter makes this painful. Init it with a dummy config,
            // just so we can tell it the stream is EOF.
            if (pad->type == MP_FRAME_AUDIO) {
                struct mp_aframe *fmt = mp_aframe_create();
                mp_aframe_set_format(fmt, AF_FORMAT_FLOAT);
                mp_aframe_set_chmap(fmt, &(struct mp_chmap)MP_CHMAP_INIT_STEREO);
                mp_aframe_set_rate(fmt, 48000);
                pad->in_fmt = (struct mp_frame){MP_FRAME_AUDIO, fmt};
            }
            if (pad->type == MP_FRAME_VIDEO) {
                struct mp_image *fmt = talloc_zero(NULL, struct mp_image);
                mp_image_setfmt(fmt, IMGFMT_420P);
                mp_image_set_size(fmt, 64, 64);
                pad->in_fmt = (struct mp_frame){MP_FRAME_VIDEO, fmt};
            }
        }

        if (pad->in_fmt.type != pad->type)
            goto error;

        AVBufferSrcParameters *params = av_buffersrc_parameters_alloc();
        if (!params)
            goto error;

        pad->timebase = AV_TIME_BASE_Q;

        char *filter_name = NULL;
        if (pad->type == MP_FRAME_AUDIO) {
            struct mp_aframe *fmt = pad->in_fmt.data;
            params->format = af_to_avformat(mp_aframe_get_format(fmt));
            params->sample_rate = mp_aframe_get_rate(fmt);
            struct mp_chmap chmap = {0};
            mp_aframe_get_chmap(fmt, &chmap);
            params->channel_layout = mp_chmap_to_lavc(&chmap);
            pad->timebase = (AVRational){1, mp_aframe_get_rate(fmt)};
            filter_name = "abuffer";
        } else if (pad->type == MP_FRAME_VIDEO) {
            struct mp_image *fmt = pad->in_fmt.data;
            params->format = imgfmt2pixfmt(fmt->imgfmt);
            params->width = fmt->w;
            params->height = fmt->h;
            params->sample_aspect_ratio.num = fmt->params.p_w;
            params->sample_aspect_ratio.den = fmt->params.p_h;
            params->hw_frames_ctx = fmt->hwctx;
            params->frame_rate = av_d2q(fmt->nominal_fps, 1000000);
            filter_name = "buffer";
        } else {
            assert(0);
        }

        params->time_base = pad->timebase;

        const AVFilter *filter = avfilter_get_by_name(filter_name);
        if (filter) {
            char name[256];
            snprintf(name, sizeof(name), "mpv_src_%s", pad->name);

            pad->buffer = avfilter_graph_alloc_filter(c->graph, filter, name);
        }
        if (!pad->buffer) {
            av_free(params);
            goto error;
        }

        int ret = av_buffersrc_parameters_set(pad->buffer, params);
        av_free(params);
        if (ret < 0)
            goto error;

        if (avfilter_init_str(pad->buffer, NULL) < 0)
            goto error;

        if (avfilter_link(pad->buffer, 0, pad->filter, pad->filter_pad) < 0)
            goto error;
    }

    return true;
error:
    MP_FATAL(c, "could not initialize filter pads\n");
    c->failed = true;
    return false;
}

static void dump_graph(struct lavfi *c)
{
    MP_DBG(c, "Filter graph:\n");
    char *s = avfilter_graph_dump(c->graph, NULL);
    if (s)
        MP_DBG(c, "%s\n", s);
    av_free(s);
}

// Initialize the graph if all inputs have formats set. If it's already
// initialized, or can't be initialized yet, do nothing.
static void init_graph(struct lavfi *c)
{
    assert(!c->initialized);

    if (!c->graph)
        precreate_graph(c, false);

    if (init_pads(c)) {
        struct mp_stream_info *info = mp_filter_find_stream_info(c->f);
        if (info && info->hwdec_devs) {
            struct mp_hwdec_ctx *hwdec = hwdec_devices_get_first(info->hwdec_devs);
            for (int n = 0; n < c->graph->nb_filters; n++) {
                AVFilterContext *filter = c->graph->filters[n];
                if (hwdec && hwdec->av_device_ref)
                    filter->hw_device_ctx = av_buffer_ref(hwdec->av_device_ref);
            }
        }

        // And here the actual libavfilter initialization happens.
        if (avfilter_graph_config(c->graph, NULL) < 0) {
            MP_FATAL(c, "failed to configure the filter graph\n");
            free_graph(c);
            c->failed = true;
            return;
        }

        // The timebase is available after configuring.
        for (int n = 0; n < c->num_out_pads; n++) {
            struct lavfi_pad *pad = c->out_pads[n];

            pad->timebase = pad->buffer->inputs[0]->time_base;
        }

        c->initialized = true;

        if (!c->direct_filter) // (output uninteresting for direct filters)
            dump_graph(c);
    }
}

static bool feed_input_pads(struct lavfi *c)
{
    bool progress = false;
    bool was_draining = c->draining_recover;

    assert(c->initialized);

    for (int n = 0; n < c->num_in_pads; n++) {
        struct lavfi_pad *pad = c->in_pads[n];

        bool requested = av_buffersrc_get_nb_failed_requests(pad->buffer) > 0;

        // Always request a frame after EOF so that we can know if the EOF state
        // changes (e.g. for sparse streams with midstream EOF).
        requested |= pad->buffer_is_eof;

        if (requested)
            read_pad_input(c, pad);

        if (!pad->pending.type || c->draining_recover)
            continue;

        if (pad->buffer_is_eof) {
            MP_WARN(c, "eof state changed on %s\n", pad->name);
            c->draining_recover = true;
            send_global_eof(c);
            continue;
        }

        if (pad->pending.type == MP_FRAME_AUDIO && !c->warned_nospeed) {
            struct mp_aframe *aframe = pad->pending.data;
            if (mp_aframe_get_speed(aframe) != 1.0) {
                MP_ERR(c, "speed changing filters before libavfilter are not "
                       "supported and can cause desyncs\n");
                c->warned_nospeed = true;
            }
        }

        AVFrame *frame = mp_frame_to_av(pad->pending, &pad->timebase);
        bool eof = pad->pending.type == MP_FRAME_EOF;

        if (c->emulate_audio_pts && pad->pending.type == MP_FRAME_AUDIO) {
            struct mp_aframe *aframe = pad->pending.data;
            c->in_pts = mp_aframe_end_pts(aframe);
            frame->pts = c->in_samples; // timebase is 1/sample_rate
            c->in_samples += frame->nb_samples;
        }

        mp_frame_unref(&pad->pending);

        if (!frame && !eof) {
            MP_FATAL(c, "out of memory or unsupported format\n");
            continue;
        }

        pad->buffer_is_eof = !frame;

        if (av_buffersrc_add_frame(pad->buffer, frame) < 0)
            MP_FATAL(c, "could not pass frame to filter\n");
        av_frame_free(&frame);

        for (int i = 0; i < c->num_out_pads; i++)
            c->out_pads[i]->got_eagain = false;

        progress = true;
    }

    if (!was_draining && c->draining_recover)
        progress = true;

    return progress;
}

// Some filters get stuck and return EAGAIN forever if they did not get any
// input (i.e. we send only EOF as input). "dynaudnorm" is known to be affected.
static bool check_stuck_eagain_on_eof_bug(struct lavfi *c)
{
    for (int n = 0; n < c->num_in_pads; n++) {
        if (!c->in_pads[n]->buffer_is_eof)
            return false;
    }

    for (int n = 0; n < c->num_out_pads; n++) {
        struct lavfi_pad *pad = c->out_pads[n];

        if (!pad->buffer_is_eof && !pad->got_eagain)
            return false;
    }

    MP_WARN(c, "Filter is stuck. This is a FFmpeg bug. Treating as EOF.\n");
    return true;
}

static bool read_output_pads(struct lavfi *c)
{
    bool progress = false;

    assert(c->initialized);

    for (int n = 0; n < c->num_out_pads; n++) {
        struct lavfi_pad *pad = c->out_pads[n];

        if (!mp_pin_in_needs_data(pad->pin))
            continue;

        assert(pad->buffer);

        int r = AVERROR_EOF;
        if (!pad->buffer_is_eof)
            r = av_buffersink_get_frame_flags(pad->buffer, c->tmp_frame, 0);

        pad->got_eagain = r == AVERROR(EAGAIN);
        if (pad->got_eagain) {
            if (check_stuck_eagain_on_eof_bug(c))
                r = AVERROR_EOF;
        } else {
            for (int i = 0; i < c->num_out_pads; i++)
                c->out_pads[i]->got_eagain = false;
        }

        if (r >= 0) {
            mp_tags_copy_from_av_dictionary(pad->metadata, c->tmp_frame->metadata);
            struct mp_frame frame =
                mp_frame_from_av(pad->type, c->tmp_frame, &pad->timebase);
            if (c->emulate_audio_pts && frame.type == MP_FRAME_AUDIO) {
                AVFrame *avframe = c->tmp_frame;
                struct mp_aframe *aframe = frame.data;
                double in_time = c->in_samples * av_q2d(c->in_pads[0]->timebase);
                double out_time = avframe->pts * av_q2d(pad->timebase);
                mp_aframe_set_pts(aframe, c->in_pts +
                    (c->in_pts != MP_NOPTS_VALUE ? (out_time - in_time) : 0));
            }
            if (frame.type == MP_FRAME_VIDEO) {
                struct mp_image *vframe = frame.data;
                vframe->nominal_fps =
                    av_q2d(av_buffersink_get_frame_rate(pad->buffer));
            }
            av_frame_unref(c->tmp_frame);
            if (frame.type) {
                mp_pin_in_write(pad->pin, frame);
            } else {
                MP_ERR(c, "could not use filter output\n");
                mp_frame_unref(&frame);
            }
            progress = true;
        } else if (r == AVERROR(EAGAIN)) {
            // We expect that libavfilter will request input on one of the
            // input pads (via av_buffersrc_get_nb_failed_requests()).
        } else if (r == AVERROR_EOF) {
            if (!c->draining_recover && !pad->buffer_is_eof)
                mp_pin_in_write(pad->pin, MP_EOF_FRAME);
            if (!pad->buffer_is_eof)
                progress = true;
            pad->buffer_is_eof = true;
        } else {
            // Real error - ignore it.
            MP_ERR(c, "error on filtering (%d)\n", r);
        }
    }

    return progress;
}

static void lavfi_process(struct mp_filter *f)
{
    struct lavfi *c = f->priv;

    if (!c->initialized)
        init_graph(c);

    while (c->initialized) {
        bool a = read_output_pads(c);
        bool b = feed_input_pads(c);
        if (!a && !b)
            break;
    }

    // Start over on format changes or EOF draining.
    if (c->draining_recover) {
        // Wait until all outputs got EOF.
        bool all_eof = true;
        for (int n = 0; n < c->num_out_pads; n++)
            all_eof &= c->out_pads[n]->buffer_is_eof;

        if (all_eof) {
            MP_VERBOSE(c, "recovering all eof\n");
            free_graph(c);
            mp_filter_internal_mark_progress(c->f);
        }
    }

    if (c->failed)
        mp_filter_internal_mark_failed(c->f);
}

static void lavfi_reset(struct mp_filter *f)
{
    struct lavfi *c = f->priv;

    free_graph(c);

    for (int n = 0; n < c->num_in_pads; n++)
        mp_frame_unref(&c->in_pads[n]->pending);
}

static void lavfi_destroy(struct mp_filter *f)
{
    struct lavfi *c = f->priv;

    lavfi_reset(f);
    av_frame_free(&c->tmp_frame);
}

static bool lavfi_command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct lavfi *c = f->priv;

    if (!c->initialized)
        return false;

    switch (cmd->type) {
    case MP_FILTER_COMMAND_TEXT: {
        return avfilter_graph_send_command(c->graph, "all", cmd->cmd, cmd->arg,
                                           &(char){0}, 0, 0) >= 0;
    }
    case MP_FILTER_COMMAND_GET_META: {
        // We can worry later about what it should do to multi output filters.
        if (c->num_out_pads < 1)
            return false;
        struct mp_tags **ptags = cmd->res;
        *ptags = mp_tags_dup(NULL, c->out_pads[0]->metadata);
        return true;
    }
    default:
        return false;
    }
}

static const struct mp_filter_info lavfi_filter = {
    .name = "lavfi",
    .priv_size = sizeof(struct lavfi),
    .process = lavfi_process,
    .reset = lavfi_reset,
    .destroy = lavfi_destroy,
    .command = lavfi_command,
};

static struct lavfi *lavfi_alloc(struct mp_filter *parent)
{
    struct mp_filter *f = mp_filter_create(parent, &lavfi_filter);
    if (!f)
        return NULL;

    struct lavfi *c = f->priv;

    c->f = f;
    c->log = f->log;
    c->public.f = f;
    c->tmp_frame = av_frame_alloc();
    if (!c->tmp_frame)
        abort();

    return c;
}

static struct mp_lavfi *do_init(struct lavfi *c)
{
    precreate_graph(c, true);

    if (c->failed)
        goto error;

    for (int n = 0; n < c->num_in_pads + c->num_out_pads; n++) {
        // First add input pins to satisfy order for bidir graph types.
        struct lavfi_pad *pad =
            n < c->num_in_pads ? c->in_pads[n] : c->out_pads[n - c->num_in_pads];

        pad->pin_index = c->f->num_pins;
        pad->pin = mp_filter_add_pin(c->f, pad->dir, pad->name);

        if (c->force_type && c->force_type != pad->type) {
            MP_FATAL(c, "mismatching media type\n");
            goto error;
        }
    }

    if (c->force_bidir) {
        if (c->f->num_pins != 2) {
            MP_FATAL(c, "exactly 2 pads required\n");
            goto error;
        }
        if (mp_pin_get_dir(c->f->ppins[0]) != MP_PIN_OUT ||
            mp_pin_get_dir(c->f->ppins[1]) != MP_PIN_IN)
        {
            MP_FATAL(c, "1 input and 1 output pad required\n");
            goto error;
        }
    }

    return &c->public;

error:
    talloc_free(c->f);
    return NULL;
}

struct mp_lavfi *mp_lavfi_create_graph(struct mp_filter *parent,
                                       enum mp_frame_type type, bool bidir,
                                       char **graph_opts, const char *graph)
{
    struct lavfi *c = lavfi_alloc(parent);
    if (!c)
        return NULL;

    c->force_type = type;
    c->force_bidir = bidir;
    c->graph_opts = mp_dup_str_array(c, graph_opts);
    c->graph_string = talloc_strdup(c, graph);

    return do_init(c);
}

struct mp_lavfi *mp_lavfi_create_filter(struct mp_filter *parent,
                                        enum mp_frame_type type, bool bidir,
                                        char **graph_opts,
                                        const char *filter, char **filter_opts)
{
    struct lavfi *c = lavfi_alloc(parent);
    if (!c)
        return NULL;

    c->force_type = type;
    c->force_bidir = bidir;
    c->graph_opts = mp_dup_str_array(c, graph_opts);
    c->graph_string = talloc_strdup(c, filter);
    c->direct_filter_opts = mp_dup_str_array(c, filter_opts);
    c->direct_filter = true;

    return do_init(c);
}

struct lavfi_user_opts {
    bool is_bridge;
    enum mp_frame_type type;

    char *graph;
    char **avopts;

    char *filter_name;
    char **filter_opts;

    int fix_pts;
};

static struct mp_filter *lavfi_create(struct mp_filter *parent, void *options)
{
    struct lavfi_user_opts *opts = options;
    struct mp_lavfi *l;
    if (opts->is_bridge) {
        l = mp_lavfi_create_filter(parent, opts->type, true, opts->avopts,
                                   opts->filter_name, opts->filter_opts);
    } else {
        l = mp_lavfi_create_graph(parent, opts->type, true,
                                  opts->avopts, opts->graph);
    }
    if (l) {
        struct lavfi *c = l->f->priv;
        c->emulate_audio_pts = opts->fix_pts;
    }
    talloc_free(opts);
    return l ? l->f : NULL;
}

static bool is_single_media_only(const AVFilterPad *pads, int media_type)
{
    int count = avfilter_pad_count(pads);
    if (count != 1)
        return false;
    return avfilter_pad_get_type(pads, 0) == media_type;
}

// Does it have exactly one video input and one video output?
static bool is_usable(const AVFilter *filter, int media_type)
{
    return is_single_media_only(filter->inputs, media_type) &&
           is_single_media_only(filter->outputs, media_type);
}

bool mp_lavfi_is_usable(const char *name, int media_type)
{
    const AVFilter *f = avfilter_get_by_name(name);
    return f && is_usable(f, media_type);
}

static void dump_list(struct mp_log *log, int media_type)
{
    mp_info(log, "Available libavfilter filters:\n");
    void *iter = NULL;
    for (;;) {
        const AVFilter *filter = av_filter_iterate(&iter);
        if (!filter)
            break;
        if (is_usable(filter, media_type))
            mp_info(log, "  %-16s %s\n", filter->name, filter->description);
    }
}

static const char *get_avopt_type_name(enum AVOptionType type)
{
    switch (type) {
    case AV_OPT_TYPE_FLAGS:             return "flags";
    case AV_OPT_TYPE_INT:               return "int";
    case AV_OPT_TYPE_INT64:             return "int64";
    case AV_OPT_TYPE_DOUBLE:            return "double";
    case AV_OPT_TYPE_FLOAT:             return "float";
    case AV_OPT_TYPE_STRING:            return "string";
    case AV_OPT_TYPE_RATIONAL:          return "rational";
    case AV_OPT_TYPE_BINARY:            return "binary";
    case AV_OPT_TYPE_DICT:              return "dict";
    case AV_OPT_TYPE_UINT64:            return "uint64";
    case AV_OPT_TYPE_IMAGE_SIZE:        return "imagesize";
    case AV_OPT_TYPE_PIXEL_FMT:         return "pixfmt";
    case AV_OPT_TYPE_SAMPLE_FMT:        return "samplefmt";
    case AV_OPT_TYPE_VIDEO_RATE:        return "fps";
    case AV_OPT_TYPE_DURATION:          return "duration";
    case AV_OPT_TYPE_COLOR:             return "color";
    case AV_OPT_TYPE_CHANNEL_LAYOUT:    return "channellayout";
    case AV_OPT_TYPE_BOOL:              return "bool";
    case AV_OPT_TYPE_CONST: // fallthrough
    default:
        return NULL;
    }
}

#define NSTR(s) ((s) ? (s) : "")

void print_lavfi_help(struct mp_log *log, const char *name, int media_type)
{
    const AVFilter *f = avfilter_get_by_name(name);
    if (!f) {
        mp_err(log, "Filter '%s' not found.\n", name);
        return;
    }
    if (!is_usable(f, media_type)) {
        mp_err(log, "Filter '%s' is not usable in this context (wrong media \n"
               "types or wrong number of inputs/outputs).\n", name);
    }
    mp_info(log, "Options:\n\n");
    const AVClass *class = f->priv_class;
    // av_opt_next() requires this for some retarded incomprehensible reason.
    const AVClass **c = &class;
    int offset= -1;
    int count = 0;
    for (const AVOption *o = av_opt_next(c, 0); o; o = av_opt_next(c, o)) {
        // This is how libavfilter (at the time) decided to assign positional
        // options (called "shorthand" in the libavfilter code). So we
        // duplicate it exactly.
        if (o->type == AV_OPT_TYPE_CONST || o->offset == offset)
            continue;
        offset = o->offset;

        const char *t = get_avopt_type_name(o->type);
        char *tstr = t ? mp_tprintf(30, "<%s>", t) : "?";
        mp_info(log, " %-10s %-12s %s\n", o->name, tstr, NSTR(o->help));

        const AVOption *sub = o;
        while (1) {
            sub = av_opt_next(c, sub);
            if (!sub || sub->type != AV_OPT_TYPE_CONST)
                break;
            mp_info(log, " %3s%-23s %s\n", "", sub->name, NSTR(sub->help));
        }

        count++;
    }
    mp_info(log, "\nTotal: %d options\n", count);
}

void print_lavfi_help_list(struct mp_log *log, int media_type)
{
    dump_list(log, media_type);
    mp_info(log, "\nIf libavfilter filters clash with builtin mpv filters,\n"
            "prefix them with lavfi- to select the libavfilter one.\n\n");
}

static void print_help(struct mp_log *log, int mediatype, char *name, char *ex)
{
    dump_list(log, mediatype);
    mp_info(log, "\n"
        "This lists %s->%s filters only. Refer to\n"
        "\n"
        " https://ffmpeg.org/ffmpeg-filters.html\n"
        "\n"
        "to see how to use each filter and what arguments each filter takes.\n"
        "Also, be sure to quote the FFmpeg filter string properly, e.g.:\n"
        "\n"
        " \"%s\"\n"
        "\n"
        "Otherwise, mpv and libavfilter syntax will conflict.\n"
        "\n", name, name, ex);
}

static void print_help_v(struct mp_log *log)
{
    print_help(log, AVMEDIA_TYPE_VIDEO, "video", "--vf=lavfi=[gradfun=20:30]");
}

static void print_help_a(struct mp_log *log)
{
    print_help(log, AVMEDIA_TYPE_AUDIO, "audio", "--af=lavfi=[volume=0.5]");
}

#define OPT_BASE_STRUCT struct lavfi_user_opts

const struct mp_user_filter_entry af_lavfi = {
    .desc = {
        .description = "libavfilter bridge",
        .name = "lavfi",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .options = (const m_option_t[]){
            {"graph", OPT_STRING(graph)},
            {"fix-pts", OPT_FLAG(fix_pts)},
            {"o", OPT_KEYVALUELIST(avopts)},
            {0}
        },
        .priv_defaults = &(const OPT_BASE_STRUCT){
            .type = MP_FRAME_AUDIO,
        },
        .print_help = print_help_a,
    },
    .create = lavfi_create,
};

const struct mp_user_filter_entry af_lavfi_bridge = {
    .desc = {
        .description = "libavfilter bridge (explicit options)",
        .name = "lavfi-bridge",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .options = (const m_option_t[]){
            {"name", OPT_STRING(filter_name)},
            {"opts", OPT_KEYVALUELIST(filter_opts)},
            {"o", OPT_KEYVALUELIST(avopts)},
            {0}
        },
        .priv_defaults = &(const OPT_BASE_STRUCT){
            .is_bridge = true,
            .type = MP_FRAME_AUDIO,
        },
        .print_help = print_help_a,
    },
    .create = lavfi_create,
};

const struct mp_user_filter_entry vf_lavfi = {
    .desc = {
        .description = "libavfilter bridge",
        .name = "lavfi",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .options = (const m_option_t[]){
            {"graph", OPT_STRING(graph)},
            {"o", OPT_KEYVALUELIST(avopts)},
            {0}
        },
        .priv_defaults = &(const OPT_BASE_STRUCT){
            .type = MP_FRAME_VIDEO,
        },
        .print_help = print_help_v,
    },
    .create = lavfi_create,
};

const struct mp_user_filter_entry vf_lavfi_bridge = {
    .desc = {
        .description = "libavfilter bridge (explicit options)",
        .name = "lavfi-bridge",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .options = (const m_option_t[]){
            {"name", OPT_STRING(filter_name)},
            {"opts", OPT_KEYVALUELIST(filter_opts)},
            {"o", OPT_KEYVALUELIST(avopts)},
            {0}
        },
        .priv_defaults = &(const OPT_BASE_STRUCT){
            .is_bridge = true,
            .type = MP_FRAME_VIDEO,
        },
        .print_help = print_help_v,
    },
    .create = lavfi_create,
};
