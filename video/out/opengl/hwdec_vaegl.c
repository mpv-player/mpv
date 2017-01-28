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

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <va/va_drmcommon.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

#include "config.h"

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

#if HAVE_VAAPI_DRM
#include <va/va_drm.h>

static VADisplay *create_drm_va_display(GL *gl)
{
    int drm_fd = (intptr_t)gl->MPGetNativeDisplay("drm");
    // Note: yes, drm_fd==0 could be valid - but it's rare and doesn't fit with
    //       our slightly crappy way of passing it through, so consider 0 not
    //       valid.
    return drm_fd ? vaGetDisplayDRM(drm_fd) : NULL;
}
#endif

struct va_create_native {
    const char *name;
    VADisplay *(*create)(GL *gl);
};

static const struct va_create_native create_native_cbs[] = {
#if HAVE_VAAPI_X11
    {"x11",     create_x11_va_display},
#endif
#if HAVE_VAAPI_WAYLAND
    {"wayland", create_wayland_va_display},
#endif
#if HAVE_VAAPI_DRM
    {"drm",     create_drm_va_display},
#endif
};

static VADisplay *create_native_va_display(GL *gl, struct mp_log *log)
{
    if (!gl->MPGetNativeDisplay)
        return NULL;
    for (int n = 0; n < MP_ARRAY_SIZE(create_native_cbs); n++) {
        const struct va_create_native *disp = &create_native_cbs[n];
        mp_verbose(log, "Trying to open a %s VA display...\n", disp->name);
        VADisplay *display = disp->create(gl);
        if (display)
            return display;
    }
    return NULL;
}

struct priv {
    struct mp_log *log;
    struct mp_vaapi_ctx *ctx;
    VADisplay *display;
    GLuint gl_textures[4];
    EGLImageKHR images[4];
    VAImage current_image;
    bool buffer_acquired;
    int current_mpfmt;
    int *formats;
    bool probing_formats; // temporary during init

    EGLImageKHR (EGLAPIENTRY *CreateImageKHR)(EGLDisplay, EGLContext,
                                              EGLenum, EGLClientBuffer,
                                              const EGLint *);
    EGLBoolean (EGLAPIENTRY *DestroyImageKHR)(EGLDisplay, EGLImageKHR);
    void (EGLAPIENTRY *EGLImageTargetTexture2DOES)(GLenum, GLeglImageOES);
};

static void determine_working_formats(struct gl_hwdec *hw);

static void unmap_frame(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    VAStatus status;

    for (int n = 0; n < 4; n++) {
        if (p->images[n])
            p->DestroyImageKHR(eglGetCurrentDisplay(), p->images[n]);
        p->images[n] = 0;
    }

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
    unmap_frame(hw);
    destroy_textures(hw);
    if (p->ctx)
        hwdec_devices_remove(hw->devs, &p->ctx->hwctx);
    va_destroy(p->ctx);
}

