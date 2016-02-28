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

#include "common/common.h"
#include "video/out/w32_common.h"
#include "context.h"

struct priv {
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
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

    eglBindAPI(EGL_OPENGL_ES_API);
    if (eglGetError() != EGL_SUCCESS) {
        MP_FATAL(vo, "Couldn't bind GLES API\n");
        goto fail;
    }

    EGLConfig config = select_fb_config_egl(ctx);
    if (!config)
        goto fail;

    p->egl_surface = eglCreateWindowSurface(p->egl_display, config,
                                            vo_w32_hwnd(vo), NULL);
    if (p->egl_surface == EGL_NO_SURFACE) {
        MP_FATAL(ctx->vo, "Could not create EGL surface!\n");
        goto fail;
    }

    if (!create_context_egl(ctx, config, 3) &&
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
