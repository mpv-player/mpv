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

#include <assert.h>

#include <va/va.h>
#include <va/va_vpp.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

#include "config.h"
#include "options/options.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "refqueue.h"

#include "video/fmt-conversion.h"
#include "video/vaapi.h"
#include "video/hwdec.h"
#include "video/mp_image_pool.h"

struct surface_refs {
    VASurfaceID *surfaces;
    int num_surfaces;
    int max_surfaces;
};

struct pipeline {
    VABufferID *filters;
    int num_filters;
    VAProcColorStandardType input_colors[VAProcColorStandardCount];
    VAProcColorStandardType output_colors[VAProcColorStandardCount];
    int num_input_colors, num_output_colors;
    struct surface_refs forward, backward;
};

struct opts {
    int deint_type;
    int interlaced_only;
    int reversal_bug;
};

struct priv {
    struct opts *opts;
    bool do_deint;
    VABufferID buffers[VAProcFilterCount];
    int num_buffers;
    VAConfigID config;
    VAContextID context;
    struct mp_image_params params;
    VADisplay display;
    AVBufferRef *av_device_ref;
    struct pipeline pipe;
    AVBufferRef *hw_pool;

    struct mp_refqueue *queue;
};

static void add_surfaces(struct priv *p, struct surface_refs *refs, int dir)
{
    for (int n = 0; n < refs->max_surfaces; n++) {
        struct mp_image *s = mp_refqueue_get(p->queue, (1 + n) * dir);
        if (!s)
            break;
        VASurfaceID id = va_surface_id(s);
        if (id == VA_INVALID_ID)
            break;
        MP_TARRAY_APPEND(p, refs->surfaces, refs->num_surfaces, id);
    }
}

// The array items must match with the "deint" suboption values.
// They're also sorted by quality.
static const int deint_algorithm[] = {
    [0] = VAProcDeinterlacingNone,
    [1] = VAProcDeinterlacingBob, // first-field, special-cased
    [2] = VAProcDeinterlacingBob,
    [3] = VAProcDeinterlacingWeave,
    [4] = VAProcDeinterlacingMotionAdaptive,
    [5] = VAProcDeinterlacingMotionCompensated,
};

static void flush_frames(struct mp_filter *f)
{
    struct priv *p = f->priv;
    mp_refqueue_flush(p->queue);
}

static void update_pipeline(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    VABufferID *filters = p->buffers;
    int num_filters = p->num_buffers;
    if (p->opts->deint_type && !p->do_deint) {
        filters++;
        num_filters--;
    }
    p->pipe.forward.num_surfaces = p->pipe.backward.num_surfaces = 0;
    p->pipe.num_input_colors = p->pipe.num_output_colors = 0;
    p->pipe.num_filters = 0;
    p->pipe.filters = NULL;
    if (!num_filters)
        goto nodeint;
    VAProcPipelineCaps caps = {
        .input_color_standards = p->pipe.input_colors,
        .output_color_standards = p->pipe.output_colors,
        .num_input_color_standards = VAProcColorStandardCount,
        .num_output_color_standards = VAProcColorStandardCount,
    };
    VAStatus status = vaQueryVideoProcPipelineCaps(p->display, p->context,
                                                   filters, num_filters, &caps);
    if (!CHECK_VA_STATUS(vf, "vaQueryVideoProcPipelineCaps()"))
        goto nodeint;
    p->pipe.filters = filters;
    p->pipe.num_filters = num_filters;
    p->pipe.num_input_colors = caps.num_input_color_standards;
    p->pipe.num_output_colors = caps.num_output_color_standards;
    p->pipe.forward.max_surfaces = caps.num_forward_references;
    p->pipe.backward.max_surfaces = caps.num_backward_references;
    if (p->opts->reversal_bug) {
        int max = MPMAX(caps.num_forward_references, caps.num_backward_references);
        mp_refqueue_set_refs(p->queue, max, max);
    } else {
        mp_refqueue_set_refs(p->queue, p->pipe.backward.max_surfaces,
                                       p->pipe.forward.max_surfaces);
    }
    mp_refqueue_set_mode(p->queue,
        (p->do_deint ? MP_MODE_DEINT : 0) |
        (p->opts->deint_type >= 2 ? MP_MODE_OUTPUT_FIELDS : 0) |
        (p->opts->interlaced_only ? MP_MODE_INTERLACED_ONLY : 0));
    return;

nodeint:
    mp_refqueue_set_refs(p->queue, 0, 0);
    mp_refqueue_set_mode(p->queue, 0);
}

