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

#pragma once

#include <CoreVideo/CoreVideo.h>

#include "config.h"
#include "video/out/gpu/hwdec.h"

struct priv_owner {
    struct mp_hwdec_ctx hwctx;

    int (*interop_init)(struct ra_hwdec_mapper *mapper);
    void (*interop_uninit)(struct ra_hwdec_mapper *mapper);

    int (*interop_map)(struct ra_hwdec_mapper *mapper);
    void (*interop_unmap)(struct ra_hwdec_mapper *mapper);
};

#ifndef __OBJC__
typedef struct __CVMetalTextureCache *CVMetalTextureCacheRef;
typedef CVImageBufferRef CVMetalTextureRef;
#endif

struct priv {
    void *interop_mapper_priv;

    CVPixelBufferRef pbuf;

#if HAVE_VIDEOTOOLBOX_GL && !HAVE_IOS_GL
    GLuint gl_planes[MP_MAX_PLANES];
#endif
#if HAVE_IOS_GL
    CVOpenGLESTextureCacheRef gl_texture_cache;
    CVOpenGLESTextureRef gl_planes[MP_MAX_PLANES];
#endif

#if HAVE_VIDEOTOOLBOX_PL
    CVMetalTextureCacheRef mtl_texture_cache;
    CVMetalTextureRef mtl_planes[MP_MAX_PLANES];
#endif

    struct ra_imgfmt_desc desc;
};

typedef bool (*vt_interop_init)(const struct ra_hwdec *hw);

bool vt_gl_init(const struct ra_hwdec *hw);
bool vt_pl_init(const struct ra_hwdec *hw);
