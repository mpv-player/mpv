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

#include <assert.h>
#include <windows.h>
#include <dwmapi.h>

#include "options/m_config.h"
#include "video/out/w32_common.h"
#include "context.h"
#include "utils.h"

#if !defined(WGL_CONTEXT_MAJOR_VERSION_ARB)
/* these are supposed to be defined in wingdi.h but mingw's is too old */
/* only the bits actually used by mplayer are defined */
/* reference: http://www.opengl.org/registry/specs/ARB/wgl_create_context.txt */

#define WGL_CONTEXT_MAJOR_VERSION_ARB          0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB          0x2092
#define WGL_CONTEXT_FLAGS_ARB                  0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB           0x9126
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB   0x00000001
#endif

struct priv {
    GL gl;

    int opt_swapinterval;
    int current_swapinterval;

    int (GLAPIENTRY *real_wglSwapInterval)(int);

    HGLRC context;
    HDC hdc;
};

static void wgl_uninit(struct ra_ctx *ctx);

static __thread struct priv *current_wgl_context;

static int GLAPIENTRY wgl_swap_interval(int interval)
{
    if (current_wgl_context)
        current_wgl_context->opt_swapinterval = interval;
    return 0;
}

static bool create_dc(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    HWND win = vo_w32_hwnd(ctx->vo);

    if (p->hdc)
        return true;

    HDC hdc = GetDC(win);
    if (!hdc)
        return false;

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;

    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(hdc, &pfd);

    if (!pf) {
        MP_ERR(ctx->vo, "unable to select a valid pixel format!\n");
        ReleaseDC(win, hdc);
        return false;
    }

    SetPixelFormat(hdc, pf, &pfd);

    p->hdc = hdc;
    return true;
}

static void *wglgpa(const GLubyte *procName)
{
    HMODULE oglmod;
    void *res = wglGetProcAddress(procName);
    if (res)
        return res;
    oglmod = GetModuleHandle(L"opengl32.dll");
    return GetProcAddress(oglmod, procName);
}

static bool create_context_wgl_old(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    HDC windc = p->hdc;
    bool res = false;

    HGLRC context = wglCreateContext(windc);
    if (!context) {
        MP_FATAL(ctx->vo, "Could not create GL context!\n");
        return res;
    }

    if (!wglMakeCurrent(windc, context)) {
        MP_FATAL(ctx->vo, "Could not set GL context!\n");
        wglDeleteContext(context);
        return res;
    }

    p->context = context;
    return true;
}

static bool create_context_wgl_gl3(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    HDC windc = p->hdc;
    HGLRC context = 0;

    // A legacy context is needed to get access to the new functions.
    HGLRC legacy_context = wglCreateContext(windc);
    if (!legacy_context) {
        MP_FATAL(ctx->vo, "Could not create GL context!\n");
        return false;
    }

    // set context
    if (!wglMakeCurrent(windc, legacy_context)) {
        MP_FATAL(ctx->vo, "Could not set GL context!\n");
        goto out;
    }

    const char *(GLAPIENTRY *wglGetExtensionsStringARB)(HDC hdc)
        = wglgpa((const GLubyte*)"wglGetExtensionsStringARB");

    if (!wglGetExtensionsStringARB)
        goto unsupported;

    const char *wgl_exts = wglGetExtensionsStringARB(windc);
    if (!strstr(wgl_exts, "WGL_ARB_create_context"))
        goto unsupported;

    HGLRC (GLAPIENTRY *wglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext,
                                                   const int *attribList)
        = wglgpa((const GLubyte*)"wglCreateContextAttribsARB");

    if (!wglCreateContextAttribsARB)
        goto unsupported;

    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 0,
        WGL_CONTEXT_FLAGS_ARB, 0,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    context = wglCreateContextAttribsARB(windc, 0, attribs);
    if (!context) {
        // NVidia, instead of ignoring WGL_CONTEXT_FLAGS_ARB, will error out if
        // it's present on pre-3.2 contexts.
        // Remove it from attribs and retry the context creation.
        attribs[6] = attribs[7] = 0;
        context = wglCreateContextAttribsARB(windc, 0, attribs);
    }
    if (!context) {
        int err = GetLastError();
        MP_FATAL(ctx->vo, "Could not create an OpenGL 3.x context: error 0x%x\n", err);
        goto out;
    }

    wglMakeCurrent(windc, NULL);
    wglDeleteContext(legacy_context);

    if (!wglMakeCurrent(windc, context)) {
        MP_FATAL(ctx->vo, "Could not set GL3 context!\n");
        wglDeleteContext(context);
        return false;
    }

    p->context = context;
    return true;

unsupported:
    MP_ERR(ctx->vo, "The OpenGL driver does not support OpenGL 3.x \n");
out:
    wglMakeCurrent(windc, NULL);
    wglDeleteContext(legacy_context);
    return false;
}

