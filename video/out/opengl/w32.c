/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <assert.h>
#include <windows.h>
#include <dwmapi.h>
#include "video/out/w32_common.h"
#include "common.h"

struct w32_context {
    int opt_swapinterval;
    int current_swapinterval;

    int (GLAPIENTRY *real_wglSwapInterval)(int);

    HGLRC context;
    HDC hdc;
    int flags;

    HINSTANCE dwmapi_dll;
    HRESULT (WINAPI *pDwmFlush)(void);
    HRESULT (WINAPI *pDwmIsCompositionEnabled)(BOOL *pfEnabled);
    HRESULT (WINAPI *pDwmEnableMMCSS)(BOOL fEnableMMCSS);
    HRESULT (WINAPI *pDwmGetCompositionTimingInfo)
                            (HWND hwnd, DWM_TIMING_INFO *pTimingInfo);
};

static void w32_uninit(MPGLContext *ctx);

static __thread struct w32_context *current_w32_context;

static int GLAPIENTRY w32_swap_interval(int interval)
{
    if (current_w32_context)
        current_w32_context->opt_swapinterval = interval;
    return 0;
}

static bool create_dc(struct MPGLContext *ctx, int flags)
{
    struct w32_context *w32_ctx = ctx->priv;
    HWND win = vo_w32_hwnd(ctx->vo);

    if (w32_ctx->hdc)
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

    int pfmt = GetPixelFormat(hdc);
    if (DescribePixelFormat(hdc, pfmt, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
        ctx->depth_r = pfd.cRedBits;
        ctx->depth_g = pfd.cGreenBits;
        ctx->depth_b = pfd.cBlueBits;
    }

    w32_ctx->hdc = hdc;
    return true;
}

static void *w32gpa(const GLubyte *procName)
{
    HMODULE oglmod;
    void *res = wglGetProcAddress(procName);
    if (res)
        return res;
    oglmod = GetModuleHandle(L"opengl32.dll");
    return GetProcAddress(oglmod, procName);
}

static bool create_context_w32_old(struct MPGLContext *ctx)
{
    struct w32_context *w32_ctx = ctx->priv;

    HDC windc = w32_ctx->hdc;
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

    w32_ctx->context = context;

    mpgl_load_functions(ctx->gl, w32gpa, NULL, ctx->vo->log);
    return true;
}

static bool create_context_w32_gl3(struct MPGLContext *ctx)
{
    struct w32_context *w32_ctx = ctx->priv;

    HDC windc = w32_ctx->hdc;
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
        = w32gpa((const GLubyte*)"wglGetExtensionsStringARB");

    if (!wglGetExtensionsStringARB)
        goto unsupported;

    const char *wgl_exts = wglGetExtensionsStringARB(windc);
    if (!strstr(wgl_exts, "WGL_ARB_create_context"))
        goto unsupported;

    HGLRC (GLAPIENTRY *wglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext,
                                                   const int *attribList)
        = w32gpa((const GLubyte*)"wglCreateContextAttribsARB");

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

    w32_ctx->context = context;

    /* update function pointers */
    mpgl_load_functions(ctx->gl, w32gpa, NULL, ctx->vo->log);

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
    struct MPGLContext *ctx = ptr;
    struct w32_context *w32_ctx = ctx->priv;

    if (!create_dc(ctx, w32_ctx->flags))
        return;

    create_context_w32_gl3(ctx);
    if (!w32_ctx->context)
        create_context_w32_old(ctx);

    w32_ctx->dwmapi_dll = LoadLibrary(L"Dwmapi.dll");
    if (w32_ctx->dwmapi_dll) {
        w32_ctx->pDwmFlush = (void *)GetProcAddress(w32_ctx->dwmapi_dll, "DwmFlush");
        w32_ctx->pDwmIsCompositionEnabled =
            (void *)GetProcAddress(w32_ctx->dwmapi_dll, "DwmIsCompositionEnabled");
        w32_ctx->pDwmGetCompositionTimingInfo =
            (void *)GetProcAddress(w32_ctx->dwmapi_dll, "DwmGetCompositionTimingInfo");
        w32_ctx->pDwmEnableMMCSS =
            (void *)GetProcAddress(w32_ctx->dwmapi_dll, "DwmEnableMMCSS");
    }

    wglMakeCurrent(w32_ctx->hdc, NULL);
}

static int w32_init(struct MPGLContext *ctx, int flags)
{
    if (!vo_w32_init(ctx->vo))
        goto fail;

    struct w32_context *w32_ctx = ctx->priv;

    w32_ctx->flags = flags;
    vo_w32_run_on_thread(ctx->vo, create_ctx, ctx);

    if (!w32_ctx->context)
        goto fail;

    if (!ctx->gl->SwapInterval)
        MP_VERBOSE(ctx->vo, "WGL_EXT_swap_control missing.");
    w32_ctx->real_wglSwapInterval = ctx->gl->SwapInterval;
    ctx->gl->SwapInterval = w32_swap_interval;
    w32_ctx->current_swapinterval = -1;

    current_w32_context = w32_ctx;
    wglMakeCurrent(w32_ctx->hdc, w32_ctx->context);
    if (w32_ctx->pDwmEnableMMCSS)
        w32_ctx->pDwmEnableMMCSS(TRUE);
    return 0;

fail:
    w32_uninit(ctx);
    return -1;
}

static int w32_reconfig(struct MPGLContext *ctx)
{
    vo_w32_config(ctx->vo);
    return 0;
}

static void destroy_gl(void *ptr)
{
    struct MPGLContext *ctx = ptr;
    struct w32_context *w32_ctx = ctx->priv;
    if (w32_ctx->context)
        wglDeleteContext(w32_ctx->context);
    w32_ctx->context = 0;
    if (w32_ctx->hdc)
        ReleaseDC(vo_w32_hwnd(ctx->vo), w32_ctx->hdc);
    w32_ctx->hdc = NULL;
    current_w32_context = NULL;
}

static void w32_uninit(MPGLContext *ctx)
{
    struct w32_context *w32_ctx = ctx->priv;
    if (w32_ctx->context)
        wglMakeCurrent(w32_ctx->hdc, 0);
    vo_w32_run_on_thread(ctx->vo, destroy_gl, ctx);

    if (w32_ctx->pDwmEnableMMCSS)
        w32_ctx->pDwmEnableMMCSS(FALSE);

    if (w32_ctx->dwmapi_dll)
        FreeLibrary(w32_ctx->dwmapi_dll);
    w32_ctx->dwmapi_dll = NULL;
    vo_w32_uninit(ctx->vo);
}

static bool compositor_active(MPGLContext *ctx)
{
    struct w32_context *w32_ctx = ctx->priv;

    if (!w32_ctx->pDwmIsCompositionEnabled || !w32_ctx->pDwmGetCompositionTimingInfo)
        return false;

    // For Windows 7.
    BOOL enabled = 0;
    if (FAILED(w32_ctx->pDwmIsCompositionEnabled(&enabled)) || !enabled)
        return false;

    // This works at least on Windows 8.1: it returns an error in fullscreen,
    // which is also when we get consistent timings without DwmFlush. Might
    // be cargo-cult.
    DWM_TIMING_INFO info = { .cbSize = sizeof(DWM_TIMING_INFO) };
    if (FAILED(w32_ctx->pDwmGetCompositionTimingInfo(0, &info)))
        return false;

    return true;
}

static void w32_swap_buffers(MPGLContext *ctx)
{
    struct w32_context *w32_ctx = ctx->priv;
    SwapBuffers(w32_ctx->hdc);

    // default if we don't DwmFLush
    int new_swapinterval = w32_ctx->opt_swapinterval;

    if (ctx->dwm_flush_opt >= 0) {
        if ((ctx->dwm_flush_opt == 1 && !ctx->vo->opts->fullscreen) ||
            (ctx->dwm_flush_opt == 2) ||
            (ctx->dwm_flush_opt == 0 && compositor_active(ctx)))
        {
            if (w32_ctx->pDwmFlush && w32_ctx->pDwmFlush() == S_OK)
                new_swapinterval = 0;
        }
    }

    if (new_swapinterval != w32_ctx->current_swapinterval &&
        w32_ctx->real_wglSwapInterval)
    {
        w32_ctx->real_wglSwapInterval(new_swapinterval);
        MP_VERBOSE(ctx->vo, "set SwapInterval(%d)\n", new_swapinterval);
    }
    w32_ctx->current_swapinterval = new_swapinterval;
}

static int w32_control(MPGLContext *ctx, int *events, int request, void *arg)
{
    return vo_w32_control(ctx->vo, events, request, arg);
}

const struct mpgl_driver mpgl_driver_w32 = {
    .name           = "w32",
    .priv_size      = sizeof(struct w32_context),
    .init           = w32_init,
    .reconfig       = w32_reconfig,
    .swap_buffers   = w32_swap_buffers,
    .control        = w32_control,
    .uninit         = w32_uninit,
};
