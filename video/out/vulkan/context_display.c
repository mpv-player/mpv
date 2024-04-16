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

#include "common/common.h"
#include "context.h"
#include "options/m_config.h"
#include "utils.h"
#include "video/out/placebo/utils.h"

#if HAVE_DRM
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "libmpv/render_gl.h"
#include "video/out/drm_common.h"
#endif

struct vulkan_display_opts {
    int display;
    int mode;
    int plane;
};

struct mode_selector {
    // Indexes of selected display/mode/plane.
    int display_idx;
    int mode_idx;
    int plane_idx;

    // Must be freed with talloc_free
    VkDisplayModePropertiesKHR *out_mode_props;
};

/**
 * If a selector is passed, verify that it is valid and return the matching
 * mode properties. If null is passed, walk all modes and print them out.
 */
static bool walk_display_properties(struct mp_log *log,
                                    int msgl_err,
                                    VkPhysicalDevice device,
                                    struct mode_selector *selector) {
    bool ret = false;
    VkResult res;

    int msgl_info = selector ? MSGL_TRACE : MSGL_INFO;

    // Use a dummy as parent for all other allocations.
    void *tmp = talloc_new(NULL);

    VkPhysicalDeviceProperties prop;
    vkGetPhysicalDeviceProperties(device, &prop);
    mp_msg(log, msgl_info, "  '%s' (GPU ID %x:%x)\n", prop.deviceName,
           (unsigned)prop.vendorID, (unsigned)prop.deviceID);

    // Count displays. This must be done before enumerating planes with the
    // Intel driver, or it will not enumerate any planes. WTF.
    int num_displays = 0;
    vkGetPhysicalDeviceDisplayPropertiesKHR(device, &num_displays, NULL);
    if (!num_displays) {
        mp_msg(log, msgl_info, "    No available displays for device.\n");
        goto done;
    }
    if (selector && selector->display_idx + 1 > num_displays) {
        mp_msg(log, msgl_err, "Selected display (%d) not present.\n",
               selector->display_idx);
        goto done;
    }

    // Enumerate Planes
    int num_planes = 0;
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(device, &num_planes, NULL);
    if (!num_planes) {
        mp_msg(log, msgl_info, "    No available planes for device.\n");
        goto done;
    }
    if (selector && selector->plane_idx + 1 > num_planes) {
        mp_msg(log, msgl_err, "Selected plane (%d) not present.\n",
               selector->plane_idx);
        goto done;
    }

    VkDisplayPlanePropertiesKHR *planes =
        talloc_array(tmp, VkDisplayPlanePropertiesKHR, num_planes);
    res = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(device, &num_planes,
                                                       planes);
    if (res != VK_SUCCESS) {
        mp_msg(log, msgl_err, "    Failed enumerating planes\n");
        goto done;
    }

    // Allocate zeroed arrays so that planes with no displays have a null entry.
    VkDisplayKHR **planes_to_displays =
        talloc_zero_array(tmp, VkDisplayKHR *, num_planes);
    for (int j = 0; j < num_planes; j++) {
        int num_displays_for_plane = 0;
        vkGetDisplayPlaneSupportedDisplaysKHR(device, j,
                                              &num_displays_for_plane, NULL);
        if (!num_displays_for_plane)
            continue;

        // Null terminated array
        VkDisplayKHR *displays =
            talloc_zero_array(planes_to_displays, VkDisplayKHR,
                              num_displays_for_plane + 1);
        res = vkGetDisplayPlaneSupportedDisplaysKHR(device, j,
                                                    &num_displays_for_plane,
                                                    displays);
        if (res != VK_SUCCESS) {
            mp_msg(log, msgl_err, "      Failed enumerating plane displays\n");
            continue;
        }
        planes_to_displays[j] = displays;
    }

    // Enumerate Displays and Modes
    VkDisplayPropertiesKHR *props =
        talloc_array(tmp, VkDisplayPropertiesKHR, num_displays);
    res = vkGetPhysicalDeviceDisplayPropertiesKHR(device, &num_displays, props);
    if (res != VK_SUCCESS) {
        mp_msg(log, msgl_err, "    Failed enumerating display properties\n");
        goto done;
    }

    for (int j = 0; j < num_displays; j++) {
        if (selector && selector->display_idx != j)
            continue;

        mp_msg(log, msgl_info, "    Display %d: '%s' (%dx%d)\n",
               j,
               props[j].displayName,
               props[j].physicalResolution.width,
               props[j].physicalResolution.height);

        VkDisplayKHR display = props[j].display;

        mp_msg(log, msgl_info, "    Modes:\n");

        int num_modes = 0;
        vkGetDisplayModePropertiesKHR(device, display, &num_modes, NULL);
        if (!num_modes) {
            mp_msg(log, msgl_info, "      No available modes for display.\n");
            continue;
        }
        if (selector && selector->mode_idx + 1 > num_modes) {
            mp_msg(log, msgl_err, "Selected mode (%d) not present.\n",
                   selector->mode_idx);
            goto done;
        }

        VkDisplayModePropertiesKHR *modes =
            talloc_array(tmp, VkDisplayModePropertiesKHR, num_modes);
        res = vkGetDisplayModePropertiesKHR(device, display, &num_modes, modes);
        if (res != VK_SUCCESS) {
            mp_msg(log, msgl_err, "      Failed enumerating display modes\n");
            continue;
        }

        for (int k = 0; k < num_modes; k++) {
            if (selector && selector->mode_idx != k)
                continue;

            mp_msg(log, msgl_info, "      Mode %02d: %dx%d (%02d.%03d Hz)\n", k,
                   modes[k].parameters.visibleRegion.width,
                   modes[k].parameters.visibleRegion.height,
                   modes[k].parameters.refreshRate / 1000,
                   modes[k].parameters.refreshRate % 1000);

            if (selector)
                selector->out_mode_props = talloc_dup(NULL, &modes[k]);
        }

        int found_plane = -1;
        mp_msg(log, msgl_info, "    Planes:\n");
        for (int k = 0; k < num_planes; k++) {
            VkDisplayKHR *displays = planes_to_displays[k];
            if (!displays) {
                // This plane is not connected to any displays.
                continue;
            }
            for (int d = 0; displays[d]; d++) {
                if (displays[d] == display) {
                    if (selector && selector->plane_idx != k)
                        continue;

                    mp_msg(log, msgl_info, "      Plane: %d\n", k);
                    found_plane = k;
                }
            }
        }
        if (selector && selector->plane_idx != found_plane) {
            mp_msg(log, msgl_err,
                   "Selected plane (%d) not available on selected display.\n",
                   selector->plane_idx);
            goto done;
        }
    }
    ret = true;
done:
    talloc_free(tmp);
    return ret;
}

