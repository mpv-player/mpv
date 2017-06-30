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
#include "video/vt.h"
#include "formats.h"
#include "hwdec.h"

struct priv {
    struct mp_hwdec_ctx hwctx;

    CVPixelBufferRef pbuf;
    GLuint gl_planes[MP_MAX_PLANES];

    struct gl_imgfmt_desc desc;
};

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

static int create(struct gl_hwdec *hw)
{
    if (!check_hwdec(hw))
        return -1;

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    hw->gl->GenTextures(MP_MAX_PLANES, p->gl_planes);

    p->hwctx = (struct mp_hwdec_ctx){
        .type = HWDEC_VIDEOTOOLBOX,
        .download_image = mp_vt_download_image,
        .ctx = &p->hwctx,
    };

#if HAVE_VIDEOTOOLBOX_HWACCEL_NEW
    av_hwdevice_ctx_create(&p->hwctx.av_device_ref, AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
                           NULL, NULL, 0);
#endif

    hwdec_devices_add(hw->devs, &p->hwctx);

    return 0;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;

    assert(params->imgfmt == hw->driver->imgfmt);

    if (!params->hw_subfmt) {
        MP_ERR(hw, "Unsupported CVPixelBuffer format.\n");
        return -1;
    }

    if (!gl_get_imgfmt_desc(hw->gl, params->hw_subfmt, &p->desc)) {
        MP_ERR(hw, "Unsupported texture format.\n");
        return -1;
    }

    params->imgfmt = params->hw_subfmt;
    params->hw_subfmt = 0;
    return 0;
}

static int map_frame(struct gl_hwdec *hw, struct mp_image *hw_image,
                     struct gl_hwdec_frame *out_frame)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    CVPixelBufferRelease(p->pbuf);
    p->pbuf = (CVPixelBufferRef)hw_image->planes[3];
    CVPixelBufferRetain(p->pbuf);
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(p->pbuf);
    if (!surface) {
        MP_ERR(hw, "CVPixelBuffer has no IOSurface\n");
        return -1;
    }

    const bool planar = CVPixelBufferIsPlanar(p->pbuf);
    const int planes  = CVPixelBufferGetPlaneCount(p->pbuf);
    assert((planar && planes == p->desc.num_planes) || p->desc.num_planes == 1);

    GLenum gl_target = GL_TEXTURE_RECTANGLE;

    for (int i = 0; i < p->desc.num_planes; i++) {
        const struct gl_format *fmt = p->desc.planes[i];

        gl->BindTexture(gl_target, p->gl_planes[i]);

        CGLError err = CGLTexImageIOSurface2D(
            CGLGetCurrentContext(), gl_target,
            fmt->internal_format,
            IOSurfaceGetWidthOfPlane(surface, i),
            IOSurfaceGetHeightOfPlane(surface, i),
            fmt->format, fmt->type, surface, i);

        if (err != kCGLNoError)
            MP_ERR(hw, "error creating IOSurface texture for plane %d: %s (%x)\n",
                   i, CGLErrorString(err), gl->GetError());

        gl->BindTexture(gl_target, 0);

        out_frame->planes[i] = (struct gl_hwdec_plane){
            .gl_texture = p->gl_planes[i],
            .gl_target = gl_target,
            .tex_w = IOSurfaceGetWidthOfPlane(surface, i),
            .tex_h = IOSurfaceGetHeightOfPlane(surface, i),
        };
    }

    return 0;
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    CVPixelBufferRelease(p->pbuf);
    gl->DeleteTextures(MP_MAX_PLANES, p->gl_planes);

    av_buffer_unref(&p->hwctx.av_device_ref);

    hwdec_devices_remove(hw->devs, &p->hwctx);
}

const struct gl_hwdec_driver gl_hwdec_videotoolbox = {
    .name = "videotoolbox",
    .api = HWDEC_VIDEOTOOLBOX,
    .imgfmt = IMGFMT_VIDEOTOOLBOX,
    .create = create,
    .reinit = reinit,
    .map_frame = map_frame,
    .destroy = destroy,
};
