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

#include <libplacebo/colorspace.h>
#include <libplacebo/swapchain.h>

#include "common/common.h"
#include "common/msg.h"
#include "misc/bstr.h"
#include "options/m_config.h"
#include "sub/osd.h"
#include "video/hwdec.h"
#include "video/mp_image.h"
#include "vo.h"
#include "gpu/context.h"
#include "gpu_next/context.h"
#include "gpu_next/video.h"

struct priv {
    struct mp_log *log;
    struct ra_ctx *ra_ctx;
    struct gpu_ctx *context;
    pl_swapchain sw;
    struct mppl_video *renderer;
    bool frame_pending;
    struct mp_image_params target_params;
};

static bool set_colorspace_hint(struct priv *p, struct pl_color_space *hint)
{
    struct ra_swapchain *sw = p->ra_ctx->swapchain;

    struct mp_image_params params = {
        .color = hint ? *hint : pl_color_space_srgb,
        .repr = {
            .sys = PL_COLOR_SYSTEM_RGB,
            .levels = mppl_video_get_output_levels(p->renderer),
            .alpha = p->ra_ctx->opts.want_alpha ? PL_ALPHA_INDEPENDENT : PL_ALPHA_NONE,
        },
    };

    if (sw->fns->set_color && sw->fns->set_color(sw, hint ? &params : NULL)) {
        if (hint) {
            *hint = params.color;
            return true;
        }
    }
    pl_swapchain_colorspace_hint(p->sw, hint);
    return false;
}

static bool draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;
    struct ra_swapchain *sw = p->ra_ctx->swapchain;

    struct pl_color_space display_csp = {0};
    // TODO: Implement this for all backends
    if (sw->fns->target_csp)
        display_csp = sw->fns->target_csp(sw);
    float display_ref_luma = sw->fns->target_ref_luma ? sw->fns->target_ref_luma(sw) : 0;
    mppl_video_begin_frame(p->renderer, frame, display_csp, display_ref_luma);

    struct pl_color_space hint = {0};
    bool external_params = false;
    enum mppl_video_hint hint_mode = mppl_video_get_target_hint(p->renderer, frame, &hint);
    if (hint_mode == MPPL_VIDEO_HINT_SET) {
        external_params = set_colorspace_hint(p, &hint);
    } else if (hint_mode == MPPL_VIDEO_HINT_NONE) {
        external_params = set_colorspace_hint(p, NULL);
    }

    struct pl_swapchain_frame swframe;
    bool should_draw = sw->fns->start_frame(sw, NULL); // for wayland logic
    if (!should_draw || !pl_swapchain_start_frame(p->sw, &swframe)) {
        mppl_video_skip_frame(p->renderer, frame);
        return VO_FALSE;
    }

    if (external_params)
        swframe.color_space = hint;
    int fb_depth = sw->fns->color_depth ? sw->fns->color_depth(sw) : 0;

    struct mp_image_params target_params;
    bool ok = mppl_video_render_frame(p->renderer, frame, &swframe, fb_depth,
                                      hint_mode == MPPL_VIDEO_HINT_NONE ? NULL : &hint,
                                      &target_params);
    p->frame_pending = true;
    if (!ok)
        return VO_TRUE;

    mp_mutex_lock(&vo->params_mutex);
    p->target_params = target_params;
    vo->target_params = &p->target_params;

    if (vo->params) {
        // Augment metadata with the peak detection results
        vo->has_peak_detect_values = mppl_video_get_hdr_metadata(p->renderer,
                                                                 &vo->params->color.hdr);
    }
    mp_mutex_unlock(&vo->params_mutex);

    return VO_TRUE;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct ra_swapchain *sw = p->ra_ctx->swapchain;

    if (p->frame_pending) {
        if (!pl_swapchain_submit_frame(p->sw))
            MP_ERR(vo, "Failed presenting frame!\n");
        p->frame_pending = false;
    }

    sw->fns->swap_buffers(sw);
}

static void get_vsync(struct vo *vo, struct vo_vsync_info *info)
{
    struct priv *p = vo->priv;
    struct ra_swapchain *sw = p->ra_ctx->swapchain;
    if (sw->fns->get_vsync)
        sw->fns->get_vsync(sw, info);
}

static int query_format(struct vo *vo, int format)
{
    struct priv *p = vo->priv;
    return mppl_video_check_format(p->renderer, format);
}

static struct mp_image *get_image(struct vo *vo, int imgfmt, int w, int h,
                                  int stride_align, int flags)
{
    struct priv *p = vo->priv;
    return mppl_video_get_image(p->renderer, imgfmt, w, h, stride_align, flags);
}

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

    mppl_video_resize(p->renderer, src, dst, osd);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;
    if (!p->ra_ctx->fns->reconfig(p->ra_ctx))
        return -1;

    if (params)
        mppl_video_config(p->renderer, params);
    resize(vo);
    mp_mutex_lock(&vo->params_mutex);
    vo->target_params = NULL;
    mp_mutex_unlock(&vo->params_mutex);
    return 0;
}