static void create_ctx(void *ptr)
{
    struct ra_ctx *ctx = ptr;
    struct priv *p = ctx->priv;

    if (!create_dc(ctx))
        return;

    create_context_wgl_gl3(ctx);
    if (!p->context)
        create_context_wgl_old(ctx);

    wglMakeCurrent(p->hdc, NULL);
}

static bool compositor_active(struct ra_ctx *ctx)
{
    // For Windows 7.
    BOOL enabled = 0;
    if (FAILED(DwmIsCompositionEnabled(&enabled)) || !enabled)
        return false;

    // This works at least on Windows 8.1: it returns an error in fullscreen,
    // which is also when we get consistent timings without DwmFlush. Might
    // be cargo-cult.
    DWM_TIMING_INFO info = { .cbSize = sizeof(DWM_TIMING_INFO) };
    if (FAILED(DwmGetCompositionTimingInfo(0, &info)))
        return false;

    return true;
}

static void wgl_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    SwapBuffers(p->hdc);

    // default if we don't DwmFLush
    int new_swapinterval = p->opt_swapinterval;

    int dwm_flush_opt;
    mp_read_option_raw(ctx->global, "opengl-dwmflush", &m_option_type_choice,
                       &dwm_flush_opt);

    if (dwm_flush_opt >= 0) {
        if ((dwm_flush_opt == 1 && !ctx->vo->opts->fullscreen) ||
            (dwm_flush_opt == 2) ||
            (dwm_flush_opt == 0 && compositor_active(ctx)))
        {
            if (DwmFlush() == S_OK)
                new_swapinterval = 0;
        }
    }

    if (new_swapinterval != p->current_swapinterval &&
        p->real_wglSwapInterval)
    {
        p->real_wglSwapInterval(new_swapinterval);
        MP_VERBOSE(ctx->vo, "set SwapInterval(%d)\n", new_swapinterval);
    }
    p->current_swapinterval = new_swapinterval;
}

static bool wgl_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);
    GL *gl = &p->gl;

    if (!vo_w32_init(ctx->vo))
        goto fail;

    vo_w32_run_on_thread(ctx->vo, create_ctx, ctx);
    if (!p->context)
        goto fail;

    current_wgl_context = p;
    wglMakeCurrent(p->hdc, p->context);

    mpgl_load_functions(gl, wglgpa, NULL, ctx->vo->log);

    if (!gl->SwapInterval)
        MP_VERBOSE(ctx->vo, "WGL_EXT_swap_control missing.\n");
    p->real_wglSwapInterval = gl->SwapInterval;
    gl->SwapInterval = wgl_swap_interval;
    p->current_swapinterval = -1;

    struct ra_gl_ctx_params params = {
        .swap_buffers = wgl_swap_buffers,
    };

    if (!ra_gl_ctx_init(ctx, gl, params))
        goto fail;

    DwmEnableMMCSS(TRUE);
    return true;

fail:
    wgl_uninit(ctx);
    return false;
}

static void resize(struct ra_ctx *ctx)
{
    ra_gl_ctx_resize(ctx->swapchain, ctx->vo->dwidth, ctx->vo->dheight, 0);
}

static bool wgl_reconfig(struct ra_ctx *ctx)
{
    vo_w32_config(ctx->vo);
    resize(ctx);
    return true;
}

static void destroy_gl(void *ptr)
{
    struct ra_ctx *ctx = ptr;
    struct priv *p = ctx->priv;
    if (p->context)
        wglDeleteContext(p->context);
    p->context = 0;
    if (p->hdc)
        ReleaseDC(vo_w32_hwnd(ctx->vo), p->hdc);
    p->hdc = NULL;
    current_wgl_context = NULL;
}

static void wgl_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);
    if (p->context)
        wglMakeCurrent(p->hdc, 0);
    vo_w32_run_on_thread(ctx->vo, destroy_gl, ctx);

    DwmEnableMMCSS(FALSE);
    vo_w32_uninit(ctx->vo);
}

static int wgl_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    int ret = vo_w32_control(ctx->vo, events, request, arg);
    if (*events & VO_EVENT_RESIZE)
        resize(ctx);
    return ret;
}

const struct ra_ctx_fns ra_ctx_wgl = {
    .type           = "opengl",
    .name           = "win",
    .init           = wgl_init,
    .reconfig       = wgl_reconfig,
    .control        = wgl_control,
    .uninit         = wgl_uninit,
};
