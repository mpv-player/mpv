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

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <android/data_space.h>

#include "video/out/android_common.h"
#include "egl_helpers.h"
#include "common/common.h"
#include "misc/jni.h"
#include "context.h"

// android.view.Display.HdrCapabilities
#define MP_HDR_TYPE_HDR10                  2
#define MP_HDR_TYPE_HLG                    3
#define MP_HDR_TYPE_HDR10_PLUS             4

enum android_hdr_mode {
    ANDROID_HDR_SDR,
    ANDROID_HDR_PQ,
    ANDROID_HDR_HLG,
};

static const struct {
    int32_t dataspace;
    EGLint colorspace;
    const char *name;
} hdr_modes[] = {
    [ANDROID_HDR_SDR] = { ADATASPACE_SRGB, 0, "SDR" },
    [ANDROID_HDR_PQ]  = { ADATASPACE_BT2020_PQ,
                          EGL_GL_COLORSPACE_BT2020_PQ_EXT,
                          "HDR10 (BT.2020/PQ)" },
    [ANDROID_HDR_HLG] = { ADATASPACE_BT2020_HLG,
                          EGL_GL_COLORSPACE_BT2020_HLG_EXT,
                          "HLG (BT.2020)" },
};

static_assert(MP_ARRAY_SIZE(hdr_modes) == ANDROID_HDR_HLG + 1,
              "hdr_modes must have an entry for every mode");

struct priv {
    struct GL gl;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    EGLConfig egl_config;
    EGLint window_format;
    enum android_hdr_mode mode;
    bool hdr_ok;
    int32_t (*set_buffers_dataspace)(ANativeWindow *window, int32_t dataspace);
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

static bool android_set_hdr_surface(struct ra_ctx *ctx,
                                    enum android_hdr_mode mode)
{
    struct priv *p = ctx->priv;
    ANativeWindow *win = vo_android_native_window(ctx->vo);
    EGLSurface surf = EGL_NO_SURFACE;

    if (!win)
        return false;

    if (p->set_buffers_dataspace)
        p->set_buffers_dataspace(win, hdr_modes[mode].dataspace);

    eglMakeCurrent(p->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    if (p->egl_surface != EGL_NO_SURFACE)
        eglDestroySurface(p->egl_display, p->egl_surface);
    p->egl_surface = EGL_NO_SURFACE;

    if (mode != ANDROID_HDR_SDR) {
        EGLint hdr[] = { EGL_GL_COLORSPACE_KHR, hdr_modes[mode].colorspace,
                         EGL_NONE };
        surf = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                      (EGLNativeWindowType)win, hdr);
        if (surf == EGL_NO_SURFACE) {
            EGLint lin[] = { EGL_GL_COLORSPACE_KHR,
                             EGL_GL_COLORSPACE_LINEAR_KHR, EGL_NONE };

            MP_WARN(ctx, "BT.2020 HDR EGL surface unsupported, "
                    "trying linear\n");
            surf = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                          (EGLNativeWindowType)win, lin);
        }
    }
    if (surf == EGL_NO_SURFACE)
        surf = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                      (EGLNativeWindowType)win, NULL);
    if (surf == EGL_NO_SURFACE) {
        MP_ERR(ctx, "Could not recreate EGL surface\n");
        return false;
    }
    p->egl_surface = surf;
    if (!eglMakeCurrent(p->egl_display, p->egl_surface, p->egl_surface,
                        p->egl_context)) {
        MP_ERR(ctx, "Failed to make recreated EGL surface current\n");
        return false;
    }

    p->mode = mode;
    MP_VERBOSE(ctx, "Video surface switched to %s\n", hdr_modes[mode].name);
    return true;
}

static enum android_hdr_mode android_color_mode(
    struct priv *p, const struct pl_color_space *csp)
{
    if (!p->hdr_ok)
        return ANDROID_HDR_SDR;
    if (csp->transfer == PL_COLOR_TRC_PQ)
        return ANDROID_HDR_PQ;
    if (csp->transfer == PL_COLOR_TRC_HLG)
        return ANDROID_HDR_HLG;
    return ANDROID_HDR_SDR;
}

