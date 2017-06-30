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

#include "config.h"

#include "video/mp_image_pool.h"
#include "video/vt.h"
#include "formats.h"
#include "hwdec.h"

struct priv {
    struct mp_hwdec_ctx hwctx;

    CVPixelBufferRef pbuf;
    CVOpenGLESTextureCacheRef gl_texture_cache;
    CVOpenGLESTextureRef gl_planes[MP_MAX_PLANES];
    struct gl_imgfmt_desc desc;
};

static bool check_hwdec(struct gl_hwdec *hw)
{
    if (hw->gl->es < 200) {
        MP_ERR(hw, "need OpenGLES 2.0 for CVOpenGLESTextureCacheCreateTextureFromImage()\n");
        return false;
    }

    if ([EAGLContext currentContext] == nil) {
        MP_ERR(hw, "need a current EAGLContext set\n");
        return false;
    }

    return true;
}

static int create_hwdec(struct gl_hwdec *hw)
{
    if (!check_hwdec(hw))
        return -1;

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    CVReturn err = CVOpenGLESTextureCacheCreate(
        kCFAllocatorDefault,
        NULL,
        [EAGLContext currentContext],
        NULL,
        &p->gl_texture_cache);

    if (err != noErr) {
        MP_ERR(hw, "Failure in CVOpenGLESTextureCacheCreate: %d\n", err);
        return -1;
    }

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

static void cleanup_textures(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    int i;

    for (i = 0; i < MP_MAX_PLANES; i++) {
        if (p->gl_planes[i]) {
            CFRelease(p->gl_planes[i]);
            p->gl_planes[i] = NULL;
        }
    }

    CVOpenGLESTextureCacheFlush(p->gl_texture_cache, 0);
}

static int map_frame(struct gl_hwdec *hw, struct mp_image *hw_image,
                     struct gl_hwdec_frame *out_frame)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    CVPixelBufferRelease(p->pbuf);
    p->pbuf = (CVPixelBufferRef)hw_image->planes[3];
    CVPixelBufferRetain(p->pbuf);

    const bool planar = CVPixelBufferIsPlanar(p->pbuf);
    const int planes  = CVPixelBufferGetPlaneCount(p->pbuf);
    assert((planar && planes == p->desc.num_planes) || p->desc.num_planes == 1);

    cleanup_textures(hw);

    for (int i = 0; i < p->desc.num_planes; i++) {
        const struct gl_format *fmt = p->desc.planes[i];
        GLenum format = fmt->format;
        GLenum internal_format = fmt->internal_format;

        if (hw->gl->es >= 300) {
            // In GLES3 mode, CVOpenGLESTextureCacheCreateTextureFromImage()
            // will return error -6683 unless invoked with GL_LUMINANCE and
            // GL_LUMINANCE_ALPHA (http://stackoverflow.com/q/36213994/332798)
            if (format == GL_RED) {
                format = internal_format = GL_LUMINANCE;
            } else if (format == GL_RG) {
                format = internal_format = GL_LUMINANCE_ALPHA;
            }
        }

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
            fmt->type,
            i,
            &p->gl_planes[i]);

        if (err != noErr) {
            MP_ERR(hw, "error creating texture for plane %d: %d\n", i, err);
            return -1;
        }

        gl->BindTexture(GL_TEXTURE_2D, CVOpenGLESTextureGetName(p->gl_planes[i]));
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->BindTexture(GL_TEXTURE_2D, 0);

        out_frame->planes[i] = (struct gl_hwdec_plane){
            .gl_texture = CVOpenGLESTextureGetName(p->gl_planes[i]),
            .gl_target = GL_TEXTURE_2D,
            .gl_format = format,
            .tex_w = CVPixelBufferGetWidthOfPlane(p->pbuf, i),
            .tex_h = CVPixelBufferGetHeightOfPlane(p->pbuf, i),
        };
    }

    return 0;
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;

    cleanup_textures(hw);

    CVPixelBufferRelease(p->pbuf);
    CFRelease(p->gl_texture_cache);
    p->gl_texture_cache = NULL;

    av_buffer_unref(&p->hwctx.av_device_ref);

    hwdec_devices_remove(hw->devs, &p->hwctx);
}

const struct gl_hwdec_driver gl_hwdec_videotoolbox = {
    .name = "videotoolbox",
    .api = HWDEC_VIDEOTOOLBOX,
    .imgfmt = IMGFMT_VIDEOTOOLBOX,
    .create = create_hwdec,
    .reinit = reinit,
    .map_frame = map_frame,
    .destroy = destroy,
};