static int print_display_info(struct mp_log *log, const struct m_option *opt,
                              struct bstr name)
{
    void *ta_ctx = talloc_new(NULL);
    pl_log pllog = mppl_log_create(ta_ctx, log);
    if (!pllog)
        goto done;

    // Create a dummy instance to list the resources
    mppl_log_set_probing(pllog, true);
    pl_vk_inst inst = pl_vk_inst_create(pllog, pl_vk_inst_params(
        .extensions = (const char *[]){
            VK_KHR_DISPLAY_EXTENSION_NAME,
        },
        .num_extensions = 1,
    ));
    mppl_log_set_probing(pllog, false);
    if (!inst) {
        mp_warn(log, "Unable to create Vulkan instance.\n");
        goto done;
    }

    uint32_t num_devices = 0;
    VkResult res = vkEnumeratePhysicalDevices(inst->instance, &num_devices, NULL);
    if (res != VK_SUCCESS || !num_devices) {
        mp_info(log, "No Vulkan devices detected.\n");
        goto done;
    }

    VkPhysicalDevice *devices = talloc_array(ta_ctx, VkPhysicalDevice, num_devices);
    res = vkEnumeratePhysicalDevices(inst->instance, &num_devices, devices);
    if (res != VK_SUCCESS) {
        mp_warn(log, "Failed enumerating physical devices.\n");
        goto done;
    }

