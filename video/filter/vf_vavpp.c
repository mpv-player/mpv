/*
 * This file is part of mpv.
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

#include "config.h"
#include "mpvcore/options.h"
#include "vf.h"
#include "video/vaapi.h"
#include "video/decode/dec_video.h"
#include <va/va.h>
#include <va/va_vpp.h>

static inline bool is_success(VAStatus status, const char *msg)
{
    if (status == VA_STATUS_SUCCESS)
        return true;
    mp_msg(MSGT_VFILTER, MSGL_ERR, "[vavpp] %s: %s\n", msg, vaErrorStr(status));
    return false;
}

struct surface_refs {
    VASurfaceID *surfaces;
    int num_allocated;
    int num_required;
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
    double prev_pts;
    int deint_type; // 0: none, 1: discard, 2: double fps
    bool do_deint;
    VABufferID buffers[VAProcFilterCount];
    int num_buffers;
    VAConfigID config;
    VAContextID context;
    struct mp_image_params params;
    VADisplay display;
    struct mp_vaapi_ctx *va;
    struct pipeline pipe;
    struct va_surface_pool *pool;
};

static const struct vf_priv_s vf_priv_default = {
    .prev_pts = MP_NOPTS_VALUE,
    .config = VA_INVALID_ID,
    .context = VA_INVALID_ID,
    .deint_type = 2,
};

static inline void realloc_refs(struct surface_refs *refs, int num)
{
    if (refs->num_allocated < num) {
        refs->surfaces = realloc(refs->surfaces, sizeof(VASurfaceID)*num);
        refs->num_allocated = num;
    }
    refs->num_required = num;
}

static bool update_pipeline(struct vf_priv_s *p, bool deint)
{
    VABufferID *filters = p->buffers;
    int num_filters = p->num_buffers;
    if (p->deint_type && !deint) {
        ++filters;
        --num_filters;
    }
    if (filters == p->pipe.filters && num_filters == p->pipe.num_filters)
        return true;
    p->pipe.forward.num_required = p->pipe.backward.num_required = 0;
    p->pipe.num_input_colors = p->pipe.num_output_colors = 0;
    p->pipe.num_filters = 0;
    p->pipe.filters = NULL;
    if (!num_filters)
        return false;
    VAProcPipelineCaps caps;
    caps.input_color_standards = p->pipe.input_colors;
    caps.output_color_standards = p->pipe.output_colors;
    caps.num_input_color_standards = VAProcColorStandardCount;
    caps.num_output_color_standards = VAProcColorStandardCount;
    VAStatus status = vaQueryVideoProcPipelineCaps(p->display, p->context, filters, num_filters, &caps);
    if (!is_success(status, "vaQueryVideoProcPipelineCaps()"))
        return false;
    p->pipe.filters = filters;
    p->pipe.num_filters = num_filters;
    p->pipe.num_input_colors = caps.num_input_color_standards;
    p->pipe.num_output_colors = caps.num_output_color_standards;
    realloc_refs(&p->pipe.forward, caps.num_forward_references);
    realloc_refs(&p->pipe.backward, caps.num_backward_references);
    return true;
}

static inline int get_deint_field(struct vf_priv_s *p, int i, const struct mp_image *mpi)
{
    if (!p->do_deint || !(mpi->fields & MP_IMGFIELD_INTERLACED))
        return VA_FRAME_PICTURE;
    return !!(mpi->fields & MP_IMGFIELD_TOP_FIRST) ^ i ? VA_TOP_FIELD : VA_BOTTOM_FIELD;
}

static struct mp_image *render(struct vf_priv_s *p, struct va_surface *in, uint flags)
{
    if (!p->pipe.filters || !in)
        return NULL;
    struct va_surface *out = va_surface_pool_get(p->pool, in->w, in->h);
    if (!out)
        return NULL;
    enum {Begun = 1, Rendered = 2};
    int state = 0;
    do { // not a loop, just for break
        VAStatus status = vaBeginPicture(p->display, p->context, out->id);
        if (!is_success(status, "vaBeginPicture()"))
            break;
        state |= Begun;
        VABufferID buffer = VA_INVALID_ID;
        VAProcPipelineParameterBuffer *param = NULL;
        status = vaCreateBuffer(p->display, p->context, VAProcPipelineParameterBufferType, sizeof(*param), 1, NULL, &buffer);
        if (!is_success(status, "vaCreateBuffer()"))
            break;
        status = vaMapBuffer(p->display, buffer, (void**)&param);
        if (!is_success(status, "vaMapBuffer()"))
            break;
        param->surface = in->id;
        param->surface_region = NULL;
        param->output_region = NULL;
        param->output_background_color = 0;
        param->filter_flags = flags;
        param->filters = p->pipe.filters;
        param->num_filters = p->pipe.num_filters;
        vaUnmapBuffer(p->display, buffer);
        param->forward_references = p->pipe.forward.surfaces;
        param->backward_references = p->pipe.backward.surfaces;
        param->num_forward_references = p->pipe.forward.num_required;
        param->num_backward_references = p->pipe.backward.num_required;
        status = vaRenderPicture(p->display, p->context, &buffer, 1);
        if (!is_success(status, "vaRenderPicture()"))
            break;
        state |= Rendered;
    } while (false);
    if (state & Begun)
        vaEndPicture(p->display, p->context);
    if (state & Rendered)
        return va_surface_wrap(out);
    va_surface_release(out);
    return NULL;
}

// return value: the number of created images
static int process(struct vf_priv_s *p, struct mp_image *in, struct mp_image **out1, struct mp_image **out2)
{
    const bool deint = p->do_deint && p->deint_type > 0;
    if (!update_pipeline(p, deint) || !p->pipe.filters) // no filtering
        return 0;
    struct va_surface *surface = va_surface_in_mp_image(in);
    const uint csp = get_va_colorspace_flag(p->params.colorspace);
    const uint field = get_deint_field(p, 0, in);
    *out1 = render(p, surface, field | csp);
    if (!*out1) // cannot render
        return 0;
    mp_image_copy_attributes(*out1, in);
    if (field == VA_FRAME_PICTURE || p->deint_type < 2) // first-field only
        return 1;
    const double add = (in->pts - p->prev_pts)*0.5;
    if (p->prev_pts == MP_NOPTS_VALUE || add <= 0.0 || add > 0.5) // no pts, skip it
        return 1;
    *out2 = render(p, surface, get_deint_field(p, 1, in) | csp);
    if (!*out2) // cannot render
        return 1;
    mp_image_copy_attributes(*out2, in);
    (*out2)->pts = in->pts + add;
    return 2;
}

static struct mp_image *upload(struct vf_priv_s *p, struct mp_image *in)
{
    struct va_surface *surface = va_surface_pool_get_by_imgfmt(p->pool, p->va->image_formats, in->imgfmt, in->w, in->h);
    if (!surface)
        surface = va_surface_pool_get(p->pool, in->w, in->h); // dummy
    else
        va_surface_upload(surface, in);
    struct mp_image *out = va_surface_wrap(surface);
    mp_image_copy_attributes(out, in);
    return out;
}

static int filter_ext(struct vf_instance *vf, struct mp_image *in)
{
    struct vf_priv_s *p = vf->priv;
    struct va_surface *surface = va_surface_in_mp_image(in);
    const int rt_format = surface ? surface->rt_format : VA_RT_FORMAT_YUV420;
    if (!p->pool || va_surface_pool_rt_format(p->pool) != rt_format) {
        va_surface_pool_release(p->pool);
        p->pool = va_surface_pool_alloc(p->display, rt_format);
    }
    if (!surface) {
        struct mp_image *tmp = upload(p, in);
        talloc_free(in);
        in = tmp;
    }

    struct mp_image *out1, *out2;
    const double pts = in->pts;
    const int num = process(p, in, &out1, &out2);
    if (!num)
        vf_add_output_frame(vf, in);
    else {
        vf_add_output_frame(vf, out1);
        if (num > 1)
            vf_add_output_frame(vf, out2);
        talloc_free(in);
    }
    p->prev_pts = pts;
    return 0;
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *params, int flags)
{
    struct vf_priv_s *p = vf->priv;

    p->prev_pts = MP_NOPTS_VALUE;
    p->params = *params;
    params->imgfmt = IMGFMT_VAAPI;
    return vf_next_reconfig(vf, params, flags);
}

static void uninit(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    for (int i=0; i<p->num_buffers; ++i)
        vaDestroyBuffer(p->display, p->buffers[i]);
    if (p->context != VA_INVALID_ID)
        vaDestroyContext(p->display, p->context);
    if (p->config != VA_INVALID_ID)
        vaDestroyConfig(p->display, p->config);
    free(p->pipe.forward.surfaces);
    free(p->pipe.backward.surfaces);
    va_surface_pool_release(p->pool);
}

static int query_format(struct vf_instance *vf, unsigned int imgfmt)
{
    struct vf_priv_s *p = vf->priv;
    if (IMGFMT_IS_VAAPI(imgfmt) || va_image_format_from_imgfmt(p->va->image_formats, imgfmt))
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
    default:
        return vf_next_control (vf, request, data);
    }
}

static int va_query_filter_caps(struct vf_priv_s *p, VAProcFilterType type, void *caps, uint count)
{
    VAStatus status = vaQueryVideoProcFilterCaps(p->display, p->context, type, caps, &count);
    return is_success(status, "vaQueryVideoProcFilterCaps()") ? count : 0;
}

static VABufferID va_create_filter_buffer(struct vf_priv_s *p, int bytes, int num, void *data)
{
    VABufferID buffer;
    VAStatus status = vaCreateBuffer(p->display, p->context, VAProcFilterParameterBufferType, bytes, num, data, &buffer);
    return is_success(status, "vaCreateBuffer()") ? buffer : VA_INVALID_ID;
}

static bool initialize(struct vf_priv_s *p)
{
    VAStatus status;

    VAConfigID config;
    status = vaCreateConfig(p->display, VAProfileNone, VAEntrypointVideoProc, NULL, 0, &config);
    if (!is_success(status, "vaCreateConfig()")) // no entrypoint for video porc
        return false;
    p->config = config;

    VAContextID context;
    status = vaCreateContext(p->display, p->config, 0, 0, 0, NULL, 0, &context);
    if (!is_success(status, "vaCreateContext()"))
        return false;
    p->context = context;

    VAProcFilterType filters[VAProcFilterCount];
    int num_filters = VAProcFilterCount;
    status = vaQueryVideoProcFilters(p->display, p->context, filters, &num_filters);
    if (!is_success(status, "vaQueryVideoProcFilters()"))
        return false;

    VABufferID buffers[VAProcFilterCount];
    for (int i=0; i<VAProcFilterCount; ++i)
        buffers[i] = VA_INVALID_ID;
    for (int i=0; i<num_filters; ++i) {
        if (filters[i] == VAProcFilterDeinterlacing) {
            if (!p->deint_type)
                continue;
            VAProcFilterCapDeinterlacing caps[VAProcDeinterlacingCount];
            int num = va_query_filter_caps(p, VAProcFilterDeinterlacing, caps, VAProcDeinterlacingCount);
            if (!num)
                continue;
            VAProcDeinterlacingType algorithm = VAProcDeinterlacingBob;
            for (int i=0; i<num; ++i) { // find Bob
                if (caps[i].type != algorithm)
                    continue;
                VAProcFilterParameterBufferDeinterlacing param;
                param.type = VAProcFilterDeinterlacing;
                param.algorithm = algorithm;
                buffers[VAProcFilterDeinterlacing] = va_create_filter_buffer(p, sizeof(param), 1, &param);
            }
        } // check other filters
    }
    p->num_buffers = 0;
    if (buffers[VAProcFilterDeinterlacing] != VA_INVALID_ID)
        p->buffers[p->num_buffers++] = buffers[VAProcFilterDeinterlacing];
    else
        p->deint_type = 0;
    p->do_deint = !!p->deint_type;
    // next filters: p->buffers[p->num_buffers++] = buffers[next_filter];
    return true;
}

static int vf_open(vf_instance_t *vf, char *args)
{
    vf->reconfig = reconfig;
    vf->filter_ext = filter_ext;
    vf->query_format = query_format;
    vf->uninit = uninit;
    vf->control = control;

    struct vf_priv_s *p = vf->priv;
    struct mp_hwdec_info hwdec;
    if (vf_control(vf->next, VFCTRL_GET_HWDEC_INFO, &hwdec) <= 0)
        return false;
    p->va = hwdec.vaapi_ctx;
    if (!p->va || !p->va->display)
        return false;
    p->display = p->va->display;
    if (initialize(p))
        return true;
    uninit(vf);
    return false;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_CHOICE("deint", deint_type, 0,
               ({"no", 0},
                {"first-field", 1},
                {"bob", 2})),
    {0}
};

const vf_info_t vf_info_vaapi = {
    .info = "VA-API Video Post-Process Filter",
    .name = "vavpp",
    .author = "xylosper",
    .comment = "",
    .vf_open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_default,
    .options = vf_opts_fields,
};
