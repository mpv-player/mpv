/* Copyright (C) 2018 the mpv developers
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MPV_CLIENT_API_RENDER_VK_H_
#define MPV_CLIENT_API_RENDER_VK_H_

#include "render.h"

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Vulkan backend
 * --------------
 *
 * This header contains definitions for using Vulkan with the render.h API.
 *
 * Vulkan interop
 * --------------
 *
 * The user owns the VkInstance, VkDevice, and queues. mpv imports the device
 * using libplacebo's pl_vulkan_import(). The user provides a VkImage per frame
 * for mpv to render into.
 *
 * Requirements:
 * - Vulkan 1.3 or higher
 * - Device must have timeline semaphore support
 * - User must enable required extensions when creating the device (see below)
 *
 * Required device extensions (user must enable these):
 * - VK_KHR_timeline_semaphore (core in 1.2+)
 * - VK_KHR_external_memory (for hwdec interop)
 *
 * Recommended device extensions:
 * - VK_KHR_video_decode_queue (for Vulkan video decode hwdec)
 * - VK_KHR_video_decode_h264
 * - VK_KHR_video_decode_h265
 *
 * Synchronization
 * ---------------
 *
 * The user provides binary or timeline semaphores for synchronization:
 * - wait_semaphore: mpv waits on this before rendering (user signals when
 *   the target image is ready to be written)
 * - signal_semaphore: mpv signals this after rendering completes (user waits
 *   on this before presenting or using the image)
 *
 * If semaphores are not provided (NULL), mpv will use pl_gpu_finish() which
 * is less efficient but simpler.
 *
 * Hardware decoding
 * -----------------
 *
 * For Vulkan hwdec to work, the user must:
 * - Provide MPV_RENDER_PARAM_WL_DISPLAY (Wayland) for dmabuf import
 * - Enable the appropriate video decode extensions on the device
 * - Ensure the device supports VK_QUEUE_VIDEO_DECODE_BIT_KHR
 */

/**
 * For initializing the mpv Vulkan state via MPV_RENDER_PARAM_VULKAN_INIT_PARAMS.
 */
typedef struct mpv_vulkan_init_params {
    /**
     * Vulkan instance. Must remain valid for the lifetime of the render context.
     */
    VkInstance instance;

    /**
     * Physical device to use for rendering. Must be from the provided instance.
     */
    VkPhysicalDevice physical_device;

    /**
     * Logical device. User creates this with required extensions enabled.
     * Must remain valid for the lifetime of the render context.
     */
    VkDevice device;

    /**
     * Graphics queue for rendering commands. Must support graphics operations.
     */
    VkQueue graphics_queue;

    /**
     * Queue family index of graphics_queue.
     */
    uint32_t graphics_queue_family;

    /**
     * Function to load Vulkan instance functions. If NULL, mpv will use the
     * default loader (vkGetInstanceProcAddr from the Vulkan library).
     */
    PFN_vkGetInstanceProcAddr get_instance_proc_addr;

    /**
     * Optional: Pointer to VkPhysicalDeviceFeatures2 describing the features
     * enabled on the device. The pNext chain should include any Vulkan 1.1+
     * feature structures that were enabled (e.g., VkPhysicalDeviceVulkan12Features).
     *
     * The device MUST have been created with at least the features required
     * by libplacebo, including:
     * - hostQueryReset (Vulkan 1.2 / VK_EXT_host_query_reset)
     * - timelineSemaphore (Vulkan 1.2 / VK_KHR_timeline_semaphore)
     *
     * If NULL, mpv will assume the device was created with these features enabled.
     */
    const VkPhysicalDeviceFeatures2 *features;

    /**
     * Optional: List of device-level extensions that were enabled.
     * This is passed to libplacebo so it knows which extensions are available.
     */
    const char * const *extensions;

    /**
     * Number of extensions in the extensions array.
     */
    int num_extensions;
} mpv_vulkan_init_params;

/**
 * For MPV_RENDER_PARAM_VULKAN_FBO - describes the render target.
 */
typedef struct mpv_vulkan_fbo {
    /**
     * The VkImage to render into. Must be created with VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
     * and VK_IMAGE_USAGE_TRANSFER_DST_BIT.
     */
    VkImage image;

    /**
     * Image view for the target image. Must be a 2D view of the image.
     */
    VkImageView image_view;

    /**
     * Image dimensions in pixels.
     */
    uint32_t width;
    uint32_t height;

    /**
     * Image format. Should be a renderable format like VK_FORMAT_B8G8R8A8_UNORM
     * or VK_FORMAT_R8G8B8A8_UNORM.
     */
    VkFormat format;

    /**
     * Current layout of the image when passed to mpv. mpv will transition
     * the image from this layout before rendering.
     */
    VkImageLayout current_layout;

    /**
     * Desired layout after rendering. mpv will transition the image to this
     * layout after rendering completes.
     */
    VkImageLayout target_layout;
} mpv_vulkan_fbo;

/**
 * For MPV_RENDER_PARAM_VULKAN_SYNC - synchronization primitives.
 * All fields are optional. If not provided, mpv uses GPU finish for sync.
 */
typedef struct mpv_vulkan_sync {
    /**
     * Semaphore that mpv waits on before starting to render.
     * The user should signal this when the target image is ready to be written.
     * Can be VK_NULL_HANDLE to skip waiting.
     */
    VkSemaphore wait_semaphore;

    /**
     * For timeline semaphores: the value to wait for. Ignored for binary semaphores.
     * Set to 0 for binary semaphores.
     */
    uint64_t wait_value;

    /**
     * Semaphore that mpv signals after rendering completes.
     * The user should wait on this before presenting or reading the image.
     * Can be VK_NULL_HANDLE to skip signaling.
     */
    VkSemaphore signal_semaphore;

    /**
     * For timeline semaphores: the value to signal. Ignored for binary semaphores.
     * Set to 0 for binary semaphores.
     */
    uint64_t signal_value;
} mpv_vulkan_sync;

#ifdef __cplusplus
}
#endif

#endif
