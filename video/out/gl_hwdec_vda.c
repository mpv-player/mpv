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

#include <IOSurface/IOSurface.h>
#include <CoreVideo/CoreVideo.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLIOSurface.h>

#include "video/mp_image_pool.h"
#include "gl_hwdec.h"

struct priv {
    CVPixelBufferRef pbuf;
    GLuint gl_texture;
    struct mp_hwdec_ctx hwctx;
};

static struct mp_image *download_image(struct mp_hwdec_ctx *ctx,
                                       struct mp_image *hw_image,
                                       struct mp_image_pool *swpool)
{
    if (hw_image->imgfmt != IMGFMT_VDA)
        return NULL;

    CVPixelBufferRef pbuf = (CVPixelBufferRef)hw_image->planes[3];
    CVPixelBufferLockBaseAddress(pbuf, 0);
    void *base = CVPixelBufferGetBaseAddress(pbuf);
    size_t width  = CVPixelBufferGetWidth(pbuf);
    size_t height = CVPixelBufferGetHeight(pbuf);
    size_t stride = CVPixelBufferGetBytesPerRow(pbuf);

    struct mp_image img = {0};
    mp_image_setfmt(&img, IMGFMT_UYVY);
    mp_image_set_size(&img, width, height);
    img.planes[0] = base;
    img.stride[0] = stride;
    mp_image_copy_attributes(&img, hw_image);

    struct mp_image *image = mp_image_pool_new_copy(swpool, &img);
    CVPixelBufferUnlockBaseAddress(pbuf, 0);

    return image;
}

static bool check_hwdec(struct gl_hwdec *hw)
{
    if (hw->gl_texture_target != GL_TEXTURE_RECTANGLE) {
        MP_ERR(hw, "must use rectangle video textures with VDA\n");
        return false;
    }

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

static int create(struct gl_hwdec *hw)
{
    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;
    hw->converted_imgfmt = IMGFMT_UYVY;
    hw->gl_texture_target = GL_TEXTURE_RECTANGLE;

    if (!check_hwdec(hw))
        return -1;

    hw->hwctx = &p->hwctx;
    hw->hwctx->type = HWDEC_VDA;
    hw->hwctx->download_image = download_image;

    GL *gl = hw->gl;
    gl->GenTextures(1, &p->gl_texture);

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

    gl->BindTexture(hw->gl_texture_target, p->gl_texture);

    CGLError err = CGLTexImageIOSurface2D(
        CGLGetCurrentContext(), hw->gl_texture_target, GL_RGB,
        CVPixelBufferGetWidth(p->pbuf), CVPixelBufferGetHeight(p->pbuf),
        GL_RGB_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, surface, 0);

    if (err != kCGLNoError)
        MP_ERR(hw, "error creating IOSurface texture: %s (%x)\n",
               CGLErrorString(err), gl->GetError());

    gl->BindTexture(hw->gl_texture_target, 0);

    out_textures[0] = p->gl_texture;
    return 0;
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    CVPixelBufferRelease(p->pbuf);
    gl->DeleteTextures(1, &p->gl_texture);
    p->gl_texture = 0;
}

const struct gl_hwdec_driver gl_hwdec_vda = {
    .api_name = "vda",
    .imgfmt = IMGFMT_VDA,
    .create = create,
    .reinit = reinit,
    .map_image = map_image,
    .destroy = destroy,
};
