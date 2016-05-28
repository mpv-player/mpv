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

#include <windows.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "angle_dynamic.h"

#include "common/common.h"
#include "video/out/w32_common.h"
#include "context.h"

#ifndef EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE
#define EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE 0x33A7
#define EGL_SURFACE_ORIENTATION_ANGLE 0x33A8
#define EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE 0x0002
#endif

struct priv {
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    bool use_es2;
};

static void angle_uninit(MPGLContext *ctx)
{
    struct priv *p = ctx->priv;

    if (p->egl_context) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroyContext(p->egl_display, p->egl_context);
    }
    p->egl_context = EGL_NO_CONTEXT;
    if (p->egl_display)
        eglTerminate(p->egl_display);
    vo_w32_uninit(ctx->vo);
}

static EGLConfig select_fb_config_egl(struct MPGLContext *ctx)
{
    struct priv *p = ctx->priv;

    EGLint attributes[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_NONE
    };

    EGLint config_count;
    EGLConfig config;

    eglChooseConfig(p->egl_display, attributes, &config, 1, &config_count);

    if (!config_count) {
        MP_FATAL(ctx->vo, "Could find EGL configuration!\n");
        return NULL;
    }

    return config;
}

static bool create_context_egl(MPGLContext *ctx, EGLConfig config, int version)
{
    struct priv *p = ctx->priv;

    EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, version,
        EGL_NONE
    };

    p->egl_context = eglCreateContext(p->egl_display, config,
                                      EGL_NO_CONTEXT, context_attributes);

    if (p->egl_context == EGL_NO_CONTEXT) {
        MP_VERBOSE(ctx->vo, "Could not create EGL GLES %d context!\n", version);
        return false;
    }

    eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                   p->egl_context);

    return true;
}

static void *get_proc_address(const GLubyte *proc_name)
{
    return eglGetProcAddress(proc_name);
}

static int angle_init(struct MPGLContext *ctx, int flags)
{
    struct priv *p = ctx->priv;
    struct vo *vo = ctx->vo;

    if (!angle_load()) {
        MP_VERBOSE(vo, "Failed to load LIBEGL.DLL\n");
        goto fail;
    }

    if (!vo_w32_init(vo))
        goto fail;

    HDC dc = GetDC(vo_w32_hwnd(vo));
    if (!dc) {
        MP_FATAL(vo, "Couldn't get DC\n");
        goto fail;
    }

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (!eglGetPlatformDisplayEXT) {
        MP_FATAL(vo, "Missing EGL_EXT_platform_base\n");
        goto fail;
    }

    EGLint d3d_types[] = {EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                          EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE};
    for (int i = 0; i < MP_ARRAY_SIZE(d3d_types); i++) {
        EGLint display_attributes[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                d3d_types[i],
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
            EGL_NONE,
        };

        p->egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, dc,
            display_attributes);
        if (p->egl_display != EGL_NO_DISPLAY)
            break;
    }
    if (p->egl_display == EGL_NO_DISPLAY) {
        MP_FATAL(vo, "Couldn't get display\n");
        goto fail;
    }

    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        MP_FATAL(vo, "Couldn't initialize EGL\n");
        goto fail;
    }

    const char *exts = eglQueryString(p->egl_display, EGL_EXTENSIONS);
    if (exts)
        MP_DBG(ctx->vo, "EGL extensions: %s\n", exts);

    eglBindAPI(EGL_OPENGL_ES_API);
    if (eglGetError() != EGL_SUCCESS) {
        MP_FATAL(vo, "Couldn't bind GLES API\n");
        goto fail;
    }

    EGLConfig config = select_fb_config_egl(ctx);
    if (!config)
        goto fail;

    int window_attribs_len = 0;
    EGLint *window_attribs = NULL;

    EGLint flip_val;
    if (eglGetConfigAttrib(p->egl_display, config,
                           EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE, &flip_val))
    {
        if (flip_val == EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE) {
            MP_TARRAY_APPEND(NULL, window_attribs, window_attribs_len,
                EGL_SURFACE_ORIENTATION_ANGLE);
            MP_TARRAY_APPEND(NULL, window_attribs, window_attribs_len,
                EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE);
            ctx->flip_v = true;
            MP_VERBOSE(vo, "Rendering flipped.\n");
        }
    }

    // EGL_DIRECT_COMPOSITION_ANGLE enables the use of flip-mode present, which
    // avoids a copy of the video image and lowers vsync jitter, though the
    // extension is only present on Windows 8 and up.
    if (strstr(exts, "EGL_ANGLE_direct_composition")) {
        MP_TARRAY_APPEND(NULL, window_attribs, window_attribs_len,
            EGL_DIRECT_COMPOSITION_ANGLE);
        MP_TARRAY_APPEND(NULL, window_attribs, window_attribs_len, EGL_TRUE);
        MP_VERBOSE(vo, "Using DirectComposition.\n");
    }

    MP_TARRAY_APPEND(NULL, window_attribs, window_attribs_len, EGL_NONE);
    p->egl_surface = eglCreateWindowSurface(p->egl_display, config,
                                            vo_w32_hwnd(vo), window_attribs);
    talloc_free(window_attribs);
    if (p->egl_surface == EGL_NO_SURFACE) {
        MP_FATAL(ctx->vo, "Could not create EGL surface!\n");
        goto fail;
    }

    if (!(!p->use_es2 && create_context_egl(ctx, config, 3)) &&
        !create_context_egl(ctx, config, 2))
    {
        MP_FATAL(ctx->vo, "Could not create EGL context!\n");
        goto fail;
    }

    mpgl_load_functions(ctx->gl, get_proc_address, NULL, vo->log);
    return 0;

fail:
    angle_uninit(ctx);
    return -1;
}

static int angle_init_es2(struct MPGLContext *ctx, int flags)
{
    struct priv *p = ctx->priv;
    p->use_es2 = true;
    if (ctx->vo->probing) {
        MP_VERBOSE(ctx->vo, "Not using this by default.\n");
        return -1;
    }
    return angle_init(ctx, flags);
}

static int angle_reconfig(struct MPGLContext *ctx)
{
    vo_w32_config(ctx->vo);
    return 0;
}

static int angle_control(MPGLContext *ctx, int *events, int request, void *arg)
{
    return vo_w32_control(ctx->vo, events, request, arg);
}

static void angle_swap_buffers(MPGLContext *ctx)
{
    struct priv *p = ctx->priv;
    eglSwapBuffers(p->egl_display, p->egl_surface);
}

const struct mpgl_driver mpgl_driver_angle = {
    .name           = "angle",
    .priv_size      = sizeof(struct priv),
    .init           = angle_init,
    .reconfig       = angle_reconfig,
    .swap_buffers   = angle_swap_buffers,
    .control        = angle_control,
    .uninit         = angle_uninit,
};

const struct mpgl_driver mpgl_driver_angle_es2 = {
    .name           = "angle-es2",
    .priv_size      = sizeof(struct priv),
    .init           = angle_init_es2,
    .reconfig       = angle_reconfig,
    .swap_buffers   = angle_swap_buffers,
    .control        = angle_control,
    .uninit         = angle_uninit,
};
