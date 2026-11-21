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

#include "video/out/android_common.h"
#include "egl_helpers.h"
#include "common/common.h"
#include "misc/jni.h"
#include "context.h"

// android.view.Display.HdrCapabilities
#define MP_HDR_TYPE_DOLBY_VISION           1
#define MP_HDR_TYPE_HDR10                  2
#define MP_HDR_TYPE_HLG                    3
#define MP_HDR_TYPE_HDR10_PLUS             4
#define MP_HDR_TYPE_HLG_PLUS               6

struct priv {
    struct GL gl;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    EGLConfig egl_config;
    bool csp_queried;
    pl_color_space_t preferred_csp;
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

    vo_android_uninit(ctx->vo);
}

static bool android_recreate_surface(struct ra_ctx *ctx, const int32_t *attribs)
{
    struct priv *p = ctx->priv;
    ANativeWindow *win = vo_android_native_window(ctx->vo);
    if (!win)
        return false;

    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    if (p->egl_surface != EGL_NO_SURFACE)
        eglDestroySurface(p->egl_display, p->egl_surface);

    bool ok = true;
    EGLSurface surf = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                             (EGLNativeWindowType)win,
                                             (const EGLint *)attribs);
    if (surf == EGL_NO_SURFACE && attribs) {
        MP_WARN(ctx, "Could not apply EGL surface attributes, trying defaults.\n");
        surf = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                      (EGLNativeWindowType)win, NULL);
        ok = false;
    }

    p->egl_surface = surf;
    if (surf == EGL_NO_SURFACE) {
        MP_ERR(ctx, "Could not recreate the EGL surface.\n");
        return false;
    }

    if (!eglMakeCurrent(p->egl_display, surf, surf, p->egl_context)) {
        MP_ERR(ctx, "Failed to make the EGL surface current.\n");
        return false;
    }

    return ok;
}

