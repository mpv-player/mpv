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

#include "video/out/gpu/context.h"
#include "video/out/wayland_common.h"

#include "common.h"
#include "context.h"
#include "utils.h"

// Generated from presentation-time.xml
#include "generated/wayland/presentation-time.h"

struct priv {
    struct mpvk_ctx vk;
};

static const struct wl_callback_listener frame_listener;

static void frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
    struct vo_wayland_state *wl = data;

    if (callback)
        wl_callback_destroy(callback);

    wl->frame_callback = wl_surface_frame(wl->surface);
    wl_callback_add_listener(wl->frame_callback, &frame_listener, wl);
    wl->frame_wait = false;
}

static const struct wl_callback_listener frame_listener = {
    frame_callback,
};

static const struct wp_presentation_feedback_listener feedback_listener;

static void feedback_sync_output(void *data, struct wp_presentation_feedback *fback,
                               struct wl_output *output)
{
}

static void feedback_presented(void *data, struct wp_presentation_feedback *fback,
                              uint32_t tv_sec_hi, uint32_t tv_sec_lo,
                              uint32_t tv_nsec, uint32_t refresh_nsec,
                              uint32_t seq_hi, uint32_t seq_lo,
                              uint32_t flags)
{
    struct vo_wayland_state *wl = data;
    wp_presentation_feedback_destroy(fback);
    vo_wayland_sync_shift(wl);

    // Very similar to oml_sync_control, in this case we assume that every
    // time the compositor receives feedback, a buffer swap has been already
    // been performed.
    //
    // Notes:
    //  - tv_sec_lo + tv_sec_hi is the equivalent of oml's ust
    //  - seq_lo + seq_hi is the equivalent of oml's msc
    //  - these values are updated everytime the compositor receives feedback.

    int index = last_available_sync(wl);
    if (index < 0) {
        queue_new_sync(wl);
        index = 0;
    }
    int64_t sec = (uint64_t) tv_sec_lo + ((uint64_t) tv_sec_hi << 32);
    wl->sync[index].sbc = wl->user_sbc;
    wl->sync[index].ust = sec * 1000000LL + (uint64_t) tv_nsec / 1000;
    wl->sync[index].msc = (uint64_t) seq_lo + ((uint64_t) seq_hi << 32);
    wl->sync[index].filled = true;
}

static void feedback_discarded(void *data, struct wp_presentation_feedback *fback)
{
    wp_presentation_feedback_destroy(fback);
}

static const struct wp_presentation_feedback_listener feedback_listener = {
    feedback_sync_output,
    feedback_presented,
    feedback_discarded,
};

static void wayland_vk_swap_buffers(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wl;

    if (wl->presentation) {
        wl->feedback = wp_presentation_feedback(wl->presentation, wl->surface);
        wp_presentation_feedback_add_listener(wl->feedback, &feedback_listener, wl);
        wl->user_sbc += 1;
        int index = last_available_sync(wl);
        if (index < 0)
            queue_new_sync(wl);
    }

    if (!wl->opts->disable_vsync)
        vo_wayland_wait_frame(wl);

    if (wl->presentation)
        wayland_sync_swap(wl);

    wl->frame_wait = true;
}

static void wayland_vk_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct vo_wayland_state *wl = ctx->vo->wl;
    if (wl->presentation) {
        info->vsync_duration = wl->vsync_duration;
        info->skipped_vsyncs = wl->last_skipped_vsyncs;
        info->last_queue_display_time = wl->last_queue_display_time;
    }
}

static void wayland_vk_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);
    vo_wayland_uninit(ctx->vo);
}

static bool wayland_vk_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;

    if (!mpvk_init(vk, ctx, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME))
        goto error;

    if (!vo_wayland_init(ctx->vo))
        goto error;

    VkWaylandSurfaceCreateInfoKHR wlinfo = {
         .sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
         .display = ctx->vo->wl->display,
         .surface = ctx->vo->wl->surface,
    };

    struct ra_vk_ctx_params params = {
        .swap_buffers = wayland_vk_swap_buffers,
        .get_vsync = wayland_vk_get_vsync,
    };

    VkInstance inst = vk->vkinst->instance;
    VkResult res = vkCreateWaylandSurfaceKHR(inst, &wlinfo, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating Wayland surface\n");
        goto error;
    }

    /* Because in Wayland clients render whenever they receive a callback from
     * the compositor, and the fact that the compositor usually stops sending
     * callbacks once the surface is no longer visible, using FIFO here would
     * mean the entire player would block on acquiring swapchain images. Hence,
     * use MAILBOX to guarantee that there'll always be a swapchain image and
     * the player won't block waiting on those */
    if (!ra_vk_ctx_init(ctx, vk, params, VK_PRESENT_MODE_MAILBOX_KHR))
        goto error;

    ra_add_native_resource(ctx->ra, "wl", ctx->vo->wl->display);

    ctx->vo->wl->frame_callback = wl_surface_frame(ctx->vo->wl->surface);
    wl_callback_add_listener(ctx->vo->wl->frame_callback, &frame_listener, ctx->vo->wl);

    return true;

error:
    wayland_vk_uninit(ctx);
    return false;
}

static bool resize(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wl;

    MP_VERBOSE(wl, "Handling resize on the vk side\n");

    const int32_t width = wl->scaling*mp_rect_w(wl->geometry);
    const int32_t height = wl->scaling*mp_rect_h(wl->geometry);

    wl_surface_set_buffer_scale(wl->surface, wl->scaling);
    return ra_vk_ctx_resize(ctx, width, height);
}

static bool wayland_vk_reconfig(struct ra_ctx *ctx)
{
    if (!vo_wayland_reconfig(ctx->vo))
        return false;

    return true;
}

static int wayland_vk_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_wayland_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE) {
        if (!resize(ctx))
            return VO_ERROR;
    }
    return ret;
}

static void wayland_vk_wakeup(struct ra_ctx *ctx)
{
    vo_wayland_wakeup(ctx->vo);
}

static void wayland_vk_wait_events(struct ra_ctx *ctx, int64_t until_time_us)
{
    vo_wayland_wait_events(ctx->vo, until_time_us);
}

const struct ra_ctx_fns ra_ctx_vulkan_wayland = {
    .type           = "vulkan",
    .name           = "waylandvk",
    .reconfig       = wayland_vk_reconfig,
    .control        = wayland_vk_control,
    .wakeup         = wayland_vk_wakeup,
    .wait_events    = wayland_vk_wait_events,
    .init           = wayland_vk_init,
    .uninit         = wayland_vk_uninit,
};
