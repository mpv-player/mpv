/*
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
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

#include <IOSurface/IOSurface.h>
#include <CoreVideo/CoreVideo.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLIOSurface.h>

#include <libavutil/hwcontext.h>

#include "config.h"

#include "video/mp_image_pool.h"
#include "video/out/gpu/hwdec.h"
#include "ra_gl.h"

struct priv_owner {
    struct mp_hwdec_ctx hwctx;
};

struct priv {
    CVPixelBufferRef pbuf;
    GLuint gl_planes[MP_MAX_PLANES];

    struct ra_imgfmt_desc desc;
};

static bool check_hwdec(struct ra_hwdec *hw)
{
    if (!ra_is_gl(hw->ra))
        return false;

    GL *gl = ra_gl_get(hw->ra);
    if (gl->version < 300) {
        MP_ERR(hw, "need >= OpenGL 3.0 for core rectangle texture support\n");
        return false;
    }

    if (!CGLGetCurrentContext()) {
        MP_ERR(hw, "need cocoa opengl backend to be active");
        return false;
    }

    return true;
}

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    if (!check_hwdec(hw))
        return -1;

    p->hwctx = (struct mp_hwdec_ctx){
        .driver_name = hw->driver->name,
    };

    av_hwdevice_ctx_create(&p->hwctx.av_device_ref, AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
                           NULL, NULL, 0);

    hwdec_devices_add(hw->devs, &p->hwctx);

    return 0;
}

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    gl->GenTextures(MP_MAX_PLANES, p->gl_planes);

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params.hw_subfmt = 0;

    if (!mapper->dst_params.imgfmt) {
        MP_ERR(mapper, "Unsupported CVPixelBuffer format.\n");
        return -1;
    }

    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &p->desc)) {
        MP_ERR(mapper, "Unsupported texture format.\n");
        return -1;
    }

    for (int n = 0; n < p->desc.num_planes; n++) {
        if (p->desc.planes[n]->ctype != RA_CTYPE_UNORM) {
            MP_ERR(mapper, "Format unsupported.\n");
            return -1;
        }
    }
    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    // Is this sane? No idea how to release the texture without deleting it.
    CVPixelBufferRelease(p->pbuf);
    p->pbuf = NULL;

    for (int i = 0; i < p->desc.num_planes; i++)
        ra_tex_free(mapper->ra, &mapper->tex[i]);
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    CVPixelBufferRelease(p->pbuf);
    p->pbuf = (CVPixelBufferRef)mapper->src->planes[3];
    CVPixelBufferRetain(p->pbuf);
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(p->pbuf);
    if (!surface) {
        MP_ERR(mapper, "CVPixelBuffer has no IOSurface\n");
        return -1;
    }

    const bool planar = CVPixelBufferIsPlanar(p->pbuf);
    const int planes  = CVPixelBufferGetPlaneCount(p->pbuf);
    assert((planar && planes == p->desc.num_planes) || p->desc.num_planes == 1);

    GLenum gl_target = GL_TEXTURE_RECTANGLE;

    for (int i = 0; i < p->desc.num_planes; i++) {
        const struct ra_format *fmt = p->desc.planes[i];

        GLint internal_format;
        GLenum format;
        GLenum type;
        ra_gl_get_format(fmt, &internal_format, &format, &type);

        gl->BindTexture(gl_target, p->gl_planes[i]);

        CGLError err = CGLTexImageIOSurface2D(
            CGLGetCurrentContext(), gl_target,
            internal_format,
            IOSurfaceGetWidthOfPlane(surface, i),
            IOSurfaceGetHeightOfPlane(surface, i),
            format, type, surface, i);

        gl->BindTexture(gl_target, 0);

        if (err != kCGLNoError) {
            MP_ERR(mapper,
                   "error creating IOSurface texture for plane %d: %s (%x)\n",
                   i, CGLErrorString(err), gl->GetError());
            return -1;
        }

        struct ra_tex_params params = {
            .dimensions = 2,
            .w = IOSurfaceGetWidthOfPlane(surface, i),
            .h = IOSurfaceGetHeightOfPlane(surface, i),
            .d = 1,
            .format = fmt,
            .render_src = true,
            .src_linear = true,
            .non_normalized = gl_target == GL_TEXTURE_RECTANGLE,
        };

        mapper->tex[i] = ra_create_wrapped_tex(mapper->ra, &params,
                                               p->gl_planes[i]);
        if (!mapper->tex[i])
            return -1;
    }

    return 0;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    gl->DeleteTextures(MP_MAX_PLANES, p->gl_planes);
}

const struct ra_hwdec_driver ra_hwdec_videotoolbox = {
    .name = "videotoolbox",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_VIDEOTOOLBOX, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
