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

#include "context.h"
#include "video.h"
#include "misc/bstr.h"
#include "video/hwdec.h"
#include "video/mp_image.h"
#include "video/out/gpu/context.h"
#include "video/out/gpu/libmpv_gpu.h"
#include "video/out/libmpv.h"

struct priv {
    struct libmpv_gpu_context *context;
    struct gpu_ctx *gpu_ctx;
    struct mppl_video *renderer;

    // From MPV_RENDER_PARAM_ICC_PROFILE
    bstr icc_profile;
};

static int init(struct render_backend *ctx, mpv_render_param *params)
{
    ctx->priv = talloc_zero(NULL, struct priv);
    struct priv *p = ctx->priv;

    int err = libmpv_gpu_context_create(ctx->global, ctx->log, params, &p->context);
    if (err < 0)
        return err;

    bool probing = !get_mpv_render_param(params, MPV_RENDER_PARAM_RENDERER, NULL);
    p->gpu_ctx = gpu_ctx_create(ctx->log, p->context->ra_ctx, probing);
    if (!p->gpu_ctx)
        return probing ? MPV_ERROR_NOT_IMPLEMENTED : MPV_ERROR_UNSUPPORTED;

    p->renderer = mppl_video_init(ctx->log, ctx->global, p->gpu_ctx);

    ctx->hwdec_devs = hwdec_devices_create();
    mppl_video_init_hwdecs(p->renderer, ctx->hwdec_devs, true);
    ctx->driver_caps = VO_CAP_ROTATE90 | VO_CAP_VFLIP;
    return 0;
}

static bool check_format(struct render_backend *ctx, int imgfmt)
{
    struct priv *p = ctx->priv;

    return mppl_video_check_format(p->renderer, imgfmt);
}

// The profile is used with icc-profile-auto only, and the renderer unloads it
// whenever its options change.
static void update_icc_profile(struct priv *p)
{
    if (mppl_video_icc_auto_enabled(p->renderer))
        mppl_video_set_icc_profile(p->renderer, bstrdup(NULL, p->icc_profile));
}

static int set_parameter(struct render_backend *ctx, mpv_render_param param)
{
    struct priv *p = ctx->priv;

    switch (param.type) {
    case MPV_RENDER_PARAM_ICC_PROFILE: {
        mpv_byte_array *data = param.data;
        talloc_free(p->icc_profile.start);
        p->icc_profile = bstrdup(p, (bstr){data->data, data->size});
        update_icc_profile(p);
        return 0;
    }
    default:
        return MPV_ERROR_NOT_IMPLEMENTED;
    }
}

static void reconfig(struct render_backend *ctx, struct mp_image_params *params)
{
    struct priv *p = ctx->priv;

    // There are no params when the VO is gone, and the video with it.
    if (!params->imgfmt)
        mppl_video_flush(p->renderer);
    mppl_video_config(p->renderer, params);
}

static void reset(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;

    mppl_video_reset(p->renderer);
}

static void set_paused(struct render_backend *ctx, bool paused)
{
    struct priv *p = ctx->priv;

    mppl_video_set_paused(p->renderer, paused);
}

static void update_external(struct render_backend *ctx, struct vo *vo)
{
    struct priv *p = ctx->priv;

    mppl_video_set_osd_source(p->renderer, vo ? vo->osd : NULL);
    mppl_video_reload_options(p->renderer);
    update_icc_profile(p);
    if (vo)
        mppl_video_configure_queue(p->renderer, vo);
}

static void resize(struct render_backend *ctx, struct mp_rect *src,
                   struct mp_rect *dst, struct mp_osd_res *osd)
{
    struct priv *p = ctx->priv;

    mppl_video_resize(p->renderer, *src, *dst, *osd);
}

static int get_target_size(struct render_backend *ctx, mpv_render_param *params,
                           int *out_w, int *out_h)
{
    struct priv *p = ctx->priv;

    struct ra_tex *tex;
    int err = p->context->fns->wrap_fbo(p->context, params, &tex);
    if (err < 0)
        return err;
    *out_w = tex->params.w;
    *out_h = tex->params.h;
    return 0;
}

static int render(struct render_backend *ctx, mpv_render_param *params,
                  struct vo_frame *frame)
{
    struct priv *p = ctx->priv;
    struct ra *ra = p->context->ra_ctx->ra;
    pl_gpu gpu = p->gpu_ctx->gpu;

    struct ra_tex *tex;
    int err = p->context->fns->wrap_fbo(p->context, params, &tex);
    if (err < 0)
        return err;

    pl_tex fbo = gpu_ctx_wrap_ra_tex(gpu, ra, tex);
    if (!fbo) {
        p->context->fns->done_frame(p->context, frame->display_synced);
        return MPV_ERROR_UNSUPPORTED;
    }

    int depth = GET_MPV_RENDER_PARAM(params, MPV_RENDER_PARAM_DEPTH, int, 0);
    int fbo_depth = fbo->params.format->component_depth[0];
    struct pl_swapchain_frame target = {
        .fbo = fbo,
        .flipped = GET_MPV_RENDER_PARAM(params, MPV_RENDER_PARAM_FLIP_Y, int, 0),
        .color_repr = {
            .sys = PL_COLOR_SYSTEM_RGB,
            .levels = PL_COLOR_LEVELS_FULL,
            .bits = {
                .sample_depth = fbo_depth,
                .color_depth = fbo_depth,
            },
        },
        .color_space = pl_color_space_monitor,
    };

    // There is no display to take a color space from and none to hint one to.
    mppl_video_begin_frame(p->renderer, frame, (struct pl_color_space){0}, 0, false);
    struct mp_image_params target_params;
    mppl_video_render_frame(p->renderer, frame, &target, depth, NULL, &target_params);

    gpu_ctx_unwrap_ra_tex(gpu, ra, &fbo);
    p->context->fns->done_frame(p->context, frame->display_synced);

    return 0;
}

static struct mp_image *get_image(struct render_backend *ctx, int imgfmt,
                                  int w, int h, int stride_align, int flags)
{
    struct priv *p = ctx->priv;

    return mppl_video_get_image(p->renderer, imgfmt, w, h, stride_align, flags);
}

static void screenshot(struct render_backend *ctx, struct vo_frame *frame,
                       struct voctrl_screenshot *args)
{
    struct priv *p = ctx->priv;

    mppl_video_screenshot(p->renderer, args);
}

static void perfdata(struct render_backend *ctx,
                     struct voctrl_performance_data *out)
{
    struct priv *p = ctx->priv;

    mppl_video_perfdata(p->renderer, out);
}

static void destroy(struct render_backend *ctx)
{
    struct priv *p = ctx->priv;

    if (p->renderer)
        mppl_video_uninit(p->renderer);

    hwdec_devices_destroy(ctx->hwdec_devs);

    gpu_ctx_destroy(&p->gpu_ctx);
    libmpv_gpu_context_destroy(&p->context);
}

const struct render_backend_fns render_backend_gpu_next = {
    .renderer = MPV_RENDER_RENDERER_GPU_NEXT,
    .init = init,
    .check_format = check_format,
    .set_parameter = set_parameter,
    .reconfig = reconfig,
    .reset = reset,
    .set_paused = set_paused,
    .update_external = update_external,
    .resize = resize,
    .get_target_size = get_target_size,
    .render = render,
    .get_image = get_image,
    .screenshot = screenshot,
    .perfdata = perfdata,
    .destroy = destroy,
};
