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

#include "video/decode/dec_video.h"
#include "cocoa_common.h"
#include "gl_common.h"

struct priv {
    CVPixelBufferRef pbuf;
    GLuint gl_texture;
};

static bool check_hwdec(struct gl_hwdec *hw)
{
    struct vo *vo = hw->mpgl->vo;

    if (hw->gl_texture_target != GL_TEXTURE_RECTANGLE) {
        MP_ERR(hw, "must use rectangle video textures with VDA\n");
        return false;
    }

    if (!vo->cocoa) {
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

    GL *gl = hw->mpgl->gl;
    gl->GenTextures(1, &p->gl_texture);

    return 0;
}

static int reinit(struct gl_hwdec *hw, const struct mp_image_params *params)
{
    return 0;
}

static int map_image(struct gl_hwdec *hw, struct mp_image *hw_image,
                     GLuint *out_textures)
{
    if (!check_hwdec(hw))
        return -1;

    struct priv *p = hw->priv;
    struct vo *vo = hw->mpgl->vo;
    GL *gl = hw->mpgl->gl;

    CVPixelBufferRelease(p->pbuf);
    p->pbuf = (CVPixelBufferRef)hw_image->planes[3];
    CVPixelBufferRetain(p->pbuf);
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(p->pbuf);

    gl->BindTexture(hw->gl_texture_target, p->gl_texture);

    CGLError err = CGLTexImageIOSurface2D(
        vo_cocoa_cgl_context(vo), hw->gl_texture_target, GL_RGB,
        CVPixelBufferGetWidth(p->pbuf), CVPixelBufferGetHeight(p->pbuf),
        GL_RGB_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, surface, 0);

    if (err != kCGLNoError)
        MP_ERR(vo, "error creating IOSurface texture: %s (%x)\n",
               CGLErrorString(err), gl->GetError());

    gl->BindTexture(hw->gl_texture_target, 0);

    out_textures[0] = p->gl_texture;
    return 0;
}

static void unmap_image(struct gl_hwdec *hw) { }

static struct mp_image *download_image(struct gl_hwdec *hw,
                                       struct mp_image *hw_image)
{
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

    struct mp_image *image = mp_image_new_copy(&img);
    CVPixelBufferUnlockBaseAddress(pbuf, 0);

    return image;
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->mpgl->gl;

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
    .unmap_image = unmap_image,
    .download_image = download_image,
    .destroy = destroy,
};