    mp_info(log, "Vulkan Devices:\n");
    for (int i = 0; i < num_devices; i++) {
        walk_display_properties(log, MSGL_WARN, devices[i], NULL);
    }

done:
    pl_vk_inst_destroy(&inst);
    pl_log_destroy(&pllog);
    talloc_free(ta_ctx);
    return M_OPT_EXIT;
}

#define OPT_BASE_STRUCT struct vulkan_display_opts
const struct m_sub_options vulkan_display_conf = {
    .opts = (const struct m_option[]) {
        {"vulkan-display-display", OPT_INT(display),
            .help = print_display_info,
        },
        {"vulkan-display-mode", OPT_INT(mode),
            .help = print_display_info,
        },
        {"vulkan-display-plane", OPT_INT(plane),
            .help = print_display_info,
        },
        {0}
    },
    .size = sizeof(struct vulkan_display_opts),
    .defaults = &(struct vulkan_display_opts) {0},
};

struct priv {
    struct mpvk_ctx vk;
    struct vulkan_display_opts *opts;
    uint32_t width;
    uint32_t height;

#if HAVE_DRM
    struct mpv_opengl_drm_params_v2 drm_params;
#endif
};

#if HAVE_DRM
static void open_render_fd(struct ra_ctx *ctx, const char *render_path)
{
    struct priv *p = ctx->priv;
    p->drm_params.fd = -1;
    p->drm_params.render_fd = open(render_path, O_RDWR | O_CLOEXEC);
    if (p->drm_params.render_fd == -1) {
        MP_WARN(ctx, "Failed to open render node: %s\n",
                mp_strerror(errno));
    }
}

static bool drm_setup(struct ra_ctx *ctx, int display_idx,
                      VkPhysicalDevicePCIBusInfoPropertiesEXT *pci_props)
{
    drmDevice *devs[32] = {};
    int count = drmGetDevices2(0, devs, MP_ARRAY_SIZE(devs));
    for (int i = 0; i < count; i++) {
        drmDevice *dev = devs[i];

        if (dev->bustype != DRM_BUS_PCI ||
            dev->businfo.pci->domain != pci_props->pciDomain ||
            dev->businfo.pci->bus != pci_props->pciBus ||
            dev->businfo.pci->dev != pci_props->pciDevice ||
            dev->businfo.pci->func != pci_props->pciFunction)
        {
            continue;
        }

        // Found our matching device.
        MP_DBG(ctx, "DRM device found for Vulkan device at %04X:%02X:%02X:%02X\n",
                pci_props->pciDomain, pci_props->pciBus,
                pci_props->pciDevice, pci_props->pciFunction);

        if (!(dev->available_nodes & 1 << DRM_NODE_RENDER)) {
            MP_DBG(ctx, "Card does not have a render node.\n");
            continue;
        }

        open_render_fd(ctx, dev->nodes[DRM_NODE_RENDER]);

        break;
    }
    drmFreeDevices(devs, MP_ARRAY_SIZE(devs));

    struct priv *p = ctx->priv;
    if (p->drm_params.render_fd == -1) {
        MP_WARN(ctx, "Couldn't open DRM render node for Vulkan device "
                     "at: %04X:%02X:%02X:%02X\n",
                     pci_props->pciDomain, pci_props->pciBus,
                     pci_props->pciDevice, pci_props->pciFunction);
        return false;
    }

    return true;
}
#endif

static void display_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    ra_vk_ctx_uninit(ctx);
    mpvk_uninit(&p->vk);

