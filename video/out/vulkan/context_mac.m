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

#import <QuartzCore/QuartzCore.h>

#include "video/out/gpu/context.h"
#include "osdep/mac/swift.h"

#include "common.h"
#include "context.h"
#include "utils.h"

struct priv {
    struct mpvk_ctx vk;
    MacCommon *vo_mac;
};

static void mac_vk_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);
    [p->vo_mac uninit:ctx->vo];
}

static void mac_vk_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    [p->vo_mac swapBuffer];
}

static void mac_vk_get_vsync(struct ra_ctx *ctx, struct vo_vsync_info *info)
{
    struct priv *p = ctx->priv;
    [p->vo_mac fillVsyncWithInfo:info];
}

static int mac_vk_color_depth(struct ra_ctx *ctx)
{
    return 0;
}

static bool mac_vk_check_visible(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    return [p->vo_mac isVisible];
}

static int mac_vk_device_score(VkPhysicalDeviceType type, VkDriverId driver)
{
    static const int priorities[] = {
        [VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU]   = 5,
        [VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU] = 4,
        [VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU]    = 3,
        [VK_PHYSICAL_DEVICE_TYPE_CPU]            = 2,
        [VK_PHYSICAL_DEVICE_TYPE_OTHER]          = 1,
    };

    int score = type < MP_ARRAY_SIZE(priorities) ? priorities[type] : 0;

    // prioritize none KosmicKrisp GPU drivers
    if (score > 2 && driver != VK_DRIVER_ID_MESA_KOSMICKRISP) {
        score += 10;
    }

    return score;
}

static VkPhysicalDevice mac_vk_choose_device(struct ra_ctx *ctx)
{
    char *device_name = ra_vk_ctx_get_device_name(ctx);
    if (device_name) {
        talloc_free(device_name);
        return NULL;
    }
    talloc_free(device_name);

    struct priv *p = ctx->priv;
    struct mpvk_ctx *vk = &p->vk;
    VkInstance inst = vk->vkinst->instance;
    VkPhysicalDevice device = NULL;
    uint32_t count = 0;
    int best = -1;

    VkResult res = vkEnumeratePhysicalDevices(inst, &count, NULL);
    if (res != VK_SUCCESS) {
        MP_VERBOSE(ctx, "No Vulkan Devices found.\n");
        return NULL;
    }

    VkPhysicalDevice *devices = talloc_array(ctx, VkPhysicalDevice, count);
    res = vkEnumeratePhysicalDevices(inst, &count, devices);
    if (res != VK_SUCCESS) {
        MP_VERBOSE(ctx, "Failed to enumerate Vulkan Devices.\n");
        talloc_free(devices);
        return NULL;
    }

    MP_VERBOSE(ctx, "Probing Vulkan Devices:\n");
    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceDriverProperties driver_props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
        };
        VkPhysicalDeviceProperties2 device_props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
            .pNext = &driver_props,
        };

        vkGetPhysicalDeviceProperties2(devices[i], &device_props);
        VkPhysicalDeviceType type = device_props.properties.deviceType;
        int score = mac_vk_device_score(type, driver_props.driverID);

        MP_VERBOSE(ctx, "    %d: %s (%s %s) (score: %d)\n", i, device_props.properties.deviceName,
                   driver_props.driverName, driver_props.driverInfo, score);

        if (!ctx->opts.allow_sw && type == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            MP_VERBOSE(ctx, "       excluding sw Device\n");
            continue;
        }

        if (score > best) {
            best = score;
            device = devices[i];
        }
    }

    talloc_free(devices);

    return device;
}

static bool mac_vk_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;

    if (!NSApp) {
        MP_ERR(ctx, "Failed to initialize macvk context, no NSApplication initialized.\n");
        goto error;
    }

    if (!mpvk_init(vk, ctx, VK_EXT_METAL_SURFACE_EXTENSION_NAME))
        goto error;

    p->vo_mac = [[MacCommon alloc] init:ctx->vo];
    if (!p->vo_mac)
        goto error;

    VkMetalSurfaceCreateInfoEXT mac_info = {
        .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .pLayer = p->vo_mac.layer,
    };

    struct ra_ctx_params params = {
        .swap_buffers = mac_vk_swap_buffers,
        .get_vsync = mac_vk_get_vsync,
        .color_depth = mac_vk_color_depth,
        .check_visible = mac_vk_check_visible,
    };

    VkInstance inst = vk->vkinst->instance;
    VkResult res = vkCreateMetalSurfaceEXT(inst, &mac_info, NULL, &vk->surface);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating metal surface\n");
        goto error;
    }

    if (!ra_vk_ctx_init(ctx, vk, params, VK_PRESENT_MODE_FIFO_KHR, mac_vk_choose_device(ctx)))
        goto error;

    return true;
error:
    if (p->vo_mac)
        [p->vo_mac uninit:ctx->vo];
    return false;
}

static bool resize(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (!p->vo_mac.window) {
        return false;
    }
    CGSize size = p->vo_mac.window.framePixel.size;

    return ra_vk_ctx_resize(ctx, (int)size.width, (int)size.height);
}

static bool mac_vk_reconfig(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    if (![p->vo_mac config:ctx->vo])
        return false;

    [p->vo_mac updateWithAlpha:ctx->opts.want_alpha];

    return true;
}

static int mac_vk_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    struct priv *p = ctx->priv;
    int ret = [p->vo_mac control:ctx->vo events:events request:request data:arg];

    if (*events & VO_EVENT_RESIZE) {
        if (!resize(ctx))
            return VO_ERROR;
    }

    return ret;
}

static void mac_vk_update_render_opts(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    [p->vo_mac updateWithAlpha:ctx->opts.want_alpha];
}

const struct ra_ctx_fns ra_ctx_vulkan_mac = {
    .type               = "vulkan",
    .name               = "macvk",
    .description        = "mac/Vulkan (via Metal)",
    .reconfig           = mac_vk_reconfig,
    .control            = mac_vk_control,
    .update_render_opts = mac_vk_update_render_opts,
    .init               = mac_vk_init,
    .uninit             = mac_vk_uninit,
};
