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

#include "hwdec_cuda.h"
#include "options/m_config.h"
#include "options/options.h"
#include "video/out/opengl/formats.h"
#include "video/out/opengl/ra_gl.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <unistd.h>

#define CHECK_CU(x) check_cu((mapper)->owner, (x), #x)

struct ext_gl {
    CUgraphicsResource cu_res;
};

static bool cuda_ext_gl_init(struct ra_hwdec_mapper *mapper,
                             const struct ra_format *format, int n)
{
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    struct cuda_mapper_priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;
    int ret = 0;
    CUcontext dummy;

    struct ext_gl *egl = talloc_ptrtype(NULL, egl);
    p->ext[n] = egl;

    struct ra_tex_params params = {
        .dimensions = 2,
        .w = mp_image_plane_w(&p->layout, n),
        .h = mp_image_plane_h(&p->layout, n),
        .d = 1,
        .format = format,
        .render_src = true,
        .src_linear = format->linear_filter,
    };

    mapper->tex[n] = ra_tex_create(mapper->ra, &params);
    if (!mapper->tex[n]) {
        goto error;
    }

    GLuint texture;
    GLenum target;
    ra_gl_get_raw_tex(mapper->ra, mapper->tex[n], &texture, &target);

    ret = CHECK_CU(cu->cuGraphicsGLRegisterImage(&egl->cu_res, texture, target,
                                                 CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cu->cuGraphicsMapResources(1, &egl->cu_res, 0));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cu->cuGraphicsSubResourceGetMappedArray(&p->cu_array[n], egl->cu_res,
                                                           0, 0));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cu->cuGraphicsUnmapResources(1, &egl->cu_res, 0));
    if (ret < 0)
        goto error;

    return true;

error:
    CHECK_CU(cu->cuCtxPopCurrent(&dummy));
    return false;
}

static void cuda_ext_gl_uninit(const struct ra_hwdec_mapper *mapper, int n)
{
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    struct cuda_mapper_priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;

    struct ext_gl *egl = p->ext[n];
    if (egl && egl->cu_res) {
        CHECK_CU(cu->cuGraphicsUnregisterResource(egl->cu_res));
        egl->cu_res = 0;
    }
    talloc_free(egl);
}

#undef CHECK_CU
#define CHECK_CU(x) check_cu(hw, (x), #x)

static bool cuda_gl_check(const struct ra_hwdec *hw) {
    if (!ra_is_gl(hw->ra_ctx->ra))
        return false; // This is not an OpenGL RA.

    GL *gl = ra_gl_get(hw->ra_ctx->ra);
    if (gl->version < 210 && gl->es < 300) {
        MP_VERBOSE(hw, "need OpenGL >= 2.1 or OpenGL-ES >= 3.0\n");
        return false;
    }

    return true;
}

static bool cuda_gl_init(const struct ra_hwdec *hw) {
    int ret = 0;
    struct cuda_hw_priv *p = hw->priv;
    CudaFunctions *cu = p->cu;

    CUdevice display_dev;
    unsigned int device_count;
    ret = CHECK_CU(cu->cuGLGetDevices(&device_count, &display_dev, 1,
                                      CU_GL_DEVICE_LIST_ALL));
    if (ret < 0)
        return false;

    ret = CHECK_CU(cu->cuCtxCreate(&p->display_ctx, CU_CTX_SCHED_BLOCKING_SYNC,
                                   display_dev));
    if (ret < 0)
        return false;

    p->decode_ctx = p->display_ctx;

    struct cuda_opts *opts = mp_get_config_group(NULL, hw->global, &cuda_conf);
    int decode_dev_idx = opts->cuda_device;
    talloc_free(opts);

    if (decode_dev_idx > -1) {
        CUcontext dummy;
        CUdevice decode_dev;
        ret = CHECK_CU(cu->cuDeviceGet(&decode_dev, decode_dev_idx));
        if (ret < 0) {
            CHECK_CU(cu->cuCtxPopCurrent(&dummy));
            return false;
        }

        if (decode_dev != display_dev) {
            MP_INFO(hw, "Using separate decoder and display devices\n");

            // Pop the display context. We won't use it again during init()
            ret = CHECK_CU(cu->cuCtxPopCurrent(&dummy));
            if (ret < 0)
                return false;

            ret = CHECK_CU(cu->cuCtxCreate(&p->decode_ctx, CU_CTX_SCHED_BLOCKING_SYNC,
                                           decode_dev));
            if (ret < 0)
                return false;
        }
    }

    // We don't have a way to do a GPU sync after copying
    p->do_full_sync = true;

    p->ext_init = cuda_ext_gl_init;
    p->ext_uninit = cuda_ext_gl_uninit;

    return true;
}

struct cuda_interop_fn cuda_gl_fn = {
    .check = cuda_gl_check,
    .init = cuda_gl_init
};