#if HAVE_DRM
    if (p->drm_params.render_fd != -1) {
        close(p->drm_params.render_fd);
        p->drm_params.render_fd = -1;
    }
#endif
}

static bool display_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct mpvk_ctx *vk = &p->vk;
    int msgl = ctx->opts.probing ? MSGL_V : MSGL_ERR;
    VkResult res;
    bool ret = false;

    VkDisplayModePropertiesKHR *mode = NULL;

    p->opts = mp_get_config_group(p, ctx->global, &vulkan_display_conf);
    int display_idx = p->opts->display;
    int mode_idx = p->opts->mode;
    int plane_idx = p->opts->plane;

    if (!mpvk_init(vk, ctx, VK_KHR_DISPLAY_EXTENSION_NAME))
        goto error;

    char *device_name = ra_vk_ctx_get_device_name(ctx);
    struct pl_vulkan_device_params vulkan_params = {
        .instance = vk->vkinst->instance,
        .device_name = device_name,
    };
    VkPhysicalDevice device = pl_vulkan_choose_device(vk->pllog, &vulkan_params);
    talloc_free(device_name);
    if (!device) {
        MP_MSG(ctx, msgl, "Failed to open physical device.\n");
        goto error;
    }

#if HAVE_DRM
        VkPhysicalDevicePCIBusInfoPropertiesEXT pci_props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT,
        };
        VkPhysicalDeviceProperties2KHR props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
            .pNext = &pci_props,
        };
        vkGetPhysicalDeviceProperties2(device, &props);

        if (!drm_setup(ctx, display_idx, &pci_props))
            MP_WARN(ctx, "Failed to set up DRM.\n");
#endif

    struct mode_selector selector = {
        .display_idx = display_idx,
        .mode_idx = mode_idx,
        .plane_idx = plane_idx,

    };
    if (!walk_display_properties(ctx->log, msgl, device, &selector))
        goto error;
    mode = selector.out_mode_props;

    VkDisplaySurfaceCreateInfoKHR xinfo = {
        .sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
        .displayMode = mode->displayMode,
        .imageExtent = mode->parameters.visibleRegion,
        .planeIndex = plane_idx,
        .transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR,
    };

    res = vkCreateDisplayPlaneSurfaceKHR(vk->vkinst->instance, &xinfo, NULL,
                                         &vk->surface);
    if (res != VK_SUCCESS) {
        MP_MSG(ctx, msgl, "Failed creating Display surface\n");
        goto error;
    }

    p->width = mode->parameters.visibleRegion.width;
    p->height = mode->parameters.visibleRegion.height;

    struct ra_vk_ctx_params params = {0};
    if (!ra_vk_ctx_init(ctx, vk, params, VK_PRESENT_MODE_FIFO_KHR))
        goto error;

#if HAVE_DRM
    if (p->drm_params.render_fd > -1) {
        ra_add_native_resource(ctx->ra, "drm_params_v2", &p->drm_params);
    } else {
        MP_WARN(ctx,
               "No DRM render fd available. VAAPI hwaccel will not be usable.\n");
    }
#endif

    ret = true;

done:
    talloc_free(mode);
    return ret;

error:
    display_uninit(ctx);
    goto done;
}

static bool display_reconfig(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    return ra_vk_ctx_resize(ctx, p->width, p->height);
}

static int display_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    return VO_NOTIMPL;
}

static void display_wakeup(struct ra_ctx *ctx)
{
    // TODO
}

static void display_wait_events(struct ra_ctx *ctx, int64_t until_time_ns)
{
    // TODO
}

const struct ra_ctx_fns ra_ctx_vulkan_display = {
    .type           = "vulkan",
    .name           = "displayvk",
    .description    = "VK_KHR_display",
    .reconfig       = display_reconfig,
    .control        = display_control,
    .wakeup         = display_wakeup,
    .wait_events    = display_wait_events,
    .init           = display_init,
    .uninit         = display_uninit,
};
