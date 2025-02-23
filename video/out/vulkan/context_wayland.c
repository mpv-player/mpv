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
#include "video/out/present_sync.h"
#include "video/out/wayland_common.h"

#include "common.h"
#include "context.h"
#include "utils.h"

struct priv {
    struct mpvk_ctx vk;
    bool use_fifo;
};

static bool wayland_vk_check_visible(struct ra_ctx *ctx)
{
    return vo_wayland_check_visible(ctx->vo);
}

static void wayland_vk_swap_buffers(struct ra_ctx *ctx)
{
    struct vo_wayland_state *wl = ctx->vo->wl;
    struct priv *p = ctx->priv;

    if ((!p->use_fifo && wl->opts->wl_internal_vsync == 1) || wl->opts->wl_internal_vsync == 2)
        vo_wayland_wait_frame(wl);

    if (wl->use_present)
        present_sync_swap(wl->present);
}

static void wayland_vk_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct vo_wayland_state *wl = ctx->vo->wl;
    if (wl->use_present)
        present_sync_get_info(wl->present, info);
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

    struct ra_ctx_params params = {
        .check_visible = wayland_vk_check_visible,
        .swap_buffers = wayland_vk_swap_buffers,
        .get_vsync = wayland_vk_get_vsync,
    };

    VkInstance inst = vk->vkinst->instance;
    VkResult res = vkCreateWaylandSurfaceKHR(inst, &wlinfo, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating Wayland surface\n");
        goto error;
    }

    /* If the Wayland compositor does not support fifo and presentation time
     * v2 protocols, the compositor will stop sending callbacks if the surface
     * is no longer visible. This means using FIFO would block the entire vo
     * thread which is just not good. Use MAILBOX for those compositors to
     * avoid indefinite blocking. */
    struct vo_wayland_state *wl = ctx->vo->wl;
    p->use_fifo = wl->has_fifo && wl->present_v2 && wl->opts->wl_internal_vsync != 2;
    if (!ra_vk_ctx_init(ctx, vk, params, p->use_fifo ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR))
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

    const int32_t width = mp_rect_w(wl->geometry);
    const int32_t height = mp_rect_h(wl->geometry);

    vo_wayland_set_opaque_region(wl, ctx->opts.want_alpha);
    vo_wayland_handle_scale(wl);
    return ra_vk_ctx_resize(ctx, width, height);
}

static bool wayland_vk_reconfig(struct ra_ctx *ctx)
{
    return vo_wayland_reconfig(ctx->vo);
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

static void wayland_vk_wait_events(struct ra_ctx *ctx, int64_t until_time_ns)
{
    vo_wayland_wait_events(ctx->vo, until_time_ns);
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
    .description        = "Wayland/Vulkan",
    .reconfig           = wayland_vk_reconfig,
    .control            = wayland_vk_control,
    .wakeup             = wayland_vk_wakeup,
    .wait_events        = wayland_vk_wait_events,
    .update_render_opts = wayland_vk_update_render_opts,
    .init               = wayland_vk_init,
    .uninit             = wayland_vk_uninit,
};
