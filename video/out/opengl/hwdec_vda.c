/*
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
 *
 * This file is part of mpv.
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

// Note: handles both VDA and VideoToolbox

#include <IOSurface/IOSurface.h>
#include <CoreVideo/CoreVideo.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLIOSurface.h>

#include "video/mp_image_pool.h"
#include "hwdec.h"

struct vda_gl_plane_format {
    GLenum gl_format;
    GLenum gl_type;
    GLenum gl_internal_format;
};

struct vda_format {
    uint32_t cvpixfmt;
    int imgfmt;
    int planes;
    struct vda_gl_plane_format gl[MP_MAX_PLANES];
};

struct priv {
    CVPixelBufferRef pbuf;
    GLuint gl_planes[MP_MAX_PLANES];
    struct mp_hwdec_ctx hwctx;
};

static struct vda_format vda_formats[] = {
    {
        .cvpixfmt = kCVPixelFormatType_422YpCbCr8,
        .imgfmt = IMGFMT_UYVY,
        .planes = 1,
        .gl = {
            { GL_RGB_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, GL_RGB }
        }
    }, {
        .cvpixfmt = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
        .imgfmt = IMGFMT_NV12,
        .planes = 2,
        .gl = {
            { GL_RED, GL_UNSIGNED_BYTE, GL_RED },
            { GL_RG,  GL_UNSIGNED_BYTE, GL_RG } ,
        }
    }
};

static struct vda_format *vda_get_gl_format(uint32_t cvpixfmt)
{
    for (int i = 0; i < MP_ARRAY_SIZE(vda_formats); i++) {
        if (vda_formats[i].cvpixfmt == cvpixfmt)
            return &vda_formats[i];
    }
    return NULL;
}

static struct vda_format *vda_get_gl_format_from_imgfmt(uint32_t imgfmt)
{
    for (int i = 0; i < MP_ARRAY_SIZE(vda_formats); i++) {
        if (vda_formats[i].imgfmt == imgfmt)
            return &vda_formats[i];
    }
    return NULL;
}

static struct mp_image *download_image(struct mp_hwdec_ctx *ctx,
                                       struct mp_image *hw_image,
                                       struct mp_image_pool *swpool)
{
    if (hw_image->imgfmt != IMGFMT_VDA && hw_image->imgfmt != IMGFMT_VIDEOTOOLBOX)
        return NULL;

    CVPixelBufferRef pbuf = (CVPixelBufferRef)hw_image->planes[3];
    CVPixelBufferLockBaseAddress(pbuf, 0);
    size_t width  = CVPixelBufferGetWidth(pbuf);
    size_t height = CVPixelBufferGetHeight(pbuf);
    uint32_t cvpixfmt = CVPixelBufferGetPixelFormatType(pbuf);
    struct vda_format *f = vda_get_gl_format(cvpixfmt);
    if (!f) {
        CVPixelBufferUnlockBaseAddress(pbuf, 0);
        return NULL;
    }

    struct mp_image img = {0};
    mp_image_setfmt(&img, f->imgfmt);
    mp_image_set_size(&img, width, height);

    for (int i = 0; i < f->planes; i++) {
        void *base    = CVPixelBufferGetBaseAddressOfPlane(pbuf, i);
        size_t stride = CVPixelBufferGetBytesPerRowOfPlane(pbuf, i);
        img.planes[i] = base;
        img.stride[i] = stride;
    }

    mp_image_copy_attributes(&img, hw_image);

    struct mp_image *image = mp_image_pool_new_copy(swpool, &img);
    CVPixelBufferUnlockBaseAddress(pbuf, 0);

    return image;
}

static bool check_hwdec(struct gl_hwdec *hw)
{
    if (hw->gl->version < 300) {
        MP_ERR(hw, "need >= OpenGL 3.0 for core rectangle texture support\n");
        return false;
    }

    if (!CGLGetCurrentContext()) {
        MP_ERR(hw, "need cocoa opengl backend to be active");
        return false;
    }

    return true;
}

static int create_common(struct gl_hwdec *hw, struct vda_format *format)
{
    struct priv *p = talloc_zero(hw, struct priv);

    hw->priv = p;
    hw->gl_texture_target = GL_TEXTURE_RECTANGLE;

    hw->converted_imgfmt = format->imgfmt;

    if (!check_hwdec(hw))
        return -1;

    hw->hwctx = &p->hwctx;
    hw->hwctx->download_image = download_image;

    GL *gl = hw->gl;
    gl->GenTextures(MP_MAX_PLANES, p->gl_planes);

    return 0;
}

static int create(struct gl_hwdec *hw)
{
    // For videotoolbox, we always request NV12.
#if HAVE_VDA_DEFAULT_INIT2
    struct vda_format *f = vda_get_gl_format_from_imgfmt(IMGFMT_NV12);
#else
    struct vda_format *f = vda_get_gl_format_from_imgfmt(IMGFMT_UYVY);
#endif
    if (create_common(hw, f))
        return -1;

    hw->hwctx->type = HWDEC_VIDEOTOOLBOX;

    return 0;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    params->imgfmt = hw->driver->imgfmt;
    return 0;
}

static int map_image(struct gl_hwdec *hw, struct mp_image *hw_image,
                     GLuint *out_textures)
{
    if (!check_hwdec(hw))
        return -1;

    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    CVPixelBufferRelease(p->pbuf);
    p->pbuf = (CVPixelBufferRef)hw_image->planes[3];
    CVPixelBufferRetain(p->pbuf);
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(p->pbuf);

    uint32_t cvpixfmt = CVPixelBufferGetPixelFormatType(p->pbuf);
    struct vda_format *f = vda_get_gl_format(cvpixfmt);
    if (!f) {
        MP_ERR(hw, "CVPixelBuffer has unsupported format type\n");
        return -1;
    }

    const bool planar = CVPixelBufferIsPlanar(p->pbuf);
    const int planes  = CVPixelBufferGetPlaneCount(p->pbuf);
    assert(planar && planes == f->planes || f->planes == 1);

    for (int i = 0; i < f->planes; i++) {
        gl->BindTexture(hw->gl_texture_target, p->gl_planes[i]);

        CGLError err = CGLTexImageIOSurface2D(
            CGLGetCurrentContext(), hw->gl_texture_target,
            f->gl[i].gl_internal_format,
            IOSurfaceGetWidthOfPlane(surface, i),
            IOSurfaceGetHeightOfPlane(surface, i),
            f->gl[i].gl_format, f->gl[i].gl_type, surface, i);

        if (err != kCGLNoError)
            MP_ERR(hw, "error creating IOSurface texture for plane %d: %s (%x)\n",
                   i, CGLErrorString(err), gl->GetError());

        gl->BindTexture(hw->gl_texture_target, 0);

        out_textures[i] = p->gl_planes[i];
    }

    return 0;
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    CVPixelBufferRelease(p->pbuf);
    gl->DeleteTextures(MP_MAX_PLANES, p->gl_planes);
}

const struct gl_hwdec_driver gl_hwdec_videotoolbox = {
    .api_name = "videotoolbox",
    .imgfmt = IMGFMT_VIDEOTOOLBOX,
    .create = create,
    .reinit = reinit,
    .map_image = map_image,
    .destroy = destroy,
};
