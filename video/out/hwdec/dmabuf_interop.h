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

#include <libavutil/hwcontext_drm.h>

#include "video/out/gpu/hwdec.h"

struct dmabuf_interop {
    bool use_modifiers;
    bool composed_layers;

    bool (*interop_init)(struct ra_hwdec_mapper *mapper,
                         const struct ra_imgfmt_desc *desc);
    void (*interop_uninit)(const struct ra_hwdec_mapper *mapper);

    bool (*interop_map)(struct ra_hwdec_mapper *mapper,
                        struct dmabuf_interop *dmabuf_interop,
                        bool probing);
    void (*interop_unmap)(struct ra_hwdec_mapper *mapper);
};

struct dmabuf_interop_priv {
    int num_planes;
    struct mp_image layout;
    struct ra_tex *tex[AV_DRM_MAX_PLANES];

    AVDRMFrameDescriptor desc;
    bool surface_acquired;

    void *interop_mapper_priv;
};

typedef bool (*dmabuf_interop_init)(const struct ra_hwdec *hw,
                                    struct dmabuf_interop *dmabuf_interop);

bool dmabuf_interop_gl_init(const struct ra_hwdec *hw,
                            struct dmabuf_interop *dmabuf_interop);
bool dmabuf_interop_pl_init(const struct ra_hwdec *hw,
                            struct dmabuf_interop *dmabuf_interop);
bool dmabuf_interop_wl_init(const struct ra_hwdec *hw,
                            struct dmabuf_interop *dmabuf_interop);
