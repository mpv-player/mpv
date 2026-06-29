/* Copyright (C) 2024 the mpv developers
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

#ifndef MPV_CLIENT_API_RENDER_LIBPLACEBO_H_
#define MPV_CLIENT_API_RENDER_LIBPLACEBO_H_

#include "render.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Libplacebo backend
 * ------------------------------------
 *
 * This header contains definitions for using the libplacebo renderer with
 * the render.h API. The libplacebo renderer allows the user to use the
 * libplacebo library directly.
 *
 * The client is responsible for:
 * - Creating and managing the graphics context (Vulkan instance/device or
 *   OpenGL context)
 * - Creating and providing a pl_swapchain from libplacebo
 *
 * Basic usage:
 *
 *   1. Create pl_log using pl_log_create()
 *   2. Create a Vulkan instance using pl_vk_inst_create() if using Vulkan
 *   3. Create a Vulkan or OpenGL context using
 *   pl_vk_create()/pl_vulkan_import() or pl_opengl_create()
 *   4. Create a pl_swapchain using pl_opengl_create_swapchain() or
 *   pl_vulkan_create_swapchain()
 *   5. Call mpv_render_context_create() with:
 *      - MPV_RENDER_PARAM_API_TYPE = MPV_RENDER_API_TYPE_LIBPLACEBO
 *      - MPV_RENDER_PARAM_LIBPLACEBO_SWAPCHAIN = pl_swapchain
 *      - MPV_RENDER_PARAM_LIBPLACEBO_PL_LOG = pl_log
 *   6. Create mpv_render_param array with the following parameters
 *      (optional):
 *      - MPV_RENDER_PARAM_LIBPLACEBO_OPTIONS = pl_options
 *      - MPV_RENDER_PARAM_LIBPLACEBO_FRAME = pl_swapchain_frame
 *      - MPV_RENDER_PARAM_LIBPLACEBO_VIEWPORT = mpv_render_rect
 *   7. Render loop: mpv_render_context_render() then
 *      pl_swapchain_submit_frame() and pl_swapchain_swap_buffers() if a
 *      frame is provided
 *
 * Main notes:
 * - Make sure to keep the swapchain valid for the lifetime of the render
 *   context
 * - Make sure to use the same libplacebo library version that the mpv
 *   library has been compiled with.
 */

typedef struct mpv_render_rect_t
{
int x0, y0;
int x1, y1;
} *mpv_render_rect;

 enum
 {

     /**
      * Required for mpv_render_context_create().
      * Type: pl_swapchain (from libplacebo)
      *
      * The swapchain that mpv will render to. The client is responsible for
      * creating this swapchain and calling pl_swapchain_swap_buffers() after
      * mpv_render_context_render() returns.
      *
      * The swapchain must remain valid for the lifetime of the render context.
      */
     MPV_RENDER_PARAM_LIBPLACEBO_SWAPCHAIN = 101,

     /**
      * Optional for mpv_render_context_create().
      * Type: pl_log (from libplacebo)
      *
      * A libplacebo log context for receiving log messages. If not provided,
      * mpv will create its own pl_log instance.
      */
     MPV_RENDER_PARAM_LIBPLACEBO_PL_LOG = 102,
     /**
      * Optional for mpv_render_context_render().
      * Type: pl_options (from libplacebo)
      *
      * A libplacebo options context for passing rendering options. If not
      * provided, mpv will use the default options.
      */
     MPV_RENDER_PARAM_LIBPLACEBO_OPTIONS = 103,

     /**
      * Optional for mpv_render_context_render().
      * Type: pl_swapchain_frame (from libplacebo)
      *
      * A libplacebo frame to render to. If not provided, mpv will start a
      * new swapchain frame and render to it, submit the frame to the swapchain
      * and call pl_swapchain_swap_buffers(). The user MUST NOT call
      * pl_swapchain_submit_frame() and pl_swapchain_swap_buffers() after
      * rendering. If the frame is provided, mpv will not submit the frame to
      * the swapchain and will not call pl_swapchain_swap_buffers(), and the
      * user will be responsible to call pl_swapchain_submit_frame() and
      * pl_swapchain_swap_buffers(). The frame must be valid for the lifetime of
      * the render context.
      */
     MPV_RENDER_PARAM_LIBPLACEBO_FRAME = 104,
     /**
      * Optional for mpv_render_context_render().
      * Type: mpv_render_rect
      *
      * The viewport rectangle to render to. If not provided, mpv will render to
      * the entire viewport.
      */
     MPV_RENDER_PARAM_LIBPLACEBO_VIEWPORT = 105,
 };

 /**
  * API type string for libplacebo backend.
  * Use with MPV_RENDER_PARAM_API_TYPE.
  */
 #define MPV_RENDER_API_TYPE_LIBPLACEBO "libplacebo"

#ifdef __cplusplus
}
#endif

#endif
