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

    struct vk_malloc *alloc;      // memory allocator for this device
    struct spirv_compiler *spirv; // GLSL -> SPIR-V compiler
    struct vk_cmdpool **pools;    // command pools (one per queue family)
    int num_pools;
    struct vk_cmd *last_cmd;      // most recently submitted command

    // Queued/pending commands. These are shared for the entire mpvk_ctx to
    // ensure submission and callbacks are FIFO
    struct vk_cmd **cmds_queued;  // recorded but not yet submitted
    struct vk_cmd **cmds_pending; // submitted but not completed
    int num_cmds_queued;
    int num_cmds_pending;

    // Pointers into *pools
    struct vk_cmdpool *pool_graphics; // required
    struct vk_cmdpool *pool_compute;  // optional
    struct vk_cmdpool *pool_transfer; // optional

    // Common pool of signals, to avoid having to re-create these objects often
    struct vk_signal **signals;
    int num_signals;

    // Cached capabilities
    VkPhysicalDeviceLimits limits;
    VkPhysicalDeviceFeatures features;
};
