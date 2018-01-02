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

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <libavcodec/jni.h>
#include <android/native_window_jni.h>

#include "egl_helpers.h"

#include "common/common.h"
#include "options/m_config.h"
#include "context.h"

struct android_opts {
    struct m_geometry surface_size;
};

#define OPT_BASE_STRUCT struct android_opts
const struct m_sub_options android_conf = {
    .opts = (const struct m_option[]) {
        OPT_SIZE_BOX("android-surface-size", surface_size, UPDATE_VO_RESIZE),
        {0}
    },
    .size = sizeof(struct android_opts),
};

struct priv {
    struct GL gl;
    EGLDisplay egl_display;
    EGLConfig egl_config;
    EGLContext egl_context;
    EGLSurface egl_surface;
    ANativeWindow *native_window;
};

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

    if (p->native_window) {
        ANativeWindow_release(p->native_window);
        p->native_window = NULL;
    }
}

static bool android_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);

    jobject surface = (jobject)(intptr_t)ctx->vo->opts->WinID;
    JavaVM *vm = (JavaVM *)av_jni_get_java_vm(NULL);
    JNIEnv *env;
    int ret = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if (ret == JNI_EDETACHED) {
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != 0) {
            MP_FATAL(ctx, "Could not attach java VM.\n");
            goto fail;
        }
    }
    p->native_window = ANativeWindow_fromSurface(env, surface);
    (*vm)->DetachCurrentThread(vm);

    p->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        MP_FATAL(ctx, "EGL failed to initialize.\n");
        goto fail;
    }

    EGLConfig config;
    if (!mpegl_create_context(ctx, p->egl_display, &p->egl_context, &config))
        goto fail;

    EGLint format;
    eglGetConfigAttrib(p->egl_display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(p->native_window, 0, 0, format);

    p->egl_surface = eglCreateWindowSurface(p->egl_display, config,
                                    (EGLNativeWindowType)p->native_window, NULL);

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

    struct ra_gl_ctx_params params = {
        .swap_buffers = android_swap_buffers,
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
    struct priv *p = ctx->priv;
    void *tmp = talloc_new(NULL);
    struct android_opts *opts = mp_get_config_group(tmp, ctx->global, &android_conf);
    int w = opts->surface_size.w, h = opts->surface_size.h;

    if (!w)
        eglQuerySurface(p->egl_display, p->egl_surface, EGL_WIDTH, &w);
    if (!h)
        eglQuerySurface(p->egl_display, p->egl_surface, EGL_HEIGHT, &h);

    if (!w || !h) {
        MP_FATAL(ctx, "Failed to get height and width!\n");
        return false;
    }

    ctx->vo->dwidth = w;
    ctx->vo->dheight = h;
    ra_gl_ctx_resize(ctx->swapchain, w, h, 0);

    talloc_free(tmp);
    return true;
}

static int android_control(struct ra_ctx *ctx, int *events, int request, void *arg)
{
    return VO_NOTIMPL;
}

const struct ra_ctx_fns ra_ctx_android = {
    .type           = "opengl",
    .name           = "android",
    .reconfig       = android_reconfig,
    .control        = android_control,
    .init           = android_init,
    .uninit         = android_uninit,
};
