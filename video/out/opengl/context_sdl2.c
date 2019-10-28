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

#include <SDL.h>

#include "../sdl_common.h"
#include "options/m_config.h"
#include "context.h"
#include "utils.h"

struct priv {
    GL gl;
    SDL_GLContext context;
};

static void sdl2_uninit(struct ra_ctx *ctx);

static int GLAPIENTRY sdl2_swap_interval(int interval)
{
    return SDL_GL_SetSwapInterval(interval);
}

static void *sdlgpa(const GLubyte *procName)
{
    return SDL_GL_GetProcAddress(procName);
}

static void sdl2_swap_buffers(struct ra_ctx *ctx)
{
    SDL_GL_SwapWindow(ctx->vo->sdl->window);
}

static bool sdl2_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    struct vo *vo = ctx->vo;
    GL *gl = &p->gl;

    if (!vo_sdl_init(vo, SDL_WINDOW_OPENGL)) {
        sdl2_uninit(ctx);
        return false;
    }

    p->context = SDL_GL_CreateContext(vo->sdl->window);
    if (!p->context) {
        MP_FATAL(vo, "Could not create GL context!\n");
        sdl2_uninit(ctx);
        return false;
    }

    SDL_GL_MakeCurrent(vo->sdl->window, p->context);

    mpgl_load_functions(gl, sdlgpa, NULL, vo->log);

    gl->SwapInterval = sdl2_swap_interval;

    struct ra_gl_ctx_params params = {
        .swap_buffers = sdl2_swap_buffers,
    };

    if (!ra_gl_ctx_init(ctx, gl, params)) {
        sdl2_uninit(ctx);
        return false;
    }

    return true;
}

static void resize(struct ra_ctx *ctx)
{
    int w, h;
    struct vo *vo = ctx->vo;
    SDL_GetWindowSize(vo->sdl->window, &w, &h);

    ctx->vo->dwidth = w;
    ctx->vo->dheight = h;

    ra_gl_ctx_resize(ctx->swapchain, ctx->vo->dwidth, ctx->vo->dheight, 0);
}

static bool sdl2_reconfig(struct ra_ctx *ctx)
{
    vo_sdl_config(ctx->vo);
    resize(ctx);
    return true;
}

static void sdl2_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);
    SDL_GL_DeleteContext(p->context);
    p->context = NULL;
    vo_sdl_uninit(ctx->vo);
}

static int sdl2_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_sdl_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE)
        resize(ctx);
    return ret;
}

static void sdl2_wakeup(struct ra_ctx *ctx)
{
    vo_sdl_wakeup(ctx->vo);
}

static void sdl2_wait_events(struct ra_ctx *ctx, int64_t until_time_us)
{
    vo_sdl_wait_events(ctx->vo, until_time_us);
}

const struct ra_ctx_fns ra_ctx_sdl2 = {
    .type           = "opengl",
    .name           = "sdl2",
    .init           = sdl2_init,
    .reconfig       = sdl2_reconfig,
    .control        = sdl2_control,
    .uninit         = sdl2_uninit,
    .wait_events    = sdl2_wait_events,
    .wakeup         = sdl2_wakeup,
};
