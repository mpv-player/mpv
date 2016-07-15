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

#include "config.h"
#include "options/options.h"
#include "vf.h"
#include "refqueue.h"
#include "video/vaapi.h"
#include "video/hwdec.h"
#include "video/mp_image_pool.h"

static bool check_error(struct vf_instance *vf, VAStatus status, const char *msg)
{
    if (status == VA_STATUS_SUCCESS)
        return true;
    MP_ERR(vf, "%s: %s\n", msg, vaErrorStr(status));
    return false;
}

struct surface_refs {
    VASurfaceID *surfaces;
    int num_surfaces;
};

struct pipeline {
    VABufferID *filters;
    int num_filters;
    VAProcColorStandardType input_colors[VAProcColorStandardCount];
    VAProcColorStandardType output_colors[VAProcColorStandardCount];
    int num_input_colors, num_output_colors;
    struct surface_refs forward, backward;
};

struct vf_priv_s {
    int deint_type; // 0: none, 1: discard, 2: double fps
    int interlaced_only;
    bool do_deint;
    VABufferID buffers[VAProcFilterCount];
    int num_buffers;
    VAConfigID config;
    VAContextID context;
    struct mp_image_params params;
    VADisplay display;
    struct mp_vaapi_ctx *va;
    struct pipeline pipe;
    struct mp_image_pool *pool;
    int current_rt_format;

    struct mp_refqueue *queue;
};

static const struct vf_priv_s vf_priv_default = {
    .config = VA_INVALID_ID,
    .context = VA_INVALID_ID,
    .deint_type = 2,
    .interlaced_only = 1,
};

static void add_surfaces(struct vf_priv_s *p, struct surface_refs *refs, int dir)
{
    for (int n = 0; ; n++) {
        struct mp_image *s = mp_refqueue_get(p->queue, (1 + n) * dir);
        if (!s)
            break;
        VASurfaceID id = va_surface_id(s);
        if (id != VA_INVALID_ID)
            MP_TARRAY_APPEND(p, refs->surfaces, refs->num_surfaces, id);
    }
}

// The array items must match with the "deint" suboption values.
static const int deint_algorithm[] = {
    [0] = VAProcDeinterlacingNone,
    [1] = VAProcDeinterlacingNone, // first-field, special-cased
    [2] = VAProcDeinterlacingBob,
    [3] = VAProcDeinterlacingWeave,
    [4] = VAProcDeinterlacingMotionAdaptive,
    [5] = VAProcDeinterlacingMotionCompensated,
};

static void flush_frames(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    mp_refqueue_flush(p->queue);
}

static void update_pipeline(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    VABufferID *filters = p->buffers;
    int num_filters = p->num_buffers;
    if (p->deint_type && !p->do_deint) {
        filters++;
        num_filters--;
    }
    if (filters == p->pipe.filters && num_filters == p->pipe.num_filters)
        return; /* cached state is correct */
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
    if (!check_error(vf, status, "vaQueryVideoProcPipelineCaps()"))
        goto nodeint;
    p->pipe.filters = filters;
    p->pipe.num_filters = num_filters;
    p->pipe.num_input_colors = caps.num_input_color_standards;
    p->pipe.num_output_colors = caps.num_output_color_standards;
    mp_refqueue_set_refs(p->queue, caps.num_backward_references,
                                   caps.num_forward_references);
    mp_refqueue_set_mode(p->queue,
        (p->do_deint ? MP_MODE_DEINT : 0) |
        (p->deint_type >= 2 ? MP_MODE_OUTPUT_FIELDS : 0) |
        (p->interlaced_only ? MP_MODE_INTERLACED_ONLY : 0));
    return;

nodeint:
    mp_refqueue_set_refs(p->queue, 0, 0);
    mp_refqueue_set_mode(p->queue, 0);
}