// Returns whether the ICC profile was updated (even on failure)
static bool update_auto_profile(struct priv *p, int *events)
{
    if (!mppl_video_icc_auto_enabled(p->renderer))
        return false;

    MP_VERBOSE(p, "Querying ICC profile...\n");
    bstr icc = {0};
    int r = p->ra_ctx->fns->control(p->ra_ctx, events, VOCTRL_GET_ICC_PROFILE, &icc);

    if (r != VO_NOTAVAIL) {
        if (r == VO_FALSE) {
            MP_WARN(p, "Could not retrieve an ICC profile.\n");
        } else if (r == VO_NOTIMPL) {
            MP_ERR(p, "icc-profile-auto not implemented on this platform.\n");
        }

        mppl_video_set_icc_profile(p->renderer, icc);
        return true;
    }

    return false;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;

    switch (request) {
    case VOCTRL_SET_PANSCAN:
        resize(vo);
        return VO_TRUE;
    case VOCTRL_PAUSE:
        if (mppl_video_showing_interpolated_frame(p->renderer))
            vo->want_redraw = true;
        mppl_video_set_paused(p->renderer, true);
        return VO_TRUE;
    case VOCTRL_RESUME:
        mppl_video_set_paused(p->renderer, false);
        return VO_TRUE;

    case VOCTRL_UPDATE_RENDER_OPTS: {
        mppl_video_reload_options(p->renderer);
        mppl_video_configure_queue(p->renderer, vo);

        p->ra_ctx->opts.want_alpha = mppl_video_want_alpha(p->renderer);
        if (p->ra_ctx->fns->update_render_opts)
            p->ra_ctx->fns->update_render_opts(p->ra_ctx);
        vo->want_redraw = true;

        // Also re-query the auto profile, in case `update_render_options`
        // unloaded a manually specified icc profile in favor of
        // icc-profile-auto
        int events = 0;
        update_auto_profile(p, &events);
        vo_event(vo, events);
        return VO_TRUE;
    }

    case VOCTRL_RESET:
        mppl_video_reset(p->renderer);
        return VO_TRUE;

    case VOCTRL_PERFORMANCE_DATA:
        mppl_video_perfdata(p->renderer, data);
        return true;

    case VOCTRL_SCREENSHOT:
        mppl_video_screenshot(p->renderer, data);
        return true;

    case VOCTRL_EXTERNAL_RESIZE:
        reconfig(vo, NULL);
        return true;

    case VOCTRL_LOAD_HWDEC_API:
        mppl_video_load_hwdecs(p->renderer, vo->hwdec_devs, data);
        return true;
    }

    int events = 0;
    int r = p->ra_ctx->fns->control(p->ra_ctx, &events, request, data);
    if (events & VO_EVENT_ICC_PROFILE_CHANGED) {
        if (update_auto_profile(p, &events))
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
    if (p->ra_ctx && p->ra_ctx->fns->wakeup)
        p->ra_ctx->fns->wakeup(p->ra_ctx);
}

static void wait_events(struct vo *vo, int64_t until_time_ns)
{
    struct priv *p = vo->priv;
    if (p->ra_ctx && p->ra_ctx->fns->wait_events) {
        p->ra_ctx->fns->wait_events(p->ra_ctx, until_time_ns);
    } else {
        vo_wait_default(vo, until_time_ns);
    }
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    mppl_video_uninit(p->renderer);
    hwdec_devices_set_loader(vo->hwdec_devs, NULL, NULL);
    hwdec_devices_destroy(vo->hwdec_devs);

    p->ra_ctx = NULL;
    p->sw = NULL;
    gpu_ctx_destroy(&p->context);
}

static void load_hwdec_api(void *ctx, struct hwdec_imgfmt_request *params)
{
    vo_control(ctx, VOCTRL_LOAD_HWDEC_API, params);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct ra_ctx_opts *ctx_opts = mp_get_config_group(vo, vo->global, &ra_ctx_conf);
    ctx_opts->want_alpha = mppl_video_opts_want_alpha(vo->global);
    p->context = gpu_ctx_create(vo, ctx_opts);
    talloc_free(ctx_opts);
    if (!p->context)
        return -1;
    p->log = vo->log;
    p->ra_ctx = p->context->ra_ctx;
    p->sw = p->context->swapchain;

    p->renderer = mppl_video_init(vo->log, vo->global, p->context);
    mppl_video_set_osd_source(p->renderer, vo->osd);
    mppl_video_configure_queue(p->renderer, vo);

    vo->hwdec_devs = hwdec_devices_create();
    hwdec_devices_set_loader(vo->hwdec_devs, load_hwdec_api, vo);
    mppl_video_init_hwdecs(p->renderer, vo->hwdec_devs, false);
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