static int create(struct gl_hwdec *hw)
{
    GL *gl = hw->gl;

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;
    p->current_image.buf = p->current_image.image_id = VA_INVALID_ID;
    p->log = hw->log;

    if (!eglGetCurrentContext())
        return -1;

    const char *exts = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    if (!exts)
        return -1;

    if (!strstr(exts, "EXT_image_dma_buf_import") ||
        !strstr(exts, "EGL_KHR_image_base") ||
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

    p->display = create_native_va_display(gl, hw->log);
    if (!p->display) {
        MP_VERBOSE(hw, "Could not create a VA display.\n");
        return -1;
    }

    p->ctx = va_initialize(p->display, p->log, true);
    if (!p->ctx) {
        vaTerminate(p->display);
        return -1;
    }
    if (!p->ctx->av_device_ref) {
        MP_VERBOSE(hw, "libavutil vaapi code rejected the driver?\n");
        destroy(hw);
        return -1;
    }

    if (hw->probing && va_guess_if_emulated(p->ctx)) {
        destroy(hw);
        return -1;
    }

    MP_VERBOSE(p, "using VAAPI EGL interop\n");

    determine_working_formats(hw);
    if (!p->formats || !p->formats[0]) {
        destroy(hw);
        return -1;
    }

    p->ctx->hwctx.supported_formats = p->formats;
    p->ctx->hwctx.driver_name = hw->driver->name;
    hwdec_devices_add(hw->devs, &p->ctx->hwctx);
    return 0;
}

static bool check_fmt(struct priv *p, int fmt)
{
    for (int n = 0; p->formats[n]; n++) {
        if (p->formats[n] == fmt)
            return true;
    }
    return false;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    // Recreate them to get rid of all previous image data (possibly).
    destroy_textures(hw);

    gl->GenTextures(4, p->gl_textures);
    for (int n = 0; n < 4; n++) {
        gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    p->current_mpfmt = params->hw_subfmt;

    if (!p->probing_formats && !check_fmt(p, p->current_mpfmt)) {
        MP_FATAL(p, "unsupported VA image format %s\n",
                 mp_imgfmt_to_name(p->current_mpfmt));
        return -1;
    }

    params->imgfmt = p->current_mpfmt;
    params->hw_subfmt = 0;

    return 0;
}

#define ADD_ATTRIB(name, value)                         \
    do {                                                \
    assert(num_attribs + 3 < MP_ARRAY_SIZE(attribs));   \
    attribs[num_attribs++] = (name);                    \
    attribs[num_attribs++] = (value);                   \
    attribs[num_attribs] = EGL_NONE;                    \
    } while(0)

static int map_frame(struct gl_hwdec *hw, struct mp_image *hw_image,
                     struct gl_hwdec_frame *out_frame)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;
    VAStatus status;
    VAImage *va_image = &p->current_image;

    unmap_frame(hw);

    status = vaDeriveImage(p->display, va_surface_id(hw_image), va_image);
    if (!CHECK_VA_STATUS(p, "vaDeriveImage()"))
        goto err;

    VABufferInfo buffer_info = {.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME};
    status = vaAcquireBufferHandle(p->display, va_image->buf, &buffer_info);
    if (!CHECK_VA_STATUS(p, "vaAcquireBufferHandle()"))
        goto err;
    p->buffer_acquired = true;

    struct mp_image layout = {0};
    mp_image_set_params(&layout, &hw_image->params);
    mp_image_setfmt(&layout, p->current_mpfmt);
    struct mp_imgfmt_desc fmt = layout.fmt;

    int drm_fmts[8] = {
        // 1 bytes per component, 1-4 components
        MP_FOURCC('R', '8', ' ', ' '),   // DRM_FORMAT_R8
        MP_FOURCC('G', 'R', '8', '8'),   // DRM_FORMAT_GR88
        0,                               // untested (DRM_FORMAT_RGB888?)
        0,                               // untested (DRM_FORMAT_RGBA8888?)
        // 2 bytes per component, 1-4 components
        MP_FOURCC('R', '1', '6', ' '),   // proposed DRM_FORMAT_R16
        MP_FOURCC('G', 'R', '3', '2'),   // proposed DRM_FORMAT_GR32
        0,                               // N/A
        0,                               // N/A
    };

    for (int n = 0; n < layout.num_planes; n++) {
        int attribs[20] = {EGL_NONE};
        int num_attribs = 0;

        int fmt_index = -1;
        int cbits = fmt.component_bits;
        if ((fmt.flags & (MP_IMGFLAG_YUV_P | MP_IMGFLAG_YUV_NV)) &&
            (fmt.flags & MP_IMGFLAG_NE) && cbits >= 8 && cbits <= 16)
        {
            // Regular planar and semi-planar formats.
            fmt_index = fmt.components[n] - 1 + 4 * ((cbits + 7) / 8 - 1);
        } else if (fmt.id == IMGFMT_RGB0 || fmt.id == IMGFMT_BGR0) {
            fmt_index = 3 + 4 * ((cbits + 7) / 8 - 1);
        }

        if (fmt_index < 0 || fmt_index >= 8 || !drm_fmts[fmt_index])
            goto err;

        ADD_ATTRIB(EGL_LINUX_DRM_FOURCC_EXT, drm_fmts[fmt_index]);
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

        out_frame->planes[n] = (struct gl_hwdec_plane){
            .gl_texture = p->gl_textures[n],
            .gl_target = GL_TEXTURE_2D,
            .tex_w = mp_image_plane_w(&layout, n),
            .tex_h = mp_image_plane_h(&layout, n),
        };
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    if (va_image->format.fourcc == VA_FOURCC_YV12)
        MPSWAP(struct gl_hwdec_plane, out_frame->planes[1], out_frame->planes[2]);

    return 0;

err:
    if (!p->probing_formats)
        MP_FATAL(p, "mapping VAAPI EGL image failed\n");
    unmap_frame(hw);
    return -1;
}

static bool try_format(struct gl_hwdec *hw, struct mp_image *surface)
{
    bool ok = false;
    struct mp_image_params params = surface->params;
    if (reinit(hw, &params) >= 0) {
        struct gl_hwdec_frame frame = {0};
        ok = map_frame(hw, surface, &frame) >= 0;
    }
    unmap_frame(hw);
    return ok;
}

static void determine_working_formats(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    int num_formats = 0;
    int *formats = NULL;

    p->probing_formats = true;

    if (HAVE_VAAPI_HWACCEL_OLD) {
        struct mp_image_pool *alloc = mp_image_pool_new(1);
        va_pool_set_allocator(alloc, p->ctx, VA_RT_FORMAT_YUV420);
        struct mp_image *s = mp_image_pool_get(alloc, IMGFMT_VAAPI, 64, 64);
        if (s) {
            va_surface_init_subformat(s);
            if (try_format(hw, s))
                MP_TARRAY_APPEND(p, formats, num_formats, IMGFMT_NV12);
        }
        talloc_free(s);
        talloc_free(alloc);
    } else {
        AVHWFramesConstraints *fc =
            av_hwdevice_get_hwframe_constraints(p->ctx->av_device_ref, NULL);
        if (!fc) {
            MP_WARN(hw, "failed to retrieve libavutil frame constaints\n");
            goto done;
        }
        for (int n = 0; fc->valid_sw_formats[n] != AV_PIX_FMT_NONE; n++) {
            AVBufferRef *fref = NULL;
            fref = av_hwframe_ctx_alloc(p->ctx->av_device_ref);
            if (!fref)
                goto err;
            AVHWFramesContext *fctx = (void *)fref->data;
            struct mp_image *s = NULL;
            AVFrame *frame = NULL;
            fctx->format = AV_PIX_FMT_VAAPI;
            fctx->sw_format = fc->valid_sw_formats[n];
            fctx->width = 128;
            fctx->height = 128;
            if (av_hwframe_ctx_init(fref) < 0)
                goto err;
            frame = av_frame_alloc();
            if (!frame)
                goto err;
            if (av_hwframe_get_buffer(fref, frame, 0) < 0)
                goto err;
            s = mp_image_from_av_frame(frame);
            if (!s || !mp_image_params_valid(&s->params))
                goto err;
            if (try_format(hw, s))
                MP_TARRAY_APPEND(p, formats, num_formats, s->params.hw_subfmt);
        err:
            talloc_free(s);
            av_frame_free(&frame);
            av_buffer_unref(&fref);
        }
        av_hwframe_constraints_free(&fc);
    }

done:
    MP_TARRAY_APPEND(p, formats, num_formats, 0); // terminate it
    p->formats = formats;
    p->probing_formats = false;

    MP_VERBOSE(hw, "Supported formats:\n");
    for (int n = 0; formats[n]; n++)
        MP_VERBOSE(hw, " %s\n", mp_imgfmt_to_name(formats[n]));
}

const struct gl_hwdec_driver gl_hwdec_vaegl = {
    .name = "vaapi-egl",
    .api = HWDEC_VAAPI,
    .imgfmt = IMGFMT_VAAPI,
    .create = create,
    .reinit = reinit,
    .map_frame = map_frame,
    .unmap = unmap_frame,
    .destroy = destroy,
};
