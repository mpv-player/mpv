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

#include <va/va.h>
#include <va/va_vpp.h>

#include "config.h"
#include "options/options.h"
#include "vf.h"
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
    struct mp_image_pool *pool;
    int current_rt_format;
};

static const struct vf_priv_s vf_priv_default = {
    .prev_pts = MP_NOPTS_VALUE,
    .config = VA_INVALID_ID,
    .context = VA_INVALID_ID,
    .deint_type = 2,
};

// The array items must match with the "deint" suboption values.
static const int deint_algorithm[] = {
    [0] = VAProcDeinterlacingNone,
    [1] = VAProcDeinterlacingNone, // first-field, special-cased
    [2] = VAProcDeinterlacingBob,
    [3] = VAProcDeinterlacingWeave,
    [4] = VAProcDeinterlacingMotionAdaptive,
    [5] = VAProcDeinterlacingMotionCompensated,
};

static inline void realloc_refs(struct surface_refs *refs, int num)
{
    if (refs->num_allocated < num) {
        refs->surfaces = realloc(refs->surfaces, sizeof(VASurfaceID)*num);
        refs->num_allocated = num;
    }
    refs->num_required = num;
}

static bool update_pipeline(struct vf_instance *vf, bool deint)
{
    struct vf_priv_s *p = vf->priv;
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
    VAStatus status = vaQueryVideoProcPipelineCaps(p->display, p->context,
                                                   filters, num_filters, &caps);
    if (!check_error(vf, status, "vaQueryVideoProcPipelineCaps()"))
        return false;
    p->pipe.filters = filters;
    p->pipe.num_filters = num_filters;
    p->pipe.num_input_colors = caps.num_input_color_standards;
    p->pipe.num_output_colors = caps.num_output_color_standards;
    realloc_refs(&p->pipe.forward, caps.num_forward_references);
    realloc_refs(&p->pipe.backward, caps.num_backward_references);
    return true;
}

static inline int get_deint_field(struct vf_priv_s *p, int i,
                                  const struct mp_image *mpi)
{
    if (!p->do_deint || !(mpi->fields & MP_IMGFIELD_INTERLACED))
        return VA_FRAME_PICTURE;
    return !!(mpi->fields & MP_IMGFIELD_TOP_FIRST) ^ i ? VA_TOP_FIELD : VA_BOTTOM_FIELD;
}

static struct mp_image *render(struct vf_instance *vf, struct mp_image *in,
                               unsigned int flags)
{
    struct vf_priv_s *p = vf->priv;
    VASurfaceID in_id = va_surface_id(in);
    if (!p->pipe.filters || in_id == VA_INVALID_ID)
        return NULL;
    struct mp_image *img = mp_image_pool_get(p->pool, IMGFMT_VAAPI, in->w, in->h);
    if (!img)
        return NULL;
    enum {Begun = 1, Rendered = 2};
    int state = 0;
    do { // not a loop, just for break
        VASurfaceID id = va_surface_id(img);
        if (id == VA_INVALID_ID)
            break;
        VAStatus status = vaBeginPicture(p->display, p->context, id);
        if (!check_error(vf, status, "vaBeginPicture()"))
            break;
        state |= Begun;
        VABufferID buffer = VA_INVALID_ID;
        VAProcPipelineParameterBuffer *param = NULL;
        status = vaCreateBuffer(p->display, p->context,
                                VAProcPipelineParameterBufferType,
                                sizeof(*param), 1, NULL, &buffer);
        if (!check_error(vf, status, "vaCreateBuffer()"))
            break;
        status = vaMapBuffer(p->display, buffer, (void**)&param);
        if (!check_error(vf, status, "vaMapBuffer()"))
            break;

        VAProcFilterParameterBufferDeinterlacing *filter_params;
        status = vaMapBuffer(p->display, *(p->pipe.filters), (void**)&filter_params);
        if (!check_error(vf, status, "vaMapBuffer()"))
            break;
        filter_params->flags = flags & VA_TOP_FIELD ? 0 : VA_DEINTERLACING_BOTTOM_FIELD;
        vaUnmapBuffer(p->display, *(p->pipe.filters));

        param->surface = in_id;
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
        if (!check_error(vf, status, "vaRenderPicture()"))
            break;
        state |= Rendered;
    } while (false);
    if (state & Begun)
        vaEndPicture(p->display, p->context);
    if (state & Rendered)
        return img;
    talloc_free(img);
    return NULL;
}

// return value: the number of created images
static int process(struct vf_instance *vf, struct mp_image *in,
                   struct mp_image **out1, struct mp_image **out2)
{
    struct vf_priv_s *p = vf->priv;
    const bool deint = p->do_deint && p->deint_type > 0;
    if (!update_pipeline(vf, deint) || !p->pipe.filters) // no filtering
        return 0;
    const unsigned int csp = va_get_colorspace_flag(p->params.colorspace);
    const unsigned int field = get_deint_field(p, 0, in);
    *out1 = render(vf, in, field | csp);
    if (!*out1) // cannot render
        return 0;
    mp_image_copy_attributes(*out1, in);
    // first-field only
    if (field == VA_FRAME_PICTURE || (p->do_deint && p->deint_type < 2))
        return 1;
    const double add = (in->pts - p->prev_pts)*0.5;
    if (p->prev_pts == MP_NOPTS_VALUE || add <= 0.0 || add > 0.5) // no pts, skip it
        return 1;
    *out2 = render(vf, in, get_deint_field(p, 1, in) | csp);
    if (!*out2) // cannot render
        return 1;
    mp_image_copy_attributes(*out2, in);
    (*out2)->pts = in->pts + add;
    return 2;
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
    if (!in)
        return 0;
    int rt_format = in->imgfmt == IMGFMT_VAAPI ? va_surface_rt_format(in)
                                               : VA_RT_FORMAT_YUV420;
    if (!p->pool || p->current_rt_format != rt_format) {
        talloc_free(p->pool);
        p->pool = mp_image_pool_new(20);
        va_pool_set_allocator(p->pool, p->va, rt_format);
        p->current_rt_format = rt_format;
    }
    if (in->imgfmt != IMGFMT_VAAPI) {
        struct mp_image *tmp = upload(vf, in);
        talloc_free(in);
        in = tmp;
        if (!in)
            return -1;
    }

    struct mp_image *out1, *out2;
    const double pts = in->pts;
    const int num = process(vf, in, &out1, &out2);
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

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    struct vf_priv_s *p = vf->priv;

    p->prev_pts = MP_NOPTS_VALUE;
    p->params = *in;
    *out = *in;
    out->imgfmt = IMGFMT_VAAPI;
    return 0;
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
    talloc_free(p->pool);
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
    for (int i=0; i<VAProcFilterCount; ++i)
        buffers[i] = VA_INVALID_ID;
    for (int i=0; i<num_filters; ++i) {
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
    vf->reconfig = reconfig;
    vf->filter_ext = filter_ext;
    vf->query_format = query_format;
    vf->uninit = uninit;
    vf->control = control;

    struct vf_priv_s *p = vf->priv;
    if (!vf->hwdec)
        return false;
    hwdec_request_api(vf->hwdec, "vaapi");
    p->va = vf->hwdec->hwctx ? vf->hwdec->hwctx->vaapi_ctx : NULL;
    if (!p->va || !p->va->display)
        return false;
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
