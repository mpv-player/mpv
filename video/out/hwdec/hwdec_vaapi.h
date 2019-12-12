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

#include <va/va_drmcommon.h>

#include "config.h"
#include "video/vaapi.h"
#include "video/out/gpu/hwdec.h"

struct priv_owner {
    struct mp_vaapi_ctx *ctx;
    VADisplay *display;
    int *formats;
    bool probing_formats; // temporary during init

    bool (*interop_init)(struct ra_hwdec_mapper *mapper,
                         const struct ra_imgfmt_desc *desc);
    void (*interop_uninit)(const struct ra_hwdec_mapper *mapper);

    bool (*interop_map)(struct ra_hwdec_mapper *mapper);
    void (*interop_unmap)(struct ra_hwdec_mapper *mapper);
};

struct priv {
    int num_planes;
    struct mp_image layout;
    struct ra_tex *tex[4];

    VADRMPRIMESurfaceDescriptor desc;
    bool surface_acquired;

    void *interop_mapper_priv;
};

typedef bool (*vaapi_interop_init)(const struct ra_hwdec *hw);

bool vaapi_gl_init(const struct ra_hwdec *hw);

bool vaapi_vk_init(const struct ra_hwdec *hw);
