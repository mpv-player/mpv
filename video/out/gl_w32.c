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

#include <windows.h>
#include "w32_common.h"
#include "gl_common.h"

struct w32_context {
    HGLRC context;
};

static void *w32gpa(const GLubyte *procName)
{
    HMODULE oglmod;
    void *res = wglGetProcAddress(procName);
    if (res)
        return res;
    oglmod = GetModuleHandle("opengl32.dll");
    return GetProcAddress(oglmod, procName);
}

static bool create_context_w32_old(struct MPGLContext *ctx)
{
    GL *gl = ctx->gl;

    struct w32_context *w32_ctx = ctx->priv;
    HGLRC *context = &w32_ctx->context;

    if (*context) {
        gl->Finish();   // supposedly to prevent flickering
        return true;
    }

    HWND win = ctx->vo->w32->window;
    HDC windc = GetDC(win);
    bool res = false;

    HGLRC new_context = wglCreateContext(windc);
    if (!new_context) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create GL context!\n");
        goto out;
    }

    if (!wglMakeCurrent(windc, new_context)) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GL context!\n");
        wglDeleteContext(new_context);
        goto out;
    }

    *context = new_context;

    mpgl_load_functions(ctx->gl, w32gpa, NULL);
    res = true;

out:
    ReleaseDC(win, windc);
    return res;
}

static bool create_context_w32_gl3(struct MPGLContext *ctx)
{
    struct w32_context *w32_ctx = ctx->priv;
    HGLRC *context = &w32_ctx->context;

    if (*context) // reuse existing context
        return true; // not reusing it breaks gl3!

    HWND win = ctx->vo->w32->window;
    HDC windc = GetDC(win);
    HGLRC new_context = 0;

    new_context = wglCreateContext(windc);
    if (!new_context) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create GL context!\n");
        return false;
    }

    // set context
    if (!wglMakeCurrent(windc, new_context)) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GL context!\n");
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

    int gl_version = ctx->requested_gl_version;
    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, MPGL_VER_GET_MAJOR(gl_version),
        WGL_CONTEXT_MINOR_VERSION_ARB, MPGL_VER_GET_MINOR(gl_version),
        WGL_CONTEXT_FLAGS_ARB, 0,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    *context = wglCreateContextAttribsARB(windc, 0, attribs);
    if (! *context) {
        // NVidia, instead of ignoring WGL_CONTEXT_FLAGS_ARB, will error out if
        // it's present on pre-3.2 contexts.
        // Remove it from attribs and retry the context creation.
        attribs[6] = attribs[7] = 0;
        *context = wglCreateContextAttribsARB(windc, 0, attribs);
    }
    if (! *context) {
        int err = GetLastError();
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not create an OpenGL 3.x"
                                    " context: error 0x%x\n", err);
        goto out;
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(new_context);

    if (!wglMakeCurrent(windc, *context)) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Could not set GL3 context!\n");
        wglDeleteContext(*context);
        return false;
    }

    /* update function pointers */
    mpgl_load_functions(ctx->gl, w32gpa, NULL);

    int pfmt = GetPixelFormat(windc);
    PIXELFORMATDESCRIPTOR pfd;
    if (DescribePixelFormat(windc, pfmt, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
        ctx->depth_r = pfd.cRedBits;
        ctx->depth_g = pfd.cGreenBits;
        ctx->depth_b = pfd.cBlueBits;
    }

    return true;

unsupported:
    mp_msg(MSGT_VO, MSGL_ERR, "[gl] The current OpenGL implementation does"
                              " not support OpenGL 3.x \n");
out:
    wglDeleteContext(new_context);
    return false;
}

static bool config_window_w32(struct MPGLContext *ctx, uint32_t d_width,
                              uint32_t d_height, uint32_t flags)
{
    if (!vo_w32_config(ctx->vo, d_width, d_height, flags))
        return false;

    bool success = false;
    if (ctx->requested_gl_version >= MPGL_VER(3, 0))
        success = create_context_w32_gl3(ctx);
    if (!success)
        success = create_context_w32_old(ctx);
    return success;
}

static void releaseGlContext_w32(MPGLContext *ctx)
{
    struct w32_context *w32_ctx = ctx->priv;
    HGLRC *context = &w32_ctx->context;
    if (*context) {
        wglMakeCurrent(0, 0);
        wglDeleteContext(*context);
    }
    *context = 0;
}

static void swapGlBuffers_w32(MPGLContext *ctx)
{
    HDC vo_hdc = GetDC(ctx->vo->w32->window);
    SwapBuffers(vo_hdc);
    ReleaseDC(ctx->vo->w32->window, vo_hdc);
}

void mpgl_set_backend_w32(MPGLContext *ctx)
{
    ctx->priv = talloc_zero(ctx, struct w32_context);
    ctx->config_window = config_window_w32;
    ctx->releaseGlContext = releaseGlContext_w32;
    ctx->swapGlBuffers = swapGlBuffers_w32;
    ctx->update_xinerama_info = w32_update_xinerama_info;
    ctx->border = vo_w32_border;
    ctx->check_events = vo_w32_check_events;
    ctx->fullscreen = vo_w32_fullscreen;
    ctx->ontop = vo_w32_ontop;
    ctx->vo_init = vo_w32_init;
    ctx->vo_uninit = vo_w32_uninit;
}