static struct mp_image *render(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    struct mp_image *in = mp_refqueue_get(p->queue, 0);
    struct mp_image *img = NULL;
    bool need_end_picture = false;
    bool success = false;

    VASurfaceID in_id = va_surface_id(in);
    if (!p->pipe.filters || in_id == VA_INVALID_ID)
        goto cleanup;

    int r_w, r_h;
    va_surface_get_uncropped_size(in, &r_w, &r_h);
    img = mp_image_pool_get(p->pool, IMGFMT_VAAPI, r_w, r_h);
    if (!img)
        goto cleanup;
    mp_image_set_size(img, in->w, in->h);
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
    if (!check_error(vf, status, "vaBeginPicture()"))
        goto cleanup;

    need_end_picture = true;

    VABufferID buffer = VA_INVALID_ID;
    VAProcPipelineParameterBuffer *param = NULL;
    status = vaCreateBuffer(p->display, p->context,
                            VAProcPipelineParameterBufferType,
                            sizeof(*param), 1, NULL, &buffer);
    if (!check_error(vf, status, "vaCreateBuffer()"))
        goto cleanup;

    VAProcFilterParameterBufferDeinterlacing *filter_params;
    status = vaMapBuffer(p->display, *(p->pipe.filters), (void**)&filter_params);
    if (!check_error(vf, status, "vaMapBuffer()"))
        goto cleanup;

    filter_params->flags = flags & VA_TOP_FIELD ? 0 : VA_DEINTERLACING_BOTTOM_FIELD;
    if (!mp_refqueue_top_field_first(p->queue))
        filter_params->flags |= VA_DEINTERLACING_BOTTOM_FIELD_FIRST;

    vaUnmapBuffer(p->display, *(p->pipe.filters));

    status = vaMapBuffer(p->display, buffer, (void**)&param);
    if (!check_error(vf, status, "vaMapBuffer()"))
        goto cleanup;

    param->surface = in_id;
    param->surface_region = &(VARectangle){0, 0, in->w, in->h};
    param->output_region = &(VARectangle){0, 0, img->w, img->h};
    param->output_background_color = 0;
    param->filter_flags = flags;
    param->filters = p->pipe.filters;
    param->num_filters = p->pipe.num_filters;

    add_surfaces(p, &p->pipe.forward, 1);
    param->forward_references = p->pipe.forward.surfaces;
    param->num_forward_references = p->pipe.forward.num_surfaces;

    add_surfaces(p, &p->pipe.backward, -1);
    param->backward_references = p->pipe.backward.surfaces;
    param->num_backward_references = p->pipe.backward.num_surfaces;

    vaUnmapBuffer(p->display, buffer);

    status = vaRenderPicture(p->display, p->context, &buffer, 1);
    if (!check_error(vf, status, "vaRenderPicture()"))
        goto cleanup;

    success = true;

cleanup:
    if (need_end_picture)
        vaEndPicture(p->display, p->context);
    if (success)
        return img;
    talloc_free(img);
    return NULL;
}

static struct mp_image *upload(struct vf_instance *vf, struct mp_image *in)
{
    struct vf_priv_s *p = vf->priv;
    struct mp_image *out = mp_image_pool_get(p->pool, IMGFMT_VAAPI, in->w, in->h);
    if (!out)
        return NULL;
    if (va_surface_upload(out, in) < 0) {
        talloc_free(out);
        return NULL;
    }
    mp_image_copy_attributes(out, in);
    return out;
}

static int filter_ext(struct vf_instance *vf, struct mp_image *in)
{
    struct vf_priv_s *p = vf->priv;

    update_pipeline(vf);

    if (in && in->imgfmt != IMGFMT_VAAPI) {
        struct mp_image *tmp = upload(vf, in);
        talloc_free(in);
        in = tmp;
        if (!in)
            return -1;
    }

    mp_refqueue_add_input(p->queue, in);
    return 0;
}

static int filter_out(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    if (!mp_refqueue_has_output(p->queue))
        return 0;

    // no filtering
    if (!p->pipe.num_filters || !mp_refqueue_should_deint(p->queue)) {
        struct mp_image *in = mp_refqueue_get(p->queue, 0);
        vf_add_output_frame(vf, mp_image_new_ref(in));
        mp_refqueue_next(p->queue);
        return 0;
    }

    struct mp_image *out = render(vf);
    mp_refqueue_next_field(p->queue);
    if (!out)
        return -1; // cannot render
    vf_add_output_frame(vf, out);
    return 0;
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    struct vf_priv_s *p = vf->priv;

    flush_frames(vf);
    talloc_free(p->pool);
    p->pool = NULL;

    p->params = *in;

    p->current_rt_format = VA_RT_FORMAT_YUV420;
    p->pool = mp_image_pool_new(20);
    va_pool_set_allocator(p->pool, p->va, p->current_rt_format);

    struct mp_image *probe = mp_image_pool_get(p->pool, IMGFMT_VAAPI, in->w, in->h);
    if (!probe)
        return -1;
    va_surface_init_subformat(probe);
    *out = *in;
    out->imgfmt = probe->params.imgfmt;
    out->hw_subfmt = probe->params.hw_subfmt;
    talloc_free(probe);

    return 0;
}

static void uninit(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    for (int i = 0; i < p->num_buffers; i++)
        vaDestroyBuffer(p->display, p->buffers[i]);
    if (p->context != VA_INVALID_ID)
        vaDestroyContext(p->display, p->context);
    if (p->config != VA_INVALID_ID)
        vaDestroyConfig(p->display, p->config);
    talloc_free(p->pool);
    flush_frames(vf);
    mp_refqueue_free(p->queue);
}

static int query_format(struct vf_instance *vf, unsigned int imgfmt)
{
    struct vf_priv_s *p = vf->priv;
    if (imgfmt == IMGFMT_VAAPI || va_image_format_from_imgfmt(p->va, imgfmt))
        return vf_next_query_format(vf, IMGFMT_VAAPI);
    return 0;
}

