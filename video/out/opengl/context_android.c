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

#include <dlfcn.h>
#include <stdint.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "video/out/android_common.h"
#include "video/mp_image.h"
#include "egl_helpers.h"
#include "common/common.h"
#include "context.h"

// Values from <android/data_space.h>, spelled out because
// ANativeWindow_setBuffersDataSpace() needs API 28 and mpv-android's
// minSdk is lower, so the function is looked up at runtime instead.
#define MP_ADATASPACE_SRGB       142671872 // BT709  | TRANSFER_SRGB   | RANGE_FULL
#define MP_ADATASPACE_BT2020_PQ  163971072 // BT2020 | TRANSFER_ST2084 | RANGE_FULL
#define MP_ADATASPACE_BT2020_HLG 168165376 // BT2020 | TRANSFER_HLG    | RANGE_FULL

typedef int32_t (*set_dataspace_fn)(ANativeWindow *window, int32_t dataspace);

struct priv {
    struct GL gl;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    set_dataspace_fn set_dataspace;
    int32_t dataspace;
};

// The function lives in libnativewindow.so (API 28+). mpv only links
// libandroid.so, and linker namespaces hide libnativewindow from
// dlsym(RTLD_DEFAULT), so open it explicitly.
static set_dataspace_fn load_set_dataspace(void)
{
    static const char *const libs[] = { "libnativewindow.so", "libandroid.so" };
    for (size_t i = 0; i < MP_ARRAY_SIZE(libs); i++) {
        void *h = dlopen(libs[i], RTLD_NOW | RTLD_LOCAL);
        if (!h)
            continue;
        void *fn = dlsym(h, "ANativeWindow_setBuffersDataSpace");
        if (fn)
            return (set_dataspace_fn)fn; // keep the library loaded
        dlclose(h);
    }
    return (set_dataspace_fn)dlsym(RTLD_DEFAULT, "ANativeWindow_setBuffersDataSpace");
}

// Tag the output buffers with an HDR dataspace so SurfaceFlinger/HWC treat
// the window as HDR (same as MediaCodec output). Without this every frame is
// treated as sRGB and mpv tone-maps HDR down to SDR.
static bool android_set_color(struct ra_ctx *ctx, struct mp_image_params *params)
{
    struct priv *p = ctx->priv;
    ANativeWindow *win = vo_android_native_window(ctx->vo);
    if (!p->set_dataspace || !win)
        return false;

    int32_t ds = MP_ADATASPACE_SRGB;
    if (params) {
        switch (params->color.transfer) {
        case PL_COLOR_TRC_PQ:  ds = MP_ADATASPACE_BT2020_PQ;  break;
        case PL_COLOR_TRC_HLG: ds = MP_ADATASPACE_BT2020_HLG; break;
        default: params = NULL; break; // not HDR: stay sRGB, let mpv tone-map
        }
    }

    if (ds != p->dataspace) {
        if (p->set_dataspace(win, ds) != 0) {
            MP_WARN(ctx, "HDR output: failed to set dataspace 0x%x\n", (unsigned)ds);
            return false;
        }
        p->dataspace = ds;
        MP_INFO(ctx, "HDR output: dataspace set to 0x%x\n", (unsigned)ds);
    }

    if (!params)
        return false;
    params->color.primaries = PL_COLOR_PRIM_BT_2020;
    return true;
}

static void android_swap_buffers(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    eglSwapBuffers(p->egl_display, p->egl_surface);
}

static void android_uninit(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    ra_gl_ctx_uninit(ctx);

    if (p->egl_surface) {
        eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroySurface(p->egl_display, p->egl_surface);
    }
    if (p->egl_context)
        eglDestroyContext(p->egl_display, p->egl_context);

    vo_android_uninit(ctx->vo);
}

static bool android_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);

    if (!vo_android_init(ctx->vo))
        goto fail;

    p->dataspace = -1;
    p->set_dataspace = load_set_dataspace();
    if (p->set_dataspace) {
        MP_INFO(ctx, "HDR output: ANativeWindow_setBuffersDataSpace available\n");
    } else {
        MP_WARN(ctx, "HDR output: ANativeWindow_setBuffersDataSpace not found (%s)\n",
                dlerror());
    }

    p->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        MP_FATAL(ctx, "EGL failed to initialize.\n");
        goto fail;
    }

    EGLConfig config;
    if (!mpegl_create_context(ctx, p->egl_display, &p->egl_context, &config))
        goto fail;

    ANativeWindow *native_window = vo_android_native_window(ctx->vo);
    EGLint format;
    eglGetConfigAttrib(p->egl_display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(native_window, 0, 0, format);

    p->egl_surface = eglCreateWindowSurface(p->egl_display, config,
                                    (EGLNativeWindowType)native_window, NULL);

    if (p->egl_surface == EGL_NO_SURFACE) {
        MP_FATAL(ctx, "Could not create EGL surface!\n");
        goto fail;
    }

    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context)) {
        MP_FATAL(ctx, "Failed to set context!\n");
        goto fail;
    }

    mpegl_load_functions(&p->gl, ctx->log);

    struct ra_ctx_params params = {
        .swap_buffers = android_swap_buffers,
        .set_color    = android_set_color,
    };

    if (!ra_gl_ctx_init(ctx, &p->gl, params))
        goto fail;

    return true;
fail:
    android_uninit(ctx);
    return false;
}

static bool android_reconfig(struct ra_ctx *ctx)
{
    int w, h;
    if (!vo_android_surface_size(ctx->vo, &w, &h))
        return false;

    // Update window geometry to prevent screen tearing
    ANativeWindow *native_window = vo_android_native_window(ctx->vo);
    if (native_window) {
        int32_t current_format = ANativeWindow_getFormat(native_window);
        ANativeWindow_setBuffersGeometry(native_window, w, h, current_format);
    }

    // The window may be new; re-apply the dataspace on the next frame.
    struct priv *p = ctx->priv;
    p->dataspace = -1;

    ctx->vo->dwidth = w;
    ctx->vo->dheight = h;
    ra_gl_ctx_resize(ctx->swapchain, w, h, 0);
    return true;
}

static int android_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    return VO_NOTIMPL;
}

const struct ra_ctx_fns ra_ctx_android = {
    .type           = "opengl",
    .name           = "android",
    .description    = "Android/EGL",
    .reconfig       = android_reconfig,
    .control        = android_control,
    .init           = android_init,
    .uninit         = android_uninit,
};