static struct mp_image *alloc_out(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    struct mp_image *fmt = mp_refqueue_get_format(p->queue);
    if (!fmt || !fmt->hwctx)
        return NULL;

    AVHWFramesContext *hw_frames = (void *)fmt->hwctx->data;
    // VAAPI requires the full surface size to match for input and output.
    int src_w = hw_frames->width;
    int src_h = hw_frames->height;

    if (!mp_update_av_hw_frames_pool(&p->hw_pool, p->av_device_ref,
                                     IMGFMT_VAAPI, IMGFMT_NV12, src_w, src_h))
    {
        MP_ERR(vf, "Failed to create hw pool.\n");
        return NULL;
    }

    AVFrame *av_frame = av_frame_alloc();
    if (!av_frame)
        abort();
    if (av_hwframe_get_buffer(p->hw_pool, av_frame, 0) < 0) {
        MP_ERR(vf, "Failed to allocate frame from hw pool.\n");
        av_frame_free(&av_frame);
        return NULL;
    }
    struct mp_image *img = mp_image_from_av_frame(av_frame);
    av_frame_free(&av_frame);
    if (!img) {
        MP_ERR(vf, "Unknown error.\n");
        return NULL;
    }
    mp_image_set_size(img, fmt->w, fmt->h);
    return img;
}

static struct mp_image *render(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    struct mp_image *in = mp_refqueue_get(p->queue, 0);
    struct mp_image *img = NULL;
    bool need_end_picture = false;
    bool success = false;
    VABufferID buffer = VA_INVALID_ID;

    VASurfaceID in_id = va_surface_id(in);
    if (!p->pipe.filters || in_id == VA_INVALID_ID)
        goto cleanup;

    img = alloc_out(vf);
    if (!img)
        goto cleanup;

    mp_image_copy_attributes(img, in);

    unsigned int flags = va_get_colorspace_flag(p->params.color.space);
    if (!mp_refqueue_should_deint(p->queue)) {
        flags |= VA_FRAME_PICTURE;
    } else if (mp_refqueue_is_top_field(p->queue)) {
        flags |= VA_TOP_FIELD;
    } else {
        flags |= VA_BOTTOM_FIELD;
    }

    VASurfaceID id = va_surface_id(img);
    if (id == VA_INVALID_ID)
        goto cleanup;

    VAStatus status = vaBeginPicture(p->display, p->context, id);
    if (!CHECK_VA_STATUS(vf, "vaBeginPicture()"))
        goto cleanup;

    need_end_picture = true;

    VAProcPipelineParameterBuffer *param = NULL;
    status = vaCreateBuffer(p->display, p->context,
                            VAProcPipelineParameterBufferType,
                            sizeof(*param), 1, NULL, &buffer);
    if (!CHECK_VA_STATUS(vf, "vaCreateBuffer()"))
        goto cleanup;

    VAProcFilterParameterBufferDeinterlacing *filter_params;
    status = vaMapBuffer(p->display, *(p->pipe.filters), (void**)&filter_params);
    if (!CHECK_VA_STATUS(vf, "vaMapBuffer()"))
        goto cleanup;

    filter_params->flags = flags & VA_TOP_FIELD ? 0 : VA_DEINTERLACING_BOTTOM_FIELD;
    if (!mp_refqueue_top_field_first(p->queue))
        filter_params->flags |= VA_DEINTERLACING_BOTTOM_FIELD_FIRST;

    vaUnmapBuffer(p->display, *(p->pipe.filters));

    status = vaMapBuffer(p->display, buffer, (void**)&param);
    if (!CHECK_VA_STATUS(vf, "vaMapBuffer()"))
        goto cleanup;

    *param = (VAProcPipelineParameterBuffer){0};
    param->surface = in_id;
    param->surface_region = &(VARectangle){0, 0, in->w, in->h};
    param->output_region = &(VARectangle){0, 0, img->w, img->h};
    param->output_background_color = 0;
    param->filter_flags = flags;
    param->filters = p->pipe.filters;
    param->num_filters = p->pipe.num_filters;

    int dir = p->opts->reversal_bug ? -1 : 1;

    add_surfaces(p, &p->pipe.forward, 1 * dir);
    param->forward_references = p->pipe.forward.surfaces;
    param->num_forward_references = p->pipe.forward.num_surfaces;

    add_surfaces(p, &p->pipe.backward, -1 * dir);
    param->backward_references = p->pipe.backward.surfaces;
    param->num_backward_references = p->pipe.backward.num_surfaces;

    MP_TRACE(vf, "in=0x%x\n", (unsigned)in_id);
    for (int n = 0; n < param->num_backward_references; n++)
        MP_TRACE(vf, " b%d=0x%x\n", n, (unsigned)param->backward_references[n]);
    for (int n = 0; n < param->num_forward_references; n++)
        MP_TRACE(vf, " f%d=0x%x\n", n, (unsigned)param->forward_references[n]);

    vaUnmapBuffer(p->display, buffer);

    status = vaRenderPicture(p->display, p->context, &buffer, 1);
    if (!CHECK_VA_STATUS(vf, "vaRenderPicture()"))
        goto cleanup;

    success = true;

cleanup:
    if (need_end_picture)
        vaEndPicture(p->display, p->context);
    vaDestroyBuffer(p->display, buffer);
    if (success)
        return img;
    talloc_free(img);
    return NULL;
}