static int control(struct vf_instance *vf, int request, void* data)
{
    struct vf_priv_s *p = vf->priv;
    switch (request){
    case VFCTRL_GET_DEINTERLACE:
        *(int*)data = !!p->do_deint;
        return true;
    case VFCTRL_SET_DEINTERLACE:
        p->do_deint = *(int*)data;
        return true;
    case VFCTRL_SEEK_RESET:
        flush_frames(vf);
        return true;
    default:
        return CONTROL_UNKNOWN;
    }
}

static int va_query_filter_caps(struct vf_instance *vf, VAProcFilterType type,
                                void *caps, unsigned int count)
{
    struct vf_priv_s *p = vf->priv;
    VAStatus status = vaQueryVideoProcFilterCaps(p->display, p->context, type,
                                                 caps, &count);
    return check_error(vf, status, "vaQueryVideoProcFilterCaps()") ? count : 0;
}

static VABufferID va_create_filter_buffer(struct vf_instance *vf, int bytes,
                                          int num, void *data)
{
    struct vf_priv_s *p = vf->priv;
    VABufferID buffer;
    VAStatus status = vaCreateBuffer(p->display, p->context,
                                     VAProcFilterParameterBufferType,
                                     bytes, num, data, &buffer);
    return check_error(vf, status, "vaCreateBuffer()") ? buffer : VA_INVALID_ID;
}

static bool initialize(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    VAStatus status;

    VAConfigID config;
    status = vaCreateConfig(p->display, VAProfileNone, VAEntrypointVideoProc,
                            NULL, 0, &config);
    if (!check_error(vf, status, "vaCreateConfig()")) // no entrypoint for video porc
        return false;
    p->config = config;

    VAContextID context;
    status = vaCreateContext(p->display, p->config, 0, 0, 0, NULL, 0, &context);
    if (!check_error(vf, status, "vaCreateContext()"))
        return false;
    p->context = context;

    VAProcFilterType filters[VAProcFilterCount];
    int num_filters = VAProcFilterCount;
    status = vaQueryVideoProcFilters(p->display, p->context, filters, &num_filters);
    if (!check_error(vf, status, "vaQueryVideoProcFilters()"))
        return false;

    VABufferID buffers[VAProcFilterCount];
    for (int i = 0; i < VAProcFilterCount; i++)
        buffers[i] = VA_INVALID_ID;
    for (int i = 0; i < num_filters; i++) {
        if (filters[i] == VAProcFilterDeinterlacing) {
            if (p->deint_type < 2)
                continue;
            VAProcFilterCapDeinterlacing caps[VAProcDeinterlacingCount];
            int num = va_query_filter_caps(vf, VAProcFilterDeinterlacing, caps,
                                           VAProcDeinterlacingCount);
            if (!num)
                continue;
            VAProcDeinterlacingType algorithm = deint_algorithm[p->deint_type];
            for (int n=0; n < num; n++) { // find the algorithm
                if (caps[n].type != algorithm)
                    continue;
                VAProcFilterParameterBufferDeinterlacing param;
                param.type = VAProcFilterDeinterlacing;
                param.algorithm = algorithm;
                buffers[VAProcFilterDeinterlacing] =
                    va_create_filter_buffer(vf, sizeof(param), 1, &param);
            }
            if (buffers[VAProcFilterDeinterlacing] == VA_INVALID_ID)
                MP_WARN(vf, "Selected deinterlacing algorithm not supported.\n");
        } // check other filters
    }
    p->num_buffers = 0;
    if (buffers[VAProcFilterDeinterlacing] != VA_INVALID_ID)
        p->buffers[p->num_buffers++] = buffers[VAProcFilterDeinterlacing];
    p->do_deint = !!p->deint_type;
    // next filters: p->buffers[p->num_buffers++] = buffers[next_filter];
    return true;
}

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    vf->reconfig = reconfig;
    vf->filter_ext = filter_ext;
    vf->filter_out = filter_out;
    vf->query_format = query_format;
    vf->uninit = uninit;
    vf->control = control;

    p->queue = mp_refqueue_alloc();

    p->va = hwdec_devices_load(vf->hwdec_devs, HWDEC_VAAPI);
    if (!p->va)
        return 0;
    p->display = p->va->display;
    if (initialize(vf))
        return true;
    uninit(vf);
    return false;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_CHOICE("deint", deint_type, 0,
               // The values must match with deint_algorithm[].
               ({"no", 0},
                {"first-field", 1},
                {"bob", 2},
                {"weave", 3},
                {"motion-adaptive", 4},
                {"motion-compensated", 5})),
    OPT_FLAG("interlaced-only", interlaced_only, 0),
    {0}
};

const vf_info_t vf_info_vaapi = {
    .description = "VA-API Video Post-Process Filter",
    .name = "vavpp",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_default,
    .options = vf_opts_fields,
};
