/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 */

#include <libplacebo/config.h>
#include "config.h"
#include "common/common.h"
#include "mpv/render_gl.h"
#include "video/hwdec.h"
#include "video/out/gpu/context.h"
#include "video/out/gpu/libmpv_gpu.h"
#include "video/out/libmpv.h"
#include "video/out/placebo/utils.h"
#include "video.h"

#if HAVE_GL && defined(PL_HAVE_OPENGL)
#include <libplacebo/opengl.h>
#if HAVE_EGL
#include <EGL/egl.h>
#endif
#endif

struct priv {
    struct libmpv_gpu_context *context;
    struct pl_video *renderer;
    pl_log pllog;
#if HAVE_GL && defined(PL_HAVE_OPENGL)
    pl_opengl opengl;
#endif
};

static int init(struct render_backend *ctx, mpv_render_param *params)
{
    const char *api = get_mpv_render_param(params, MPV_RENDER_PARAM_API_TYPE, NULL);
    if (!api)
        return MPV_ERROR_INVALID_PARAMETER;
    if (strcmp(api, MPV_RENDER_API_TYPE_OPENGL_NEXT))
        return MPV_ERROR_NOT_IMPLEMENTED;

#if HAVE_GL && defined(PL_HAVE_OPENGL)
    mpv_opengl_init_params *init_params =
        get_mpv_render_param(params, MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, NULL);
    if (!init_params || !init_params->get_proc_address)
        return MPV_ERROR_INVALID_PARAMETER;

    struct priv *p = ctx->priv = talloc_zero(NULL, struct priv);
    p->context = talloc_zero(p, struct libmpv_gpu_context);
    *p->context = (struct libmpv_gpu_context) {
        .global = ctx->global,
        .log = ctx->log,
        .fns = &libmpv_gpu_context_gl,
    };
    int err = libmpv_gpu_context_init(p->context, params);
    if (err < 0)
        return err;

    struct ra_ctx *ra_ctx = p->context->ra_ctx;
    ra_ctx->opts.want_alpha = pl_video_wants_alpha(ctx->global);
    p->pllog = mppl_log_create(p, ctx->log);
    if (!p->pllog)
        return MPV_ERROR_UNSUPPORTED;

    struct pl_opengl_params gl_params = *pl_opengl_params(
        .get_proc_addr_ex = (void *)init_params->get_proc_address,
        .proc_ctx = init_params->get_proc_address_ctx,
        .debug = ra_ctx->opts.debug,
        .allow_software = true,
    );
#if HAVE_EGL
    gl_params.egl_display = eglGetCurrentDisplay();
    gl_params.egl_context = eglGetCurrentContext();
#endif
    p->opengl = pl_opengl_create(p->pllog, &gl_params);
    if (!p->opengl)
        return MPV_ERROR_UNSUPPORTED;

    p->renderer = pl_video_init(ra_ctx, p->pllog, p->opengl->gpu,
                                ctx->global, ctx->log);
    if (!p->renderer)
        return MPV_ERROR_UNSUPPORTED;

    // The render API cannot service decoder requests on its rendering thread.
    // Load interops now, as the existing OpenGL render backend does.
    pl_video_load_hwdec_api(p->renderer, &(struct hwdec_imgfmt_request) {
        .imgfmt = IMGFMT_NONE,
        .probing = true,
    });
    ctx->hwdec_devs = pl_video_get_hwdec_devices(p->renderer);
    ctx->driver_caps = VO_CAP_ROTATE90 | VO_CAP_FILM_GRAIN | VO_CAP_VFLIP;
    return 0;
#else
    return MPV_ERROR_NOT_IMPLEMENTED;
#endif
}

static bool check_format(struct render_backend *ctx, int imgfmt)
{
    struct priv *p = ctx->priv;
    return pl_video_check_format(p->renderer, imgfmt);
}

static int set_parameter(struct render_backend *ctx, mpv_render_param param)
{
    struct priv *p = ctx->priv;
    if (param.type == MPV_RENDER_PARAM_ICC_PROFILE) {
        mpv_byte_array *data = param.data;
        if (!data || (data->size && !data->data))
            return MPV_ERROR_INVALID_PARAMETER;
        pl_video_update_options(p->renderer);
        if (!pl_video_icc_auto_enabled(p->renderer))
            return 0;
        return pl_video_set_icc_profile(p->renderer,
                    bstrdup(NULL, (bstr){data->data, data->size}))
                ? 0 : MPV_ERROR_INVALID_PARAMETER;
    }
    return MPV_ERROR_NOT_IMPLEMENTED;
}

static void reconfig(struct render_backend *ctx, struct mp_image_params *params)
{
    struct priv *p = ctx->priv;
    pl_video_config(p->renderer, params);
}

static void reset(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;
    pl_video_reset(p->renderer);
}

static void update_external(struct render_backend *ctx, struct vo *vo)
{
    struct priv *p = ctx->priv;
    pl_video_set_osd_source(p->renderer, vo ? vo->osd : NULL);
    pl_video_update_options(p->renderer);
    p->context->ra_ctx->opts.want_alpha = pl_video_wants_alpha(ctx->global);
    if (vo)
        pl_video_configure_queue(p->renderer, vo);
}