static void vf_vavpp_process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    update_pipeline(f);

    mp_refqueue_execute_reinit(p->queue);

    if (!mp_refqueue_can_output(p->queue))
        return;

    if (!p->pipe.num_filters || !mp_refqueue_should_deint(p->queue)) {
        // no filtering
        struct mp_image *in = mp_refqueue_get(p->queue, 0);
        mp_refqueue_write_out_pin(p->queue, mp_image_new_ref(in));
    } else {
        mp_refqueue_write_out_pin(p->queue, render(f));
    }
}

static void uninit(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    for (int i = 0; i < p->num_buffers; i++)
        vaDestroyBuffer(p->display, p->buffers[i]);
    if (p->context != VA_INVALID_ID)
        vaDestroyContext(p->display, p->context);
    if (p->config != VA_INVALID_ID)
        vaDestroyConfig(p->display, p->config);
    av_buffer_unref(&p->hw_pool);
    flush_frames(vf);
    talloc_free(p->queue);
    av_buffer_unref(&p->av_device_ref);
}

static int va_query_filter_caps(struct mp_filter *vf, VAProcFilterType type,
                                void *caps, unsigned int count)
{
    struct priv *p = vf->priv;
    VAStatus status = vaQueryVideoProcFilterCaps(p->display, p->context, type,
                                                 caps, &count);
    return CHECK_VA_STATUS(vf, "vaQueryVideoProcFilterCaps()") ? count : 0;
}

static VABufferID va_create_filter_buffer(struct mp_filter *vf, int bytes,
                                          int num, void *data)
{
    struct priv *p = vf->priv;
    VABufferID buffer;
    VAStatus status = vaCreateBuffer(p->display, p->context,
                                     VAProcFilterParameterBufferType,
                                     bytes, num, data, &buffer);
    return CHECK_VA_STATUS(vf, "vaCreateBuffer()") ? buffer : VA_INVALID_ID;
}

