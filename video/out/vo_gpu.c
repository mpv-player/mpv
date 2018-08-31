/*
 * Based on vo_gl.c by Reimar Doeffinger.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>

#include <libavutil/common.h>

#include "config.h"

#include "mpv_talloc.h"
#include "common/common.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "common/global.h"
#include "options/m_config.h"
#include "vo.h"
#include "video/mp_image.h"
#include "sub/osd.h"

#include "gpu/context.h"
#include "gpu/hwdec.h"
#include "gpu/video.h"

struct gpu_priv {
    struct mp_log *log;
    struct ra_ctx *ctx;

    char *context_name;
    char *context_type;
    struct ra_ctx_opts opts;
    struct gl_video *renderer;

    int events;
};

static void resize(struct vo *vo)
{
    struct gpu_priv *p = vo->priv;
    struct ra_swapchain *sw = p->ctx->swapchain;

    MP_VERBOSE(vo, "Resize: %dx%d\n", vo->dwidth, vo->dheight);

    struct mp_rect src, dst;
    struct mp_osd_res osd;
    vo_get_src_dst_rects(vo, &src, &dst, &osd);

    gl_video_resize(p->renderer, &src, &dst, &osd);

    int fb_depth = sw->fns->color_depth ? sw->fns->color_depth(sw) : 0;
    if (fb_depth)
        MP_VERBOSE(p, "Reported display depth: %d\n", fb_depth);
    gl_video_set_fb_depth(p->renderer, fb_depth);

    vo->want_redraw = true;
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct gpu_priv *p = vo->priv;
    struct ra_swapchain *sw = p->ctx->swapchain;

    struct ra_fbo fbo;
    if (!sw->fns->start_frame(sw, &fbo))
        return;

    gl_video_render_frame(p->renderer, frame, fbo, RENDER_FRAME_DEF);
    if (!sw->fns->submit_frame(sw, frame)) {
        MP_ERR(vo, "Failed presenting frame!\n");
        return;
    }
}

static void flip_page(struct vo *vo)
{
    struct gpu_priv *p = vo->priv;
    struct ra_swapchain *sw = p->ctx->swapchain;
    sw->fns->swap_buffers(sw);
}

static double get_latency(struct vo *vo)
{
    struct gpu_priv *p = vo->priv;
    struct ra_swapchain *sw = p->ctx->swapchain;
    return sw->fns->get_latency ? sw->fns->get_latency(sw) : -1;
}

static int query_format(struct vo *vo, int format)
{
    struct gpu_priv *p = vo->priv;
    if (!gl_video_check_format(p->renderer, format))
        return 0;
    return 1;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct gpu_priv *p = vo->priv;

    if (!p->ctx->fns->reconfig(p->ctx))
        return -1;

    resize(vo);
    gl_video_config(p->renderer, params);

    return 0;
}

static void request_hwdec_api(struct vo *vo)
{
    struct gpu_priv *p = vo->priv;

    gl_video_load_hwdecs_all(p->renderer, vo->hwdec_devs);
}

static void call_request_hwdec_api(void *ctx)
{
    // Roundabout way to run hwdec loading on the VO thread.
    // Redirects to request_hwdec_api().
    vo_control(ctx, VOCTRL_LOAD_HWDEC_API, NULL);
}

static void get_and_update_icc_profile(struct gpu_priv *p)
{
    if (gl_video_icc_auto_enabled(p->renderer)) {
        MP_VERBOSE(p, "Querying ICC profile...\n");
        bstr icc = bstr0(NULL);
        int r = p->ctx->fns->control(p->ctx, &p->events, VOCTRL_GET_ICC_PROFILE, &icc);

        if (r != VO_NOTAVAIL) {
            if (r == VO_FALSE) {
                MP_WARN(p, "Could not retrieve an ICC profile.\n");
            } else if (r == VO_NOTIMPL) {
                MP_ERR(p, "icc-profile-auto not implemented on this platform.\n");
            }

            gl_video_set_icc_profile(p->renderer, icc);
        }
    }
}

static void get_and_update_ambient_lighting(struct gpu_priv *p)
{
    int lux;
    int r = p->ctx->fns->control(p->ctx, &p->events, VOCTRL_GET_AMBIENT_LUX, &lux);
    if (r == VO_TRUE) {
        gl_video_set_ambient_lux(p->renderer, lux);
    }
    if (r != VO_TRUE && gl_video_gamma_auto_enabled(p->renderer)) {
        MP_ERR(p, "gamma_auto option provided, but querying for ambient"
                  " lighting is not supported on this platform\n");
    }
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct gpu_priv *p = vo->priv;

    switch (request) {
    case VOCTRL_SET_PANSCAN:
        resize(vo);
        return VO_TRUE;
    case VOCTRL_SET_EQUALIZER:
        vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_SCREENSHOT: {
        struct vo_frame *frame = vo_get_current_vo_frame(vo);
        if (frame)
            gl_video_screenshot(p->renderer, frame, data);
        talloc_free(frame);
        return true;
    }
    case VOCTRL_LOAD_HWDEC_API:
        request_hwdec_api(vo);
        return true;
    case VOCTRL_UPDATE_RENDER_OPTS: {
        gl_video_configure_queue(p->renderer, vo);
        get_and_update_icc_profile(p);
        vo->want_redraw = true;
        return true;
    }
    case VOCTRL_RESET:
        gl_video_reset(p->renderer);
        return true;
    case VOCTRL_PAUSE:
        if (gl_video_showing_interpolated_frame(p->renderer))
            vo->want_redraw = true;
        break;
    case VOCTRL_PERFORMANCE_DATA:
        gl_video_perfdata(p->renderer, (struct voctrl_performance_data *)data);
        return true;
    case VOCTRL_EXTERNAL_RESIZE:
        p->ctx->fns->reconfig(p->ctx);
        resize(vo);
        return true;
    }

    int events = 0;
    int r = p->ctx->fns->control(p->ctx, &events, request, data);
    if (events & VO_EVENT_ICC_PROFILE_CHANGED) {
        get_and_update_icc_profile(p);
        vo->want_redraw = true;
    }
    if (events & VO_EVENT_AMBIENT_LIGHTING_CHANGED) {
        get_and_update_ambient_lighting(p);
        vo->want_redraw = true;
    }
    events |= p->events;
    p->events = 0;
    if (events & VO_EVENT_RESIZE)
        resize(vo);
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    vo_event(vo, events);

    return r;
}

static void wakeup(struct vo *vo)
{
    struct gpu_priv *p = vo->priv;
    if (p->ctx && p->ctx->fns->wakeup)
        p->ctx->fns->wakeup(p->ctx);
}

static void wait_events(struct vo *vo, int64_t until_time_us)
{
    struct gpu_priv *p = vo->priv;
    if (p->ctx && p->ctx->fns->wait_events) {
        p->ctx->fns->wait_events(p->ctx, until_time_us);
    } else {
        vo_wait_default(vo, until_time_us);
    }
}

static struct mp_image *get_image(struct vo *vo, int imgfmt, int w, int h,
                                  int stride_align)
{
    struct gpu_priv *p = vo->priv;

    return gl_video_get_image(p->renderer, imgfmt, w, h, stride_align);
}

static void uninit(struct vo *vo)
{
    struct gpu_priv *p = vo->priv;

    gl_video_uninit(p->renderer);
    if (vo->hwdec_devs) {
        hwdec_devices_set_loader(vo->hwdec_devs, NULL, NULL);
        hwdec_devices_destroy(vo->hwdec_devs);
    }
    ra_ctx_destroy(&p->ctx);
}

static int preinit(struct vo *vo)
{
    struct gpu_priv *p = vo->priv;
    p->log = vo->log;

    int alpha_mode;
    mp_read_option_raw(vo->global, "alpha", &m_option_type_choice, &alpha_mode);

    struct ra_ctx_opts opts = p->opts;
    opts.want_alpha = alpha_mode == 1;

    p->ctx = ra_ctx_create(vo, p->context_type, p->context_name, opts);
    if (!p->ctx)
        goto err_out;
    assert(p->ctx->ra);
    assert(p->ctx->swapchain);

    p->renderer = gl_video_init(p->ctx->ra, vo->log, vo->global);
    gl_video_set_osd_source(p->renderer, vo->osd);
    gl_video_configure_queue(p->renderer, vo);

    get_and_update_icc_profile(p);

    vo->hwdec_devs = hwdec_devices_create();
    hwdec_devices_set_loader(vo->hwdec_devs, call_request_hwdec_api, vo);

    gl_video_load_hwdecs(p->renderer, vo->hwdec_devs, false);

    return 0;

err_out:
    uninit(vo);
    return -1;
}

#define OPT_BASE_STRUCT struct gpu_priv
static const m_option_t options[] = {
    OPT_STRING_VALIDATE("gpu-context", context_name, 0, ra_ctx_validate_context),
    OPT_STRING_VALIDATE("gpu-api", context_type, 0, ra_ctx_validate_api),
    OPT_FLAG("gpu-debug", opts.debug, 0),
    OPT_FLAG("gpu-sw", opts.allow_sw, 0),
    OPT_INTRANGE("swapchain-depth", opts.swapchain_depth, 0, 1, 8),
    {0}
};

static const struct gpu_priv defaults = { .opts = {
    .swapchain_depth = 3,
}};

const struct vo_driver video_out_gpu = {
    .description = "Shader-based GPU Renderer",
    .name = "gpu",
    .caps = VO_CAP_ROTATE90,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .get_image = get_image,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .get_latency = get_latency,
    .wait_events = wait_events,
    .wakeup = wakeup,
    .uninit = uninit,
    .priv_size = sizeof(struct gpu_priv),
    .priv_defaults = &defaults,
    .options = options,
};
