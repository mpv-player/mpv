/*
 * Copyright (C) 2021 Niklas Haas
 *
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

#include <libplacebo/swapchain.h>

#include "common/common.h"
#include "options/m_config.h"
#include "sub/osd.h"
#include "video/hwdec.h"
#include "gpu/context.h"
#include "gpu_next/context.h"
#include "gpu_next/video.h"
#include "vo.h"

struct priv {
    struct gpu_ctx *context;
    struct pl_video *renderer;
    bool frame_pending;
    struct mp_image_params target_params;
};

static void resize(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct mp_rect src, dst;
    struct mp_osd_res osd;
    vo_get_src_dst_rects(vo, &src, &dst, &osd);
    if (vo->dwidth && vo->dheight) {
        gpu_ctx_resize(p->context, vo->dwidth, vo->dheight);
        vo->want_redraw = true;
    }
    pl_video_resize(p->renderer, src, dst, osd);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;
    struct ra_ctx *ctx = p->context->ra_ctx;
    if (!ctx->fns->reconfig(ctx))
        return -1;
    if (params)
        pl_video_config(p->renderer, params);
    resize(vo);
    mp_mutex_lock(&vo->params_mutex);
    vo->target_params = NULL;
    mp_mutex_unlock(&vo->params_mutex);
    return 0;
}

static bool set_colorspace_hint(struct vo *vo, struct pl_color_space *hint)
{
    struct priv *p = vo->priv;
    struct ra_ctx *ctx = p->context->ra_ctx;
    struct ra_swapchain *sw = ctx->swapchain;
    enum pl_color_levels levels = pl_video_get_output_levels(p->renderer);
    struct mp_image_params params = {
        .color = hint ? *hint : pl_color_space_srgb,
        .repr = {
            .sys = PL_COLOR_SYSTEM_RGB,
            .levels = levels ? levels : PL_COLOR_LEVELS_FULL,
            .alpha = ctx->opts.want_alpha ? PL_ALPHA_INDEPENDENT : PL_ALPHA_NONE,
        },
    };
    if (sw->fns->set_color && sw->fns->set_color(sw, hint ? &params : NULL)) {
        if (hint) {
            *hint = params.color;
            return true;
        }
    }
    pl_swapchain_colorspace_hint(p->context->swapchain, hint);
    return false;
}

static bool draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;
    struct ra_swapchain *sw = p->context->ra_ctx->swapchain;
    struct pl_video_target target = {0};
    if (sw->fns->target_csp)
        target.color = sw->fns->target_csp(sw);
    if (sw->fns->target_ref_luma)
        target.reference_luma = sw->fns->target_ref_luma(sw);
    pl_video_get_target_hint(p->renderer, frame, &target);
    if (target.hint_enabled && frame->current)
        target.color_overridden = set_colorspace_hint(vo, &target.hint);
    else if (!target.hint_enabled)
        target.color_overridden = set_colorspace_hint(vo, NULL);

    struct pl_swapchain_frame swframe;
    bool should_draw = sw->fns->start_frame(sw, NULL);
    bool acquired = should_draw &&
                    pl_swapchain_start_frame(p->context->swapchain, &swframe);
    bool rendered = pl_video_render_frame(p->renderer, frame,
                                          acquired ? &swframe : NULL, false, &target);
    if (!acquired)
        return VO_FALSE;

    if (rendered) {
        mp_mutex_lock(&vo->params_mutex);
        p->target_params = *pl_video_get_target_params(p->renderer);
        vo->target_params = &p->target_params;
        if (vo->params)
            vo->has_peak_detect_values = pl_video_get_hdr_metadata(p->renderer, vo->params);
        mp_mutex_unlock(&vo->params_mutex);
    }
    p->frame_pending = true;
    return VO_TRUE;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct ra_swapchain *sw = p->context->ra_ctx->swapchain;
    if (p->frame_pending) {
        if (!pl_swapchain_submit_frame(p->context->swapchain))
            MP_ERR(vo, "Failed presenting frame!\n");
        p->frame_pending = false;
    }
    sw->fns->swap_buffers(sw);
}

static void get_vsync(struct vo *vo, struct vo_vsync_info *info)
{
    struct priv *p = vo->priv;
    struct ra_swapchain *sw = p->context->ra_ctx->swapchain;
    if (sw->fns->get_vsync)
        sw->fns->get_vsync(sw, info);
}

static int query_format(struct vo *vo, int format)
{
    struct priv *p = vo->priv;
    return pl_video_check_format(p->renderer, format);
}

static struct mp_image *get_image(struct vo *vo, int imgfmt, int w, int h,
                                  int stride_align, int flags)
{
    struct priv *p = vo->priv;
    return pl_video_get_image(p->renderer, imgfmt, w, h, stride_align, flags);
}

static bool update_auto_profile(struct vo *vo, int *events)
{
    struct priv *p = vo->priv;
    if (!pl_video_icc_auto_enabled(p->renderer))
        return false;
    MP_VERBOSE(vo, "Querying ICC profile...\n");
    bstr icc = {0};
    struct ra_ctx *ctx = p->context->ra_ctx;
    int r = ctx->fns->control(ctx, events, VOCTRL_GET_ICC_PROFILE, &icc);
    if (r == VO_NOTAVAIL)
        return false;
    if (r == VO_FALSE)
        MP_WARN(vo, "Could not retrieve an ICC profile.\n");
    else if (r == VO_NOTIMPL)
        MP_ERR(vo, "icc-profile-auto not implemented on this platform.\n");
    pl_video_set_icc_profile(p->renderer, icc);
    return true;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    struct ra_ctx *ctx = p->context->ra_ctx;
    switch (request) {
    case VOCTRL_SET_PANSCAN:
        resize(vo);
        return VO_TRUE;
    case VOCTRL_PAUSE:
        if (pl_video_set_paused(p->renderer, true))
            vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_RESUME:
        pl_video_set_paused(p->renderer, false);
        return VO_TRUE;
    case VOCTRL_UPDATE_RENDER_OPTS: {
        ctx->opts.want_alpha = pl_video_wants_alpha(vo->global);
        if (ctx->fns->update_render_opts)
            ctx->fns->update_render_opts(ctx);
        vo->want_redraw = true;
        pl_video_update_options(p->renderer);
        pl_video_configure_queue(p->renderer, vo);
        int events = 0;
        update_auto_profile(vo, &events);
        vo_event(vo, events);
        return VO_TRUE;
    }
    case VOCTRL_RESET:
        pl_video_reset(p->renderer);
        return VO_TRUE;
    case VOCTRL_PERFORMANCE_DATA:
        pl_video_get_perfdata(p->renderer, data);
        return true;
    case VOCTRL_SCREENSHOT:
        pl_video_screenshot(p->renderer, data);
        return true;
    case VOCTRL_EXTERNAL_RESIZE:
        reconfig(vo, NULL);
        return true;
    case VOCTRL_LOAD_HWDEC_API:
        pl_video_load_hwdec_api(p->renderer, data);
        return true;
    }

    int events = 0;
    int r = ctx->fns->control(ctx, &events, request, data);
    if (events & VO_EVENT_ICC_PROFILE_CHANGED) {
        if (update_auto_profile(vo, &events))
            vo->want_redraw = true;
    }
    if (events & VO_EVENT_RESIZE)
        resize(vo);
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    vo_event(vo, events);
    return r;
}

static void wakeup(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct ra_ctx *ctx = p->context ? p->context->ra_ctx : NULL;
    if (ctx && ctx->fns->wakeup)
        ctx->fns->wakeup(ctx);
}

static void wait_events(struct vo *vo, int64_t until_time_ns)
{
    struct priv *p = vo->priv;
    struct ra_ctx *ctx = p->context ? p->context->ra_ctx : NULL;
    if (ctx && ctx->fns->wait_events)
        ctx->fns->wait_events(ctx, until_time_ns);
    else
        vo_wait_default(vo, until_time_ns);
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    pl_video_uninit(p->renderer);
    p->renderer = NULL;
    vo->hwdec_devs = NULL;
    gpu_ctx_destroy(&p->context);
}

static void load_hwdec_api(void *ctx, struct hwdec_imgfmt_request *params)
{
    vo_control(ctx, VOCTRL_LOAD_HWDEC_API, params);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct ra_ctx_opts *opts = mp_get_config_group(vo, vo->global, &ra_ctx_conf);
    opts->want_alpha = pl_video_wants_alpha(vo->global);
    p->context = gpu_ctx_create(vo, opts);
    talloc_free(opts);
    if (!p->context)
        return -1;
    p->renderer = pl_video_init(p->context->ra_ctx, p->context->pllog,
                                p->context->gpu, vo->global, vo->log);
    if (!p->renderer) {
        uninit(vo);
        return -1;
    }
    pl_video_set_osd_source(p->renderer, vo->osd);
    pl_video_configure_queue(p->renderer, vo);
    vo->hwdec_devs = pl_video_get_hwdec_devices(p->renderer);
    hwdec_devices_set_loader(vo->hwdec_devs, load_hwdec_api, vo);
    return 0;
}

const struct vo_driver video_out_gpu_next = {
    .description = "Video output based on libplacebo",
    .name = "gpu-next",
    .caps = VO_CAP_ROTATE90 |
            VO_CAP_FILM_GRAIN |
            VO_CAP_VFLIP |
            0x0,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .get_image_ts = get_image,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .get_vsync = get_vsync,
    .wait_events = wait_events,
    .wakeup = wakeup,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