static bool initialize(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    VAStatus status;

    VAConfigID config;
    status = vaCreateConfig(p->display, VAProfileNone, VAEntrypointVideoProc,
                            NULL, 0, &config);
    if (!CHECK_VA_STATUS(vf, "vaCreateConfig()")) // no entrypoint for video porc
        return false;
    p->config = config;

    VAContextID context;
    status = vaCreateContext(p->display, p->config, 0, 0, 0, NULL, 0, &context);
    if (!CHECK_VA_STATUS(vf, "vaCreateContext()"))
        return false;
    p->context = context;

    VAProcFilterType filters[VAProcFilterCount];
    int num_filters = VAProcFilterCount;
    status = vaQueryVideoProcFilters(p->display, p->context, filters, &num_filters);
    if (!CHECK_VA_STATUS(vf, "vaQueryVideoProcFilters()"))
        return false;

    VABufferID buffers[VAProcFilterCount];
    for (int i = 0; i < VAProcFilterCount; i++)
        buffers[i] = VA_INVALID_ID;
    for (int i = 0; i < num_filters; i++) {
        if (filters[i] == VAProcFilterDeinterlacing) {
            VAProcFilterCapDeinterlacing caps[VAProcDeinterlacingCount];
            int num = va_query_filter_caps(vf, VAProcFilterDeinterlacing, caps,
                                           VAProcDeinterlacingCount);
            if (!num)
                continue;
            if (p->opts->deint_type < 0) {
                for (int n = MP_ARRAY_SIZE(deint_algorithm) - 1; n > 0; n--) {
                    for (int x = 0; x < num; x++) {
                        if (caps[x].type == deint_algorithm[n]) {
                            p->opts->deint_type = n;
                            MP_VERBOSE(vf, "Selected deinterlacing algorithm: "
                                       "%d\n", deint_algorithm[n]);
                            goto found;
                        }
                    }
                }
                found: ;
            }
            if (p->opts->deint_type <= 0)
                continue;
            VAProcDeinterlacingType algorithm =
                deint_algorithm[p->opts->deint_type];
            for (int n=0; n < num; n++) { // find the algorithm
                if (caps[n].type != algorithm)
                    continue;
                VAProcFilterParameterBufferDeinterlacing param = {0};
                param.type = VAProcFilterDeinterlacing;
                param.algorithm = algorithm;
                buffers[VAProcFilterDeinterlacing] =
                    va_create_filter_buffer(vf, sizeof(param), 1, &param);
            }
            if (buffers[VAProcFilterDeinterlacing] == VA_INVALID_ID)
                MP_WARN(vf, "Selected deinterlacing algorithm not supported.\n");
        } // check other filters
    }
    if (p->opts->deint_type < 0)
        p->opts->deint_type = 0;
    p->num_buffers = 0;
    if (buffers[VAProcFilterDeinterlacing] != VA_INVALID_ID)
        p->buffers[p->num_buffers++] = buffers[VAProcFilterDeinterlacing];
    p->do_deint = !!p->opts->deint_type;
    // next filters: p->buffers[p->num_buffers++] = buffers[next_filter];
    return true;
}

static const struct mp_filter_info vf_vavpp_filter = {
    .name = "vavpp",
    .process = vf_vavpp_process,
    .reset = flush_frames,
    .destroy = uninit,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_vavpp_create(struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &vf_vavpp_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);
    p->config = VA_INVALID_ID;
    p->context = VA_INVALID_ID;

    p->queue = mp_refqueue_alloc(f);

    p->av_device_ref = mp_filter_load_hwdec_device(f, AV_HWDEVICE_TYPE_VAAPI);
    if (!p->av_device_ref)
        goto error;

    AVHWDeviceContext *hwctx = (void *)p->av_device_ref->data;
    AVVAAPIDeviceContext *vactx = hwctx->hwctx;

    p->display = vactx->display;

    mp_refqueue_add_in_format(p->queue, IMGFMT_VAAPI, 0);

    if (!initialize(f))
        goto error;

    return f;

error:
    talloc_free(f);
    return NULL;
}

#define OPT_BASE_STRUCT struct opts
static const m_option_t vf_opts_fields[] = {
    {"deint", OPT_CHOICE(deint_type,
        // The values >=0 must match with deint_algorithm[].
        {"auto", -1},
        {"no", 0},
        {"first-field", 1},
        {"bob", 2},
        {"weave", 3},
        {"motion-adaptive", 4},
        {"motion-compensated", 5})},
    {"interlaced-only", OPT_FLAG(interlaced_only)},
    {"reversal-bug", OPT_FLAG(reversal_bug)},
    {0}
};

const struct mp_user_filter_entry vf_vavpp = {
    .desc = {
        .description = "VA-API Video Post-Process Filter",
        .name = "vavpp",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT){
            .deint_type = -1,
            .interlaced_only = 0,
            .reversal_bug = 1,
        },
        .options = vf_opts_fields,
    },
    .create = vf_vavpp_create,
};
