#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "config.h"

#include "common/common.h"
#include "common/msg.h"

// Helpers for loading functions at runtime. Omit the "vk" prefix.
// VK_LOAD_FUN_PL is preferred if possible.
#define VK_LOAD_FUN_PL(vkinst, name) \
    PFN_vk##name name = (PFN_vk##name) vkinst->get_proc_addr(vkinst->instance, "vk" #name);
#define VK_LOAD_FUN(inst, name) \
    PFN_vk##name name = (PFN_vk##name) vkGetInstanceProcAddr(inst, "vk" #name);

// We need to define all platforms we want to support. Since we have
// our own mechanism for checking this, we re-define the right symbols
#if HAVE_WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#if HAVE_X11
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#if HAVE_WIN32_DESKTOP
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <libplacebo/vulkan.h>

// Shared struct used to hold vulkan context information
struct mpvk_ctx {
    pl_log pllog;
    pl_vk_inst vkinst;
    pl_vulkan vulkan;
    pl_gpu gpu; // points to vulkan->gpu for convenience
    pl_swapchain swapchain;
    VkSurfaceKHR surface;
};
