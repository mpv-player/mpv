/*
 * Copyright (c) 2019 Philip Langdale <philipl@overt.org>
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

#pragma once

#include <ffnvcodec/dynlink_loader.h>

#include "video/out/gpu/hwdec.h"

struct cuda_hw_priv {
    struct mp_hwdec_ctx hwctx;
    CudaFunctions *cu;
    CUcontext display_ctx;
    CUcontext decode_ctx;

    // Stored as int to avoid depending on libplacebo enum
    int handle_type;

    // Do we need to do a full CPU sync after copying
    bool do_full_sync;

    bool (*ext_init)(struct ra_hwdec_mapper *mapper,
                     const struct ra_format *format, int n);
    void (*ext_uninit)(const struct ra_hwdec_mapper *mapper, int n);

    // These are only necessary if the gpu api requires synchronisation
    bool (*ext_wait)(const struct ra_hwdec_mapper *mapper, int n);
    bool (*ext_signal)(const struct ra_hwdec_mapper *mapper, int n);
};

struct cuda_mapper_priv {
    struct mp_image layout;
    CUarray cu_array[4];

    CUcontext display_ctx;

    void *ext[4];
};

typedef bool (*cuda_interop_init)(const struct ra_hwdec *hw);

bool cuda_gl_init(const struct ra_hwdec *hw);

bool cuda_vk_init(const struct ra_hwdec *hw);

int check_cu(const struct ra_hwdec *hw, CUresult err, const char *func);