// Ask the Java framework about the HDR capabilities of the default display.
// There is no NDK API for this, so this goes through DisplayManagerGlobal.
static void android_query_display_csp(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    JNIEnv *env = mp_jni_get_env(ctx->log);
    jclass cls_dmg = NULL, cls_display = NULL, cls_caps = NULL;
    jclass cls_mode = NULL;
    jobject dmg = NULL, display = NULL, caps = NULL, mode = NULL;
    jmethodID m_get_instance, m_get_display, m_get_caps, m_get_mode;
    jmethodID m_mode_get_types = NULL, m_caps_get_types = NULL;
    jmethodID m_get_max_luma, m_get_avg_luma, m_get_min_luma;
    jintArray types = NULL;
    jint *elems = NULL;
    jsize nb_types = 0;
    bool pq = false, hlg = false;
    jfloat max_luma = -1, avg_luma = -1, min_luma = -1;
    char types_str[64] = "";
    int i;

    p->csp_queried = true;

    if (!env)
        return;
    if ((*env)->PushLocalFrame(env, 16) < 0) {
        mp_jni_exception_check(env, 1, ctx->log);
        return;
    }

    cls_dmg = (*env)->FindClass(env,
        "android/hardware/display/DisplayManagerGlobal");
    cls_display = (*env)->FindClass(env, "android/view/Display");
    cls_caps = (*env)->FindClass(env,
        "android/view/Display$HdrCapabilities");
    if (!cls_dmg || !cls_display || !cls_caps)
        goto done;

    m_get_instance = (*env)->GetStaticMethodID(env, cls_dmg, "getInstance",
        "()Landroid/hardware/display/DisplayManagerGlobal;");
    m_get_display = (*env)->GetMethodID(env, cls_dmg, "getRealDisplay",
        "(I)Landroid/view/Display;");
    m_get_caps = (*env)->GetMethodID(env, cls_display,
        "getHdrCapabilities", "()Landroid/view/Display$HdrCapabilities;");
    m_get_mode = (*env)->GetMethodID(env, cls_display, "getMode",
        "()Landroid/view/Display$Mode;");
    m_get_max_luma = (*env)->GetMethodID(env, cls_caps,
        "getDesiredMaxLuminance", "()F");
    m_get_avg_luma = (*env)->GetMethodID(env, cls_caps,
        "getDesiredMaxAverageLuminance", "()F");
    m_get_min_luma = (*env)->GetMethodID(env, cls_caps,
        "getDesiredMinLuminance", "()F");
    if (!m_get_instance || !m_get_display || !m_get_caps || !m_get_mode ||
        !m_get_max_luma || !m_get_avg_luma || !m_get_min_luma)
        goto done;

    // Display.Mode.getSupportedHdrTypes() replaced the deprecated
    // HdrCapabilities.getSupportedHdrTypes() in API 34.
    cls_mode = (*env)->FindClass(env, "android/view/Display$Mode");
    if (mp_jni_exception_check(env, 0, NULL) < 0)
        cls_mode = NULL;
    m_mode_get_types = cls_mode ? (*env)->GetMethodID(env, cls_mode,
        "getSupportedHdrTypes", "()[I") : NULL;
    if (mp_jni_exception_check(env, 0, NULL) < 0)
        m_mode_get_types = NULL;
    if (!m_mode_get_types) {
        m_caps_get_types = (*env)->GetMethodID(env, cls_caps,
            "getSupportedHdrTypes", "()[I");
        if (!m_caps_get_types)
            goto done;
    }

    dmg = (*env)->CallStaticObjectMethod(env, cls_dmg, m_get_instance);
    // Display.DEFAULT_DISPLAY
    if (dmg)
        display = (*env)->CallObjectMethod(env, dmg, m_get_display, 0);
    if (mp_jni_exception_check(env, 1, ctx->log) < 0 || !display)
        goto done;

    p->preferred_csp = pl_color_space_srgb;

    caps = (*env)->CallObjectMethod(env, display, m_get_caps);
    if (mp_jni_exception_check(env, 1, ctx->log) < 0 || !caps)
        goto done;

    if (m_mode_get_types) {
        mode = (*env)->CallObjectMethod(env, display, m_get_mode);
        types = (*env)->CallObjectMethod(env, mode, m_mode_get_types);
    } else {
        types = (*env)->CallObjectMethod(env, caps, m_caps_get_types);
    }
    if (mp_jni_exception_check(env, 1, ctx->log) < 0 || !types)
        goto done;

    max_luma = (*env)->CallFloatMethod(env, caps, m_get_max_luma);
    avg_luma = (*env)->CallFloatMethod(env, caps, m_get_avg_luma);
    min_luma = (*env)->CallFloatMethod(env, caps, m_get_min_luma);
    if (mp_jni_exception_check(env, 1, ctx->log) < 0)
        goto done;

    nb_types = (*env)->GetArrayLength(env, types);
    elems = (*env)->GetIntArrayElements(env, types, NULL);
    for (i = 0; elems && i < nb_types; i++) {
        mp_snprintf_cat(types_str, sizeof(types_str), "%s%d",
                        i ? "," : "", elems[i]);
        switch (elems[i]) {
        case MP_HDR_TYPE_DOLBY_VISION:
        case MP_HDR_TYPE_HDR10:
        case MP_HDR_TYPE_HDR10_PLUS:
            pq = true;
            break;
        case MP_HDR_TYPE_HLG:
        case MP_HDR_TYPE_HLG_PLUS:
            hlg = true;
            break;
        }
    }
    if (elems)
        (*env)->ReleaseIntArrayElements(env, types, elems, JNI_ABORT);

    MP_VERBOSE(ctx, "Display HDR descriptor: types=[%s] max_luma=%.1f "
               "max_average_luma=%.1f min_luma=%.3f\n",
               types_str, max_luma, avg_luma, min_luma);

    if (pq || hlg) {
        p->preferred_csp = (pl_color_space_t){
            .primaries = PL_COLOR_PRIM_BT_2020,
            .transfer  = pq ? PL_COLOR_TRC_PQ : PL_COLOR_TRC_HLG,
            .hdr = {
                .max_luma = MPMAX(max_luma, 0.0f),
                .min_luma = MPMAX(min_luma, 0.0f),
            },
        };
    }

done:
    mp_jni_exception_check(env, 1, ctx->log);
    (*env)->PopLocalFrame(env, NULL);
}

static pl_color_space_t android_preferred_csp(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (!p->csp_queried)
        android_query_display_csp(ctx);

    return p->preferred_csp;
}


static bool android_init(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv = talloc_zero(ctx, struct priv);

    if (!vo_android_init(ctx->vo))
        goto fail;

    p->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(p->egl_display, NULL, NULL)) {
        MP_FATAL(ctx, "EGL failed to initialize.\n");
        goto fail;
    }

    EGLConfig config;
    if (!mpegl_create_context(ctx, p->egl_display, &p->egl_context, &config))
        goto fail;

    p->egl_config = config;

    ANativeWindow *native_window = vo_android_native_window(ctx->vo);
    EGLint format;
    eglGetConfigAttrib(p->egl_display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(native_window, 0, 0, format);

    if (!android_recreate_surface(ctx, NULL))
        goto fail;

    mpegl_load_functions(&p->gl, ctx->log);

    struct ra_ctx_params params = {
        .swap_buffers      = android_swap_buffers,
        .recreate_surface  = android_recreate_surface,
        .preferred_csp     = android_preferred_csp,
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
