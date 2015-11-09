/*
 * This file is part of mpv.
 *
 * Parts based on the MPlayer VA-API patch (see vo_vaapi.c).
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
 */

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <va/va_drmcommon.h>

#include "hwdec.h"
#include "video/vaapi.h"
#include "video/img_fourcc.h"
#include "video/mp_image_pool.h"
#include "common.h"

#ifndef GL_OES_EGL_image
typedef void* GLeglImageOES;
#endif
#ifndef EGL_KHR_image
typedef void *EGLImageKHR;
#endif

#ifndef EGL_LINUX_DMA_BUF_EXT
#define EGL_LINUX_DMA_BUF_EXT             0x3270
#define EGL_LINUX_DRM_FOURCC_EXT          0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT         0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT     0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT      0x3274
#endif

#if HAVE_VAAPI_X11
#include <va/va_x11.h>

static VADisplay *create_x11_va_display(GL *gl)
{
    Display *x11 = gl->MPGetNativeDisplay("x11");
    return x11 ? vaGetDisplay(x11) : NULL;
}
#endif

#if HAVE_VAAPI_WAYLAND
#include <va/va_wayland.h>

static VADisplay *create_wayland_va_display(GL *gl)
{
    struct wl_display *wl = gl->MPGetNativeDisplay("wl");
    return wl ? vaGetDisplayWl(wl) : NULL;
}
#endif

static VADisplay *create_native_va_display(GL *gl)
{
    if (!gl->MPGetNativeDisplay)
        return NULL;
    VADisplay *display = NULL;
#if HAVE_VAAPI_X11
    display = create_x11_va_display(gl);
    if (display)
        return display;
#endif
#if HAVE_VAAPI_WAYLAND
    display = create_wayland_va_display(gl);
    if (display)
        return display;
#endif
    return display;
}

struct priv {
    struct mp_log *log;
    struct mp_vaapi_ctx *ctx;
    VADisplay *display;
    GLuint gl_textures[4];
    EGLImageKHR images[4];
    VAImage current_image;
    bool buffer_acquired;
    struct mp_image *current_ref;

    EGLImageKHR (EGLAPIENTRY *CreateImageKHR)(EGLDisplay, EGLContext,
                                              EGLenum, EGLClientBuffer,
                                              const EGLint *);
    EGLBoolean (EGLAPIENTRY *DestroyImageKHR)(EGLDisplay, EGLImageKHR);
    void (EGLAPIENTRY *EGLImageTargetTexture2DOES)(GLenum, GLeglImageOES);
};

static bool test_format(struct gl_hwdec *hw);

static void unref_image(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    VAStatus status;

    for (int n = 0; n < 4; n++) {
        if (p->images[n])
            p->DestroyImageKHR(eglGetCurrentDisplay(), p->images[n]);
        p->images[n] = 0;
    }

    va_lock(p->ctx);

    if (p->buffer_acquired) {
        status = vaReleaseBufferHandle(p->display, p->current_image.buf);
        CHECK_VA_STATUS(p, "vaReleaseBufferHandle()");
        p->buffer_acquired = false;
    }
    if (p->current_image.image_id != VA_INVALID_ID) {
        status = vaDestroyImage(p->display, p->current_image.image_id);
        CHECK_VA_STATUS(p, "vaDestroyImage()");
        p->current_image.image_id = VA_INVALID_ID;
    }

    mp_image_unrefp(&p->current_ref);

    va_unlock(p->ctx);
}

static void destroy_textures(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    gl->DeleteTextures(4, p->gl_textures);
    for (int n = 0; n < 4; n++)
        p->gl_textures[n] = 0;
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    unref_image(hw);
    destroy_textures(hw);
    va_destroy(p->ctx);
}

