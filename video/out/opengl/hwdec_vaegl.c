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

#include <va/va_drmcommon.h>

#include "video/out/x11_common.h"
#include "hwdec.h"
#include "video/vaapi.h"
#include "video/img_fourcc.h"
#include "common.h"

struct priv {
    struct mp_log *log;
    struct mp_vaapi_ctx *ctx;
    VADisplay *display;
    Display *xdisplay;
    GLuint gl_textures[4];
    EGLImageKHR images[4];
    VAImage current_image;
    bool buffer_acquired;
    struct mp_image *current_ref;
};

static void unref_image(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    VAStatus status;

    for (int n = 0; n < 4; n++) {
        if (p->images[n])
            hw->gl->DestroyImageKHR(eglGetCurrentDisplay(), p->images[n]);
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

static int create(struct gl_hwdec *hw)
{
    GL *gl = hw->gl;

    if (hw->hwctx)
        return -1;
    if (!eglGetCurrentDisplay())
        return -1;

    Display *x11disp =
        hw->gl->MPGetNativeDisplay ? hw->gl->MPGetNativeDisplay("x11") : NULL;
    if (!x11disp)
        return -1;
    if (!gl->CreateImageKHR || !gl->EGLImageTargetTexture2DOES ||
        !strstr(gl->extensions, "EXT_image_dma_buf_import") ||
        !(gl->mpgl_caps & MPGL_CAP_TEX_RG))
        return -1;

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;
    p->current_image.buf = p->current_image.image_id = VA_INVALID_ID;
    p->log = hw->log;
    p->xdisplay = x11disp;
    p->display = vaGetDisplay(x11disp);
    if (!p->display)
        return -1;

    p->ctx = va_initialize(p->display, p->log, true);
    if (!p->ctx) {
        vaTerminate(p->display);
        return -1;
    }

    if (hw->reject_emulated && va_guess_if_emulated(p->ctx)) {
        destroy(hw);
        return -1;
    }

    MP_VERBOSE(p, "using VAAPI EGL interop\n");

    hw->hwctx = &p->ctx->hwctx;
    hw->converted_imgfmt = IMGFMT_NV12;
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
    if (mpfmt != IMGFMT_NV12) {
        MP_FATAL(p, "unsupported VA image format %s\n",
                 VA_STR_FOURCC(va_image->format.fourcc));
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

        p->images[n] = hw->gl->CreateImageKHR(eglGetCurrentDisplay(),
            EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
        if (!p->images[n])
            goto err;

        gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
        gl->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, p->images[n]);

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

const struct gl_hwdec_driver gl_hwdec_vaegl = {
    .api_name = "vaapi",
    .imgfmt = IMGFMT_VAAPI,
    .create = create,
    .reinit = reinit,
    .map_image = map_image,
    .destroy = destroy,
};
