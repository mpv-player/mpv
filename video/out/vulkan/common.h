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

#include <vulkan/vulkan.h>

// Vulkan allows the optional use of a custom allocator. We don't need one but
// mark this parameter with a better name in case we ever decide to change this
// in the future. (And to make the code more readable)
#define MPVK_ALLOCATOR NULL

// A lot of things depend on streaming resources across frames. Depending on
// how many frames we render ahead of time, we need to pick enough to avoid
// any conflicts, so make all of these tunable relative to this constant in
// order to centralize them.
#define MPVK_MAX_STREAMING_DEPTH 8

// Shared struct used to hold vulkan context information
struct mpvk_ctx {
    struct mp_log *log;
    VkInstance inst;
    VkPhysicalDevice physd;
    VkDebugReportCallbackEXT dbg;
    VkDevice dev;

    // Surface, must be initialized fter the context itself
    VkSurfaceKHR surf;
    VkSurfaceFormatKHR surf_format; // picked at surface initialization time

    struct vk_malloc *alloc; // memory allocator for this device
    struct vk_cmdpool *pool; // primary command pool for this device
    struct vk_cmdpool *pool_transfer; // pool for async transfers (optional)
    struct vk_cmd *last_cmd; // most recently submitted command
    struct spirv_compiler *spirv; // GLSL -> SPIR-V compiler

    // Cached capabilities
    VkPhysicalDeviceLimits limits;
};