// Create an empty dummy VPP. This works around a weird bug that affects the
// VA surface format, as it is reported by vaDeriveImage(). Before a VPP
// context or a decoder context is created, the surface format will be reported
// as YV12. Surfaces created after context creation will report NV12 (even
// though surface creation does not take a context as argument!). Existing
// surfaces will change their format from YV12 to NV12 as soon as the decoder
// renders to them! Because we want know the surface format in advance (to
// simplify our renderer configuration logic), we hope that this hack gives
// us reasonable behavior.
// See: https://bugs.freedesktop.org/show_bug.cgi?id=79848
static void insane_hack(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    VAConfigID config;
    if (vaCreateConfig(p->display, VAProfileNone, VAEntrypointVideoProc,
                       NULL, 0, &config) == VA_STATUS_SUCCESS)
    {
        // We want to keep this until the VADisplay is destroyed. It will
        // implicitly free the context.
        VAContextID context;
        vaCreateContext(p->display, config, 0, 0, 0, NULL, 0, &context);
    }
}

static int create(struct gl_hwdec *hw)
{
    GL *gl = hw->gl;

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;
    p->current_image.buf = p->current_image.image_id = VA_INVALID_ID;
    p->log = hw->log;

    if (hw->hwctx)
        return -1;
    if (!eglGetCurrentDisplay())
        return -1;

    if (!strstr(gl->extensions, "EXT_image_dma_buf_import") ||
        !strstr(gl->extensions, "EGL_KHR_image_base") ||
        !strstr(gl->extensions, "GL_OES_EGL_image") ||
        !(gl->mpgl_caps & MPGL_CAP_TEX_RG))
        return -1;

    // EGL_KHR_image_base
    p->CreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
    p->DestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
    // GL_OES_EGL_image
    p->EGLImageTargetTexture2DOES =
        (void *)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    if (!p->CreateImageKHR || !p->DestroyImageKHR ||
        !p->EGLImageTargetTexture2DOES)
        return -1;

    p->display = create_native_va_display(gl);
    if (!p->display)
        return -1;

    p->ctx = va_initialize(p->display, p->log, true);
    if (!p->ctx) {
        vaTerminate(p->display);
        return -1;
    }

    if (hw->probing && va_guess_if_emulated(p->ctx)) {
        destroy(hw);
        return -1;
    }

    MP_VERBOSE(p, "using VAAPI EGL interop\n");

    insane_hack(hw);
    if (!test_format(hw)) {
        destroy(hw);
        return -1;
    }

    hw->hwctx = &p->ctx->hwctx;
    return 0;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    // Recreate them to get rid of all previous image data (possibly).
    destroy_textures(hw);

    assert(params->imgfmt == hw->driver->imgfmt);

    gl->GenTextures(4, p->gl_textures);
    for (int n = 0; n < 4; n++) {
        gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    return 0;
}

#define ADD_ATTRIB(name, value)                         \
    do {                                                \
    assert(num_attribs + 3 < MP_ARRAY_SIZE(attribs));   \
    attribs[num_attribs++] = (name);                    \
    attribs[num_attribs++] = (value);                   \
    attribs[num_attribs] = EGL_NONE;                    \
    } while(0)

static int map_image(struct gl_hwdec *hw, struct mp_image *hw_image,
                     GLuint *out_textures)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;
    VAStatus status;
    VAImage *va_image = &p->current_image;

    unref_image(hw);

    mp_image_setrefp(&p->current_ref, hw_image);

    va_lock(p->ctx);

    status = vaDeriveImage(p->display, va_surface_id(hw_image), va_image);
    if (!CHECK_VA_STATUS(p, "vaDeriveImage()"))
        goto err;

    int mpfmt = va_fourcc_to_imgfmt(va_image->format.fourcc);
    if (mpfmt != IMGFMT_NV12 && mpfmt != IMGFMT_420P) {
        MP_FATAL(p, "unsupported VA image format %s\n",
                 VA_STR_FOURCC(va_image->format.fourcc));
        goto err;
    }

    if (!hw->converted_imgfmt) {
        MP_VERBOSE(p, "format: %s %s\n", VA_STR_FOURCC(va_image->format.fourcc),
                   mp_imgfmt_to_name(mpfmt));
        hw->converted_imgfmt = mpfmt;
    }

    if (hw->converted_imgfmt != mpfmt) {
        MP_FATAL(p, "mid-stream hwdec format change (%s -> %s) not supported\n",
                 mp_imgfmt_to_name(hw->converted_imgfmt), mp_imgfmt_to_name(mpfmt));
        goto err;
    }

    VABufferInfo buffer_info = {.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME};
    status = vaAcquireBufferHandle(p->display, va_image->buf, &buffer_info);
    if (!CHECK_VA_STATUS(p, "vaAcquireBufferHandle()"))
        goto err;
    p->buffer_acquired = true;

    struct mp_image layout = {0};
    mp_image_set_params(&layout, &hw_image->params);
    mp_image_setfmt(&layout, mpfmt);

    // (it would be nice if we could use EGL_IMAGE_INTERNAL_FORMAT_EXT)
    int drm_fmts[4] = {MP_FOURCC('R', '8', ' ', ' '),   // DRM_FORMAT_R8
                       MP_FOURCC('G', 'R', '8', '8'),   // DRM_FORMAT_GR88
                       MP_FOURCC('R', 'G', '2', '4'),   // DRM_FORMAT_RGB888
                       MP_FOURCC('R', 'A', '2', '4')};  // DRM_FORMAT_RGBA8888

    for (int n = 0; n < layout.num_planes; n++) {
        int attribs[20] = {EGL_NONE};
        int num_attribs = 0;

        ADD_ATTRIB(EGL_LINUX_DRM_FOURCC_EXT, drm_fmts[layout.fmt.bytes[n] - 1]);
        ADD_ATTRIB(EGL_WIDTH, mp_image_plane_w(&layout, n));
        ADD_ATTRIB(EGL_HEIGHT, mp_image_plane_h(&layout, n));
        ADD_ATTRIB(EGL_DMA_BUF_PLANE0_FD_EXT, buffer_info.handle);
        ADD_ATTRIB(EGL_DMA_BUF_PLANE0_OFFSET_EXT, va_image->offsets[n]);
        ADD_ATTRIB(EGL_DMA_BUF_PLANE0_PITCH_EXT, va_image->pitches[n]);

        p->images[n] = p->CreateImageKHR(eglGetCurrentDisplay(),
            EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
        if (!p->images[n])
            goto err;

        gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
        p->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, p->images[n]);

        out_textures[n] = p->gl_textures[n];
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    if (va_image->format.fourcc == VA_FOURCC_YV12)
        MPSWAP(GLuint, out_textures[1], out_textures[2]);

    va_unlock(p->ctx);
    return 0;

err:
    va_unlock(p->ctx);
    MP_FATAL(p, "mapping VAAPI EGL image failed\n");
    unref_image(hw);
    return -1;
}

static bool test_format(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    bool ok = false;

    struct mp_image_pool *alloc = mp_image_pool_new(1);
    va_pool_set_allocator(alloc, p->ctx, VA_RT_FORMAT_YUV420);
    struct mp_image *surface = mp_image_pool_get(alloc, IMGFMT_VAAPI, 64, 64);
    if (surface) {
        struct mp_image_params params = surface->params;
        if (reinit(hw, &params) >= 0) {
            GLuint textures[4];
            ok = map_image(hw, surface, textures) >= 0;
        }
        unref_image(hw);
    }
    talloc_free(surface);
    talloc_free(alloc);

    return ok;
}

const struct gl_hwdec_driver gl_hwdec_vaegl = {
    .api_name = "vaapi",
    .imgfmt = IMGFMT_VAAPI,
    .create = create,
    .reinit = reinit,
    .map_image = map_image,
    .destroy = destroy,
};
