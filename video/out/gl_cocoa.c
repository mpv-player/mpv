/*
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "cocoa_common.h"
#include "gl_common.h"

static bool config_window_cocoa(struct MPGLContext *ctx, uint32_t d_width,
                                uint32_t d_height, uint32_t flags)
{
    int rv = vo_cocoa_config_window(ctx->vo, d_width, d_height, flags,
                                    ctx->requested_gl_version >= MPGL_VER(3, 0));
    if (rv != 0)
        return false;

    mpgl_load_functions(ctx->gl, (void *)vo_cocoa_glgetaddr, NULL);

    ctx->depth_r = vo_cocoa_cgl_color_size(ctx->vo);
    ctx->depth_g = vo_cocoa_cgl_color_size(ctx->vo);
    ctx->depth_b = vo_cocoa_cgl_color_size(ctx->vo);

    if (!ctx->gl->SwapInterval)
        ctx->gl->SwapInterval = vo_cocoa_swap_interval;

    return true;
}

static void releaseGlContext_cocoa(MPGLContext *ctx)
{
}

static void swapGlBuffers_cocoa(MPGLContext *ctx)
{
    vo_cocoa_swap_buffers(ctx->vo);
}

static void set_current_cocoa(MPGLContext *ctx, bool current)
{
    vo_cocoa_set_current_context(ctx->vo, current);
}

void mpgl_set_backend_cocoa(MPGLContext *ctx)
{
    ctx->config_window = config_window_cocoa;
    ctx->releaseGlContext = releaseGlContext_cocoa;
    ctx->swapGlBuffers = swapGlBuffers_cocoa;
    ctx->check_events = vo_cocoa_check_events;
    ctx->update_xinerama_info = vo_cocoa_update_xinerama_info;
    ctx->fullscreen = vo_cocoa_fullscreen;
    ctx->ontop = vo_cocoa_ontop;
    ctx->vo_init = vo_cocoa_init;
    ctx->pause = vo_cocoa_pause;
    ctx->resume = vo_cocoa_resume;
    ctx->register_resize_callback = vo_cocoa_register_resize_callback;
    ctx->vo_uninit = vo_cocoa_uninit;
    ctx->set_current = set_current_cocoa;
    ctx->set_current = set_current_cocoa;
}
