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

struct priv {
    struct mpvk_ctx vk;
};

static bool wayland_vk_start_frame(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wl;
    bool render = !wl->hidden || wl->opts->disable_vsync;
    wl->frame_wait = true;

    return render;
}

static void wayland_vk_swap_buffers(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wl;

    if (!wl->opts->disable_vsync)
        vo_wayland_wait_frame(wl);

    if (wl->presentation)
        vo_wayland_sync_swap(wl);
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
        .start_frame = wayland_vk_start_frame,
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

    return true;

error:
    wayland_vk_uninit(ctx);
    return false;
}

static bool resize(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wl;

    MP_VERBOSE(wl, "Handling resize on the vk side\n");

    const int32_t width = wl->scaling * mp_rect_w(wl->geometry);
    const int32_t height = wl->scaling * mp_rect_h(wl->geometry);

    vo_wayland_set_opaque_region(wl, ctx->opts.want_alpha);
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

static void wayland_vk_update_render_opts(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wl;
    vo_wayland_set_opaque_region(wl, ctx->opts.want_alpha);
    wl_surface_commit(wl->surface);
}

const struct ra_ctx_fns ra_ctx_vulkan_wayland = {
    .type               = "vulkan",
    .name               = "waylandvk",
    .reconfig           = wayland_vk_reconfig,
    .control            = wayland_vk_control,
    .wakeup             = wayland_vk_wakeup,
    .wait_events        = wayland_vk_wait_events,
    .update_render_opts = wayland_vk_update_render_opts,
    .init               = wayland_vk_init,
    .uninit             = wayland_vk_uninit,
};