// Ask the Java framework about the HDR capabilities of the default display.
// There is no NDK API for this, so this goes through ActivityThread.
static void android_query_display_csp(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;
    JNIEnv *env = mp_jni_get_env(ctx->log);
    jclass cls_activity = NULL, cls_app = NULL, cls_dm = NULL;
    jclass cls_display = NULL, cls_caps = NULL;
    jobject app = NULL, dm = NULL, display = NULL, caps = NULL;
    jmethodID m_current_app, m_get_service, m_get_display, m_get_caps;
    jmethodID m_get_types, m_get_max_luma;
    jintArray types = NULL;
    jstring service = NULL;
    jint *elems = NULL;
    jsize nb_types = 0;
    bool pq = false, hlg = false;
    jfloat peak = 0;
    int i;

    p->csp_queried = true;

    if (!env || !p->hdr_ok)
        return;

    cls_activity = (*env)->FindClass(env, "android/app/ActivityThread");
    cls_app = (*env)->FindClass(env, "android/app/Application");
    cls_dm = (*env)->FindClass(env,
        "android/hardware/display/DisplayManager");
    cls_display = (*env)->FindClass(env, "android/view/Display");
    cls_caps = (*env)->FindClass(env,
        "android/view/Display$HdrCapabilities");
    if (!cls_activity || !cls_app || !cls_dm || !cls_display || !cls_caps)
        goto done;

    m_current_app = (*env)->GetStaticMethodID(env, cls_activity,
        "currentApplication", "()Landroid/app/Application;");
    m_get_service = (*env)->GetMethodID(env, cls_app, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    m_get_display = (*env)->GetMethodID(env, cls_dm, "getDisplay",
        "(I)Landroid/view/Display;");
    m_get_caps = (*env)->GetMethodID(env, cls_display,
        "getHdrCapabilities", "()Landroid/view/Display$HdrCapabilities;");
    m_get_types = (*env)->GetMethodID(env, cls_caps, "getSupportedHdrTypes",
        "()[I");
    m_get_max_luma = (*env)->GetMethodID(env, cls_caps,
        "getDesiredMaxLuminance", "()F");
    if (!m_current_app || !m_get_service || !m_get_display || !m_get_caps ||
        !m_get_types || !m_get_max_luma)
        goto done;

    app = (*env)->CallStaticObjectMethod(env, cls_activity, m_current_app);
    service = (*env)->NewStringUTF(env, "display");
    if (app && service)
        dm = (*env)->CallObjectMethod(env, app, m_get_service, service);
    // Display.DEFAULT_DISPLAY
    if (dm)
        display = (*env)->CallObjectMethod(env, dm, m_get_display, 0);
    if (display)
        caps = (*env)->CallObjectMethod(env, display, m_get_caps);
    if (mp_jni_exception_check(env, 1, ctx->log) < 0 || !caps)
        goto done;

    types = (*env)->CallObjectMethod(env, caps, m_get_types);
    peak = (*env)->CallFloatMethod(env, caps, m_get_max_luma);
    if (mp_jni_exception_check(env, 1, ctx->log) < 0)
        goto done;

    if (types) {
        nb_types = (*env)->GetArrayLength(env, types);
        elems = (*env)->GetIntArrayElements(env, types, NULL);
        for (i = 0; elems && i < nb_types; i++) {
            if (elems[i] == MP_HDR_TYPE_HDR10 ||
                elems[i] == MP_HDR_TYPE_HDR10_PLUS)
                pq = true;
            else if (elems[i] == MP_HDR_TYPE_HLG)
                hlg = true;
        }
        if (elems)
            (*env)->ReleaseIntArrayElements(env, types, elems, JNI_ABORT);
    }

    if (pq || hlg) {
        p->preferred_csp = (pl_color_space_t){
            .primaries    = PL_COLOR_PRIM_BT_2020,
            .transfer     = pq ? PL_COLOR_TRC_PQ : PL_COLOR_TRC_HLG,
            .hdr.max_luma = peak > 0 ? peak : 1000.0f,
        };
    }

done:
    // Clear any pending exception from a failed lookup above
    mp_jni_exception_check(env, 1, ctx->log);
    MP_VERBOSE(ctx, "Display HDR caps: pq=%d hlg=%d peak=%.0f nits\n",
               pq, hlg, peak);
    MP_JNI_LOCAL_FREEP(&types);
    MP_JNI_LOCAL_FREEP(&service);
    MP_JNI_LOCAL_FREEP(&caps);
    MP_JNI_LOCAL_FREEP(&display);
    MP_JNI_LOCAL_FREEP(&dm);
    MP_JNI_LOCAL_FREEP(&app);
    MP_JNI_LOCAL_FREEP(&cls_caps);
    MP_JNI_LOCAL_FREEP(&cls_display);
    MP_JNI_LOCAL_FREEP(&cls_dm);
    MP_JNI_LOCAL_FREEP(&cls_app);
    MP_JNI_LOCAL_FREEP(&cls_activity);
}

static pl_color_space_t android_preferred_csp(struct ra_ctx *ctx)
{
    struct priv *p = ctx->priv;

    if (!p->csp_queried)
        android_query_display_csp(ctx);

    return p->preferred_csp;
}

static bool android_set_color(struct ra_ctx *ctx,
                              struct mp_image_params *params)
{
    struct priv *p = ctx->priv;

    if (!params) {
        // --target-colorspace-hint=no: explicitly drop back to SDR
        if (p->mode != ANDROID_HDR_SDR &&
            !android_set_hdr_surface(ctx, ANDROID_HDR_SDR))
            return false;
        return true;
    }

    enum android_hdr_mode mode = android_color_mode(p, &params->color);
    bool mode_changed = mode != p->mode;

    if (mode_changed && !android_set_hdr_surface(ctx, mode))
        return false;

    if (mode != ANDROID_HDR_SDR)
        params->color.primaries = PL_COLOR_PRIM_BT_2020;

    if (mode == ANDROID_HDR_PQ) {
        float peak = android_preferred_csp(ctx).hdr.max_luma;

        params->color.transfer = PL_COLOR_TRC_PQ;
        // Use the panel peak as the target, so source-mode hints still tone
        // map content brighter than the display.
        if (peak > 0)
            params->color.hdr.max_luma = peak;
        else if (!params->color.hdr.max_luma)
            params->color.hdr.max_luma = 1000.0f;
    } else if (mode == ANDROID_HDR_HLG) {
        params->color.transfer = PL_COLOR_TRC_HLG;
    } else if (pl_color_transfer_is_hdr(params->color.transfer)) {
        // Unsupported HDR transfer: tone map to SDR
        params->color.primaries = PL_COLOR_PRIM_BT_709;
        params->color.transfer = PL_COLOR_TRC_SRGB;
    }

    if (mode_changed)
        MP_VERBOSE(ctx, "HDR output mode=%d peak=%.0f nits\n",
                   mode, params->color.hdr.max_luma);

    return true;
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
    eglGetConfigAttrib(p->egl_display, config, EGL_NATIVE_VISUAL_ID,
                       &p->window_format);
    MP_VERBOSE(ctx, "EGL window visual=0x%x\n", p->window_format);

    const char *egl_exts = eglQueryString(p->egl_display, EGL_EXTENSIONS);
    bool pq_ext  = egl_exts && strstr(egl_exts,
                                      "EGL_EXT_gl_colorspace_bt2020_pq");
    bool khr_ext = egl_exts && strstr(egl_exts, "EGL_KHR_gl_colorspace");
    void *libandroid = dlopen("libandroid.so", RTLD_LAZY | RTLD_GLOBAL);
    if (libandroid)
        p->set_buffers_dataspace = (void *)dlsym(libandroid,
            "ANativeWindow_setBuffersDataSpace");
    p->hdr_ok = pq_ext && khr_ext && p->set_buffers_dataspace;
    MP_VERBOSE(ctx, "HDR output %s (pq_ext=%d khr_ext=%d dataspace_fn=%d)\n",
               p->hdr_ok ? "available" : "unavailable", pq_ext, khr_ext,
               p->set_buffers_dataspace != NULL);

    ANativeWindow *native_window = vo_android_native_window(ctx->vo);
    ANativeWindow_setBuffersGeometry(native_window, 0, 0, p->window_format);

    p->egl_surface = eglCreateWindowSurface(p->egl_display, p->egl_config,
                                            (EGLNativeWindowType)native_window,
                                            NULL);

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
        .swap_buffers  = android_swap_buffers,
        .set_color     = android_set_color,
        .preferred_csp = android_preferred_csp,
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
    int w, h;
    if (!vo_android_surface_size(ctx->vo, &w, &h))
        return false;

    // Update window geometry to prevent screen tearing
    ANativeWindow *native_window = vo_android_native_window(ctx->vo);
    if (native_window) {
        ANativeWindow_setBuffersGeometry(native_window, w, h, p->window_format);
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
