/*
 * This file is part of mpv.
 *
 * Filter graph creation code taken from Libav avplay.c (LGPL 2.1 or later)
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
#include <stdarg.h>
#include <assert.h>

#include <libavutil/avstring.h>
#include <libavutil/mem.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

#include "common/msg.h"
#include "options/m_option.h"
#include "common/av_opts.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "video/fmt-conversion.h"
#include "vf.h"
#include "vf_lavfi.h"

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

// ":" is deprecated, but "|" doesn't work in earlier versions.
#if (IS_LIBAV_FORK  && LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(3, 7, 0)) || \
    (!IS_LIBAV_FORK && LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(3, 50, 100))
#define FMTSEP "|"
#else
#define FMTSEP ":"
#endif

struct vf_priv_s {
    AVFilterGraph *graph;
    AVFilterContext *in;
    AVFilterContext *out;

    AVRational timebase_in;
    AVRational timebase_out;
    AVRational par_in;

    // for the lw wrapper
    void *old_priv;
    void (*lw_recreate_cb)(struct vf_instance *vf);

    // options
    char *cfg_graph;
    int64_t cfg_sws_flags;
    char *cfg_avopts;
};

static const struct vf_priv_s vf_priv_dflt = {
    .cfg_sws_flags = SWS_BICUBIC,
};

static void destroy_graph(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    avfilter_graph_free(&p->graph);
    p->in = p->out = NULL;
}

static AVRational par_from_sar_dar(int width, int height,
                                   int d_width, int d_height)
{
    return av_div_q((AVRational){d_width, d_height},
                    (AVRational){width, height});
}

static void dar_from_sar_par(int width, int height, AVRational par,
                             int *out_dw, int *out_dh)
{
    *out_dw = width;
    *out_dh = height;
    if (par.num != 0 && par.den != 0) {
        double d = av_q2d(par);
        if (d > 1.0) {
            *out_dw *= d;
        } else {
            *out_dh /= d;
        }
    }
}

static bool recreate_graph(struct vf_instance *vf, int width, int height,
                           int d_width, int d_height, unsigned int fmt)
{
    void *tmp = talloc_new(NULL);
    struct vf_priv_s *p = vf->priv;
    AVFilterContext *in = NULL, *out = NULL, *f_format = NULL;

    if (vf->priv->lw_recreate_cb)
        vf->priv->lw_recreate_cb(vf);

    if (bstr0(p->cfg_graph).len == 0) {
        mp_msg(MSGT_VFILTER, MSGL_FATAL, "lavfi: no filter graph set\n");
        return false;
    }

    destroy_graph(vf);
    mp_msg(MSGT_VFILTER, MSGL_V, "lavfi: create graph: '%s'\n", p->cfg_graph);

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

    // Build list of acceptable output pixel formats. libavfilter will insert
    // conversion filters if needed.
    char *fmtstr = talloc_strdup(tmp, "");
    for (int n = IMGFMT_START; n < IMGFMT_END; n++) {
        if (vf_next_query_format(vf, n)) {
            const char *name = av_get_pix_fmt_name(imgfmt2pixfmt(n));
            if (name) {
                const char *s = fmtstr[0] ? FMTSEP : "";
                fmtstr = talloc_asprintf_append_buffer(fmtstr, "%s%s", s, name);
            }
        }
    }

    char *sws_flags = talloc_asprintf(tmp, "flags=%"PRId64, p->cfg_sws_flags);
    graph->scale_sws_opts = av_strdup(sws_flags);

    AVRational par = par_from_sar_dar(width, height, d_width, d_height);
    AVRational timebase = AV_TIME_BASE_Q;

    char *src_args = talloc_asprintf(tmp, "%d:%d:%d:%d:%d:%d:%d",
                                     width, height, imgfmt2pixfmt(fmt),
                                     timebase.num, timebase.den,
                                     par.num, par.den);

    if (avfilter_graph_create_filter(&in, avfilter_get_by_name("buffer"),
                                     "src", src_args, NULL, graph) < 0)
        goto error;

    if (avfilter_graph_create_filter(&out, avfilter_get_by_name("buffersink"),
                                     "out", NULL, NULL, graph) < 0)
        goto error;

    if (avfilter_graph_create_filter(&f_format, avfilter_get_by_name("format"),
                                     "format", fmtstr, NULL, graph) < 0)
        goto error;

    if (avfilter_link(f_format, 0, out, 0) < 0)
        goto error;

    outputs->name    = av_strdup("in");
    outputs->filter_ctx = in;

    inputs->name    = av_strdup("out");
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
    mp_msg(MSGT_VFILTER, MSGL_FATAL, "Can't configure libavfilter graph.\n");
    avfilter_graph_free(&graph);
    talloc_free(tmp);
    return false;
}

static int config(struct vf_instance *vf, int width, int height,
                  int d_width, int d_height, unsigned int flags,
                  unsigned int fmt)
{
    struct vf_priv_s *p = vf->priv;

    if (!recreate_graph(vf, width, height, d_width, d_height, fmt))
        return 0;

    AVFilterLink *l_out = p->out->inputs[0];
    AVFilterLink *l_in = p->in->outputs[0];

    p->timebase_in = l_in->time_base;
    p->timebase_out = l_out->time_base;

    p->par_in = l_in->sample_aspect_ratio;

    int dw, dh;
    dar_from_sar_par(l_out->w, l_out->h, l_out->sample_aspect_ratio, &dw, &dh);

    return vf_next_config(vf, l_out->w, l_out->h, dw, dh, flags,
                          pixfmt2imgfmt(l_out->format));

}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    // We accept all sws-convertable formats as inputs. Output formats are
    // handled in config(). The current public libavfilter API doesn't really
    // allow us to do anything more sophisticated.
    // This breaks with filters which accept input pixel formats not
    // supported by libswscale.
    return mp_sws_supported_format(fmt) ? VFCAP_CSP_SUPPORTED : 0;
}

static AVFrame *mp_to_av(struct vf_instance *vf, struct mp_image *img)
{
    struct vf_priv_s *p = vf->priv;
    uint64_t pts = img->pts == MP_NOPTS_VALUE ?
                   AV_NOPTS_VALUE : img->pts * av_q2d(av_inv_q(p->timebase_in));
    AVFrame *frame = mp_image_to_av_frame_and_unref(img);
    frame->pts = pts;
    frame->sample_aspect_ratio = p->par_in;
    return frame;
}

static struct mp_image *av_to_mp(struct vf_instance *vf, AVFrame *av_frame)
{
    struct vf_priv_s *p = vf->priv;
    struct mp_image *img = mp_image_from_av_frame(av_frame);
    img->pts = av_frame->pts == AV_NOPTS_VALUE ?
               MP_NOPTS_VALUE : av_frame->pts * av_q2d(p->timebase_out);
    av_frame_free(&av_frame);
    return img;
}

static int filter_ext(struct vf_instance *vf, struct mp_image *mpi)
{
    struct vf_priv_s *p = vf->priv;

    if (!p->graph)
        return -1;

    AVFrame *frame = mp_to_av(vf, mpi);
    if (av_buffersrc_add_frame(p->in, frame) < 0) {
        av_frame_free(&frame);
        return -1;
    }
    av_frame_free(&frame);

    for (;;) {
        frame = av_frame_alloc();
        if (av_buffersink_get_frame(p->out, frame) < 0) {
            // Not an error situation - no more output buffers in queue.
            av_frame_free(&frame);
            break;
        }
        vf_add_output_frame(vf, av_to_mp(vf, frame));
    }

    return 0;
}

static void reset(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;
    struct mp_image_params *f = &vf->fmt_in;
    if (p->graph && f->imgfmt)
        recreate_graph(vf, f->w, f->h, f->d_w, f->d_h, f->imgfmt);
}

static int control(vf_instance_t *vf, int request, void *data)
{
    switch (request) {
    case VFCTRL_SEEK_RESET:
        reset(vf);
        return CONTROL_OK;
    }
    return CONTROL_UNKNOWN;
}

static void uninit(struct vf_instance *vf)
{
    if (!vf->priv)
        return;
    destroy_graph(vf);
}

static int vf_open(vf_instance_t *vf)
{
    vf->reconfig = NULL;
    vf->config = config;
    vf->filter_ext = filter_ext;
    vf->filter = NULL;
    vf->query_format = query_format;
    vf->control = control;
    vf->uninit = uninit;
    return 1;
}

static bool is_single_video_only(const AVFilterPad *pads)
{
    int count = avfilter_pad_count(pads);
    if (count != 1)
        return false;
    return avfilter_pad_get_type(pads, 0) == AVMEDIA_TYPE_VIDEO;
}

// Does it have exactly one video input and one video output?
static bool is_usable(const AVFilter *filter)
{
    return is_single_video_only(filter->inputs) &&
           is_single_video_only(filter->outputs);
}

static void print_help(void)
{
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, "List of libavfilter filters:\n");
    for (const AVFilter *filter = avfilter_next(NULL); filter;
         filter = avfilter_next(filter))
    {
        if (is_usable(filter)) {
            mp_msg(MSGT_CFGPARSER, MSGL_INFO, " %-16s %s\n",
                   filter->name, filter->description);
        }
    }
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n"
        "This lists video->video filters only. Refer to\n"
        "\n"
        " https://ffmpeg.org/ffmpeg-filters.html\n"
        "\n"
        "to see how to use each filter and what arguments each filter takes.\n"
        "Also, be sure to quote the FFmpeg filter string properly, e.g.:\n"
        "\n"
        " \"--vf=lavfi=[gradfun=20:30]\"\n"
        "\n"
        "Otherwise, mpv and libavfilter syntax will conflict.\n"
        "\n");
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_STRING("graph", cfg_graph, M_OPT_MIN, .min = 1),
    OPT_INT64("sws-flags", cfg_sws_flags, 0),
    OPT_STRING("o", cfg_avopts, 0),
    {0}
};

const vf_info_t vf_info_lavfi = {
    .description = "libavfilter bridge",
    .name = "lavfi",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_dflt,
    .options = vf_opts_fields,
    .print_help = print_help,
};

// The following code is for the old filters wrapper code.

struct vf_lw_opts {
    int enable;
    int64_t sws_flags;
    char *avopts;
};

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct vf_lw_opts
const struct m_sub_options vf_lw_conf = {
    .opts = (const m_option_t[]) {
        OPT_FLAG("lavfi", enable, 0),
        OPT_INT64("lavfi-sws-flags", sws_flags, 0),
        OPT_STRING("lavfi-o", avopts, 0),
        {0}
    },
    .defaults = &(const struct vf_lw_opts){
        .enable = 1,
        .sws_flags = SWS_BICUBIC,
    },
    .size = sizeof(struct vf_lw_opts),
};

static bool have_filter(const char *name)
{
    for (const AVFilter *filter = avfilter_next(NULL); filter;
         filter = avfilter_next(filter))
    {
        if (strcmp(filter->name, name) == 0)
            return true;
    }
    return false;
}

// This is used by "old" filters for wrapping lavfi if possible.
// On success, this overwrites all vf callbacks and literally takes over the
// old filter and replaces it with vf_lavfi.
// On error (<0), nothing is changed.
int vf_lw_set_graph(struct vf_instance *vf, struct vf_lw_opts *lavfi_opts,
                    char *filter, char *opts, ...)
{
    if (!lavfi_opts)
        lavfi_opts = (struct vf_lw_opts *)vf_lw_conf.defaults;
    if (!lavfi_opts->enable || !have_filter(filter))
        return -1;
    mp_msg(MSGT_VFILTER, MSGL_V, "Using libavfilter for '%s'\n", vf->info->name);
    void *old_priv = vf->priv;
    struct vf_priv_s *p = talloc(vf, struct vf_priv_s);
    vf->priv = p;
    *p = vf_priv_dflt;
    p->cfg_sws_flags = lavfi_opts->sws_flags;
    p->cfg_avopts = lavfi_opts->avopts;
    va_list ap;
    va_start(ap, opts);
    char *s = talloc_vasprintf(vf, opts, ap);
    p->cfg_graph = talloc_asprintf(vf, "%s=%s", filter, s);
    talloc_free(s);
    va_end(ap);
    p->old_priv = old_priv;
    // Note: we should be sure vf_open really overwrites _all_ vf callbacks.
    if (vf_open(vf) < 1)
        abort();
    return 1;
}

void *vf_lw_old_priv(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    return p->old_priv;
}

void vf_lw_update_graph(struct vf_instance *vf, char *filter, char *opts, ...)
{
    struct vf_priv_s *p = vf->priv;
    va_list ap;
    va_start(ap, opts);
    char *s = talloc_vasprintf(vf, opts, ap);
    talloc_free(p->cfg_graph);
    p->cfg_graph = talloc_asprintf(vf, "%s=%s", filter, s);
    talloc_free(s);
    va_end(ap);
}

void vf_lw_set_recreate_cb(struct vf_instance *vf,
                           void (*recreate)(struct vf_instance *vf))
{
    vf->priv->lw_recreate_cb = recreate;
}