static void resize(struct render_backend *ctx, struct mp_rect *src,
                   struct mp_rect *dst, struct mp_osd_res *osd)
{
    struct priv *p = ctx->priv;
    pl_video_resize(p->renderer, *src, *dst, *osd);
}

static int get_target_size(struct render_backend *ctx, mpv_render_param *params,
                           int *out_w, int *out_h)
{
    mpv_opengl_fbo *fbo =
        get_mpv_render_param(params, MPV_RENDER_PARAM_OPENGL_FBO, NULL);
    if (!fbo || fbo->w <= 0 || fbo->h <= 0)
        return MPV_ERROR_INVALID_PARAMETER;
#if HAVE_GL && defined(PL_HAVE_OPENGL)
    struct priv *p = ctx->priv;
    int max_size = p->opengl->gpu->limits.max_tex_2d_dim;
    if (fbo->w > max_size || fbo->h > max_size)
        return MPV_ERROR_INVALID_PARAMETER;
#endif
    *out_w = fbo->w;
    *out_h = fbo->h;
    return 0;
}

static int render(struct render_backend *ctx, mpv_render_param *params,
                  struct vo_frame *frame)
{
#if HAVE_GL && defined(PL_HAVE_OPENGL)
    struct priv *p = ctx->priv;
    int w, h;
    int err = get_target_size(ctx, params, &w, &h);
    if (err < 0)
        return err;

    struct ra_tex *ra_target;
    err = p->context->fns->wrap_fbo(p->context, params, &ra_target);
    if (err < 0)
        return err;

    mpv_opengl_fbo *fbo =
        get_mpv_render_param(params, MPV_RENDER_PARAM_OPENGL_FBO, NULL);
    pl_gpu gpu = p->opengl->gpu;
    pl_tex target = pl_opengl_wrap(gpu, pl_opengl_wrap_params(
        .framebuffer = fbo->fbo,
        .width = w,
        .height = h,
        .iformat = fbo->internal_format,
    ));
    if (!target)
        return MPV_ERROR_UNSUPPORTED;

    struct pl_swapchain_frame swframe = {
        .fbo = target,
        .color_repr = pl_color_repr_rgb,
        .color_space = pl_color_space_srgb,
    };
    swframe.color_repr.alpha = pl_video_wants_alpha(ctx->global)
                               ? PL_ALPHA_INDEPENDENT : PL_ALPHA_NONE;
    int depth = GET_MPV_RENDER_PARAM(params, MPV_RENDER_PARAM_DEPTH, int, 0);
    if (depth > 0) {
        swframe.color_repr.bits.sample_depth = depth;
        swframe.color_repr.bits.color_depth = depth;
    }
    struct pl_video_target display = {
        .color = swframe.color_space,
        .depth = depth > 0 ? depth : 8,
        .color_known = true,
    };
    bool flip = GET_MPV_RENDER_PARAM(params, MPV_RENDER_PARAM_FLIP_Y, int, 0);
    bool ok = pl_video_render_frame(p->renderer, frame, &swframe, flip, &display);
    pl_tex_destroy(gpu, &target);
    p->context->fns->done_frame(p->context, frame->display_synced);
    return ok ? 0 : MPV_ERROR_GENERIC;
#else
    return MPV_ERROR_NOT_IMPLEMENTED;
#endif
}

static struct mp_image *get_image(struct render_backend *ctx, int imgfmt,
                                  int w, int h, int stride_align, int flags)
{
    struct priv *p = ctx->priv;
    return pl_video_get_image(p->renderer, imgfmt, w, h, stride_align, flags);
}

static void screenshot(struct render_backend *ctx, struct vo_frame *frame,
                       struct voctrl_screenshot *args)
{
    struct priv *p = ctx->priv;
    pl_video_screenshot(p->renderer, args);
}

static void perfdata(struct render_backend *ctx,
                     struct voctrl_performance_data *out)
{
    struct priv *p = ctx->priv;
    pl_video_get_perfdata(p->renderer, out);
}

static void destroy(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;
    if (!p)
        return;
    if (p->renderer)
        pl_video_uninit(p->renderer);
#if HAVE_GL && defined(PL_HAVE_OPENGL)
    pl_opengl_destroy(&p->opengl);
#endif
    pl_log_destroy(&p->pllog);
    if (p->context) {
        p->context->fns->destroy(p->context);
        talloc_free(p->context->priv);
    }
}

const struct render_backend_fns render_backend_gpu_next = {
    .init = init,
    .check_format = check_format,
    .set_parameter = set_parameter,
    .reconfig = reconfig,
    .reset = reset,
    .update_external = update_external,
    .resize = resize,
    .get_target_size = get_target_size,
    .render = render,
    .get_image = get_image,
    .screenshot = screenshot,
    .perfdata = perfdata,
    .destroy = destroy,
};
