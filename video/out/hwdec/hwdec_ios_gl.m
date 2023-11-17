/*
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
 *               2017 Aman Gupta <ffmpeg@tmm1.net>
 *
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

#include <assert.h>

#include <CoreVideo/CoreVideo.h>
#include <OpenGLES/EAGL.h>

#include <libavutil/hwcontext.h>

#include "video/out/gpu/hwdec.h"
#include "video/mp_image_pool.h"
#include "video/out/opengl/ra_gl.h"
#include "hwdec_vt.h"

static bool check_hwdec(const struct ra_hwdec *hw)
{
    if (!ra_is_gl(hw->ra_ctx->ra))
        return false;

    GL *gl = ra_gl_get(hw->ra_ctx->ra);
    if (gl->es < 200) {
        MP_ERR(hw, "need OpenGLES 2.0 for CVOpenGLESTextureCacheCreateTextureFromImage()\n");
        return false;
    }

    if ([EAGLContext currentContext] == nil) {
        MP_ERR(hw, "need a current EAGLContext set\n");
        return false;
    }

    return true;
}

// In GLES3 mode, CVOpenGLESTextureCacheCreateTextureFromImage()
// will return error -6683 unless invoked with GL_LUMINANCE and
// GL_LUMINANCE_ALPHA (http://stackoverflow.com/q/36213994/332798)
// If a format trues to use GL_RED/GL_RG instead, try to find a format
// that uses GL_LUMINANCE[_ALPHA] instead.
static const struct ra_format *find_la_variant(struct ra *ra,
                                               const struct ra_format *fmt)
{
    GLint internal_format;
    GLenum format;
    GLenum type;
    ra_gl_get_format(fmt, &internal_format, &format, &type);

    if (format == GL_RED) {
        format = internal_format = GL_LUMINANCE;
    } else if (format == GL_RG) {
        format = internal_format = GL_LUMINANCE_ALPHA;
    } else {
        return fmt;
    }

    for (int n = 0; n < ra->num_formats; n++) {
        const struct ra_format *fmt2 = ra->formats[n];
        GLint internal_format2;
        GLenum format2;
        GLenum type2;
        ra_gl_get_format(fmt2, &internal_format2, &format2, &type2);
        if (internal_format2 == internal_format &&
            format2 == format && type2 == type)
            return fmt2;
    }

    return NULL;
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    for (int n = 0; n < p->desc.num_planes; n++) {
        p->desc.planes[n] = find_la_variant(mapper->ra, p->desc.planes[n]);
        if (!p->desc.planes[n] || p->desc.planes[n]->ctype != RA_CTYPE_UNORM) {
            MP_ERR(mapper, "Format unsupported.\n");
            return -1;
        }
    }

    CVReturn err = CVOpenGLESTextureCacheCreate(
        kCFAllocatorDefault,
        NULL,
        [EAGLContext currentContext],
        NULL,
        &p->gl_texture_cache);

    if (err != noErr) {
        MP_ERR(mapper, "Failure in CVOpenGLESTextureCacheCreate: %d\n", err);
        return -1;
    }

    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    for (int i = 0; i < p->desc.num_planes; i++) {
        ra_tex_free(mapper->ra, &mapper->tex[i]);
        if (p->gl_planes[i]) {
            CFRelease(p->gl_planes[i]);
            p->gl_planes[i] = NULL;
        }
    }

    CVOpenGLESTextureCacheFlush(p->gl_texture_cache, 0);
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    CVPixelBufferRelease(p->pbuf);
    p->pbuf = (CVPixelBufferRef)mapper->src->planes[3];
    CVPixelBufferRetain(p->pbuf);

    const bool planar = CVPixelBufferIsPlanar(p->pbuf);
    const int planes  = CVPixelBufferGetPlaneCount(p->pbuf);
    assert((planar && planes == p->desc.num_planes) || p->desc.num_planes == 1);

    for (int i = 0; i < p->desc.num_planes; i++) {
        const struct ra_format *fmt = p->desc.planes[i];

        GLint internal_format;
        GLenum format;
        GLenum type;
        ra_gl_get_format(fmt, &internal_format, &format, &type);

        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault,
            p->gl_texture_cache,
            p->pbuf,
            NULL,
            GL_TEXTURE_2D,
            internal_format,
            CVPixelBufferGetWidthOfPlane(p->pbuf, i),
            CVPixelBufferGetHeightOfPlane(p->pbuf, i),
            format,
            type,
            i,
            &p->gl_planes[i]);

        if (err != noErr) {
            MP_ERR(mapper, "error creating texture for plane %d: %d\n", i, err);
            return -1;
        }

        gl->BindTexture(GL_TEXTURE_2D, CVOpenGLESTextureGetName(p->gl_planes[i]));
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->BindTexture(GL_TEXTURE_2D, 0);

        struct ra_tex_params params = {
            .dimensions = 2,
            .w = CVPixelBufferGetWidthOfPlane(p->pbuf, i),
            .h = CVPixelBufferGetHeightOfPlane(p->pbuf, i),
            .d = 1,
            .format = fmt,
            .render_src = true,
            .src_linear = true,
        };

        mapper->tex[i] = ra_create_wrapped_tex(
            mapper->ra,
            &params,
            CVOpenGLESTextureGetName(p->gl_planes[i])
        );
        if (!mapper->tex[i])
            return -1;
    }

    return 0;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    CVPixelBufferRelease(p->pbuf);
    if (p->gl_texture_cache) {
        CFRelease(p->gl_texture_cache);
        p->gl_texture_cache = NULL;
    }
}

bool vt_gl_init(const struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    if (!check_hwdec(hw))
        return false;

    p->interop_init   = mapper_init;
    p->interop_uninit = mapper_uninit;
    p->interop_map    = mapper_map;
    p->interop_unmap  = mapper_unmap;

    return true;
}
