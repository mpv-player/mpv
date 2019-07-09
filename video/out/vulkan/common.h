#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "config.h"

#include "common/common.h"
#include "common/msg.h"

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
    struct mp_log *pl_log;
    struct pl_context *ctx;
    const struct pl_vk_inst *vkinst;
    const struct pl_vulkan *vulkan;
    const struct pl_gpu *gpu; // points to vulkan->gpu for convenience
    VkSurfaceKHR surface;
};
