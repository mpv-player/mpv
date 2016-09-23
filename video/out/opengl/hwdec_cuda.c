/*
 * Copyright (c) 2016 Philip Langdale <philipl@overt.org>
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

/*
 * This hwdec implements an optimized output path using CUDA->OpenGL
 * interop for frame data that is stored in CUDA device memory.
 * Although it is not explicit in the code here, the only practical way
 * to get data in this form is from the 'cuvid' decoder (aka NvDecode).
 *
 * For now, cuvid/NvDecode will always return images in NV12 format, even
 * when decoding 10bit streams (there is some hardware dithering going on).
 */

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>

#include "video/mp_image_pool.h"
#include "hwdec.h"
#include "video.h"

#include <cudaGL.h>

struct priv {
    struct mp_hwdec_ctx hwctx;
    struct mp_image layout;
    GLuint gl_textures[2];
    CUgraphicsResource cu_res[2];
    CUarray cu_array[2];
    bool mapped;

    CUcontext cuda_ctx;
};

static int check_cu(struct gl_hwdec *hw, CUresult err, const char *func)
{
    const char *err_name;
    const char *err_string;

    MP_TRACE(hw, "Calling %s\n", func);

    if (err == CUDA_SUCCESS)
        return 0;

    cuGetErrorName(err, &err_name);
    cuGetErrorString(err, &err_string);

    MP_ERR(hw, "%s failed", func);
    if (err_name && err_string)
        MP_ERR(hw, " -> %s: %s", err_name, err_string);
    MP_ERR(hw, "\n");

    return -1;
}

#define CHECK_CU(x) check_cu(hw, (x), #x)

static struct mp_image *cuda_download_image(struct mp_hwdec_ctx *ctx,
                                            struct mp_image *hw_image,
                                            struct mp_image_pool *swpool)
{
    CUcontext cuda_ctx = ctx->ctx;
    CUcontext dummy;
    CUresult err, eerr;

    if (hw_image->imgfmt != IMGFMT_CUDA)
        return NULL;

    struct mp_image *out = mp_image_pool_get(swpool, IMGFMT_NV12,
                                             hw_image->w, hw_image->h);
    if (!out)
        return NULL;

    err = cuCtxPushCurrent(cuda_ctx);
    if (err != CUDA_SUCCESS)
        goto error;

    mp_image_set_size(out, hw_image->w, hw_image->h);
    mp_image_copy_attributes(out, hw_image);

    for (int n = 0; n < 2; n++) {
       CUDA_MEMCPY2D cpy = {
            .srcMemoryType = CU_MEMORYTYPE_DEVICE,
            .dstMemoryType = CU_MEMORYTYPE_HOST,
            .srcDevice     = (CUdeviceptr)hw_image->planes[n],
            .dstHost       = out->planes[n],
            .srcPitch      = hw_image->stride[n],
            .dstPitch      = out->stride[n],
            .WidthInBytes  = mp_image_plane_w(out, n) * (n + 1),
            .Height        = mp_image_plane_h(out, n),
        };

        err = cuMemcpy2D(&cpy);
        if (err != CUDA_SUCCESS) {
            goto error;
        }
    }

 error:
    eerr = cuCtxPopCurrent(&dummy);
    if (eerr != CUDA_SUCCESS || err != CUDA_SUCCESS) {
        talloc_free(out);
        return NULL;
    }

    return out;
}

static int cuda_create(struct gl_hwdec *hw)
{
    CUdevice device;
    CUcontext cuda_ctx = NULL;
    CUcontext dummy;
    unsigned int device_count;
    int ret = 0, eret = 0;

    if (hw->gl->version < 210 && hw->gl->es < 300) {
        MP_ERR(hw, "need OpenGL >= 2.1 or OpenGL-ES >= 3.0\n");
        return -1;
    }

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    ret = CHECK_CU(cuInit(0));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cuGLGetDevices(&device_count, &device, 1,
                                  CU_GL_DEVICE_LIST_ALL));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cuCtxCreate(&cuda_ctx, CU_CTX_SCHED_BLOCKING_SYNC, device));
    if (ret < 0)
        goto error;

    p->cuda_ctx = cuda_ctx;

    p->hwctx = (struct mp_hwdec_ctx) {
        .type = HWDEC_CUDA,
        .ctx = cuda_ctx,
        .download_image = cuda_download_image,
    };
    p->hwctx.driver_name = hw->driver->name;
    hwdec_devices_add(hw->devs, &p->hwctx);

 error:
   eret = CHECK_CU(cuCtxPopCurrent(&dummy));
   if (eret < 0)
       return eret;

   return ret;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;
    CUcontext dummy;
    int ret = 0, eret = 0;

    assert(params->imgfmt == hw->driver->imgfmt);
    params->imgfmt = IMGFMT_NV12;
    params->hw_subfmt = 0;

    mp_image_set_params(&p->layout, params);

    ret = CHECK_CU(cuCtxPushCurrent(p->cuda_ctx));
    if (ret < 0)
        return ret;

    gl->GenTextures(2, p->gl_textures);
    for (int n = 0; n < 2; n++) {
        gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
        GLenum filter = GL_NEAREST;
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->TexImage2D(GL_TEXTURE_2D, 0, n == 0 ? GL_R8 : GL_RG8,
                       mp_image_plane_w(&p->layout, n),
                       mp_image_plane_h(&p->layout, n),
                       0, n == 0 ? GL_RED : GL_RG, GL_UNSIGNED_BYTE, NULL);
        gl->BindTexture(GL_TEXTURE_2D, 0);

        ret = CHECK_CU(cuGraphicsGLRegisterImage(&p->cu_res[n], p->gl_textures[n],
                                                 GL_TEXTURE_2D,
                                                 CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
        if (ret < 0)
            goto error;

        ret = CHECK_CU(cuGraphicsMapResources(1, &p->cu_res[n], 0));
        if (ret < 0)
            goto error;

        ret = CHECK_CU(cuGraphicsSubResourceGetMappedArray(&p->cu_array[n], p->cu_res[n],
                                                           0, 0));
        if (ret < 0)
            goto error;

        ret = CHECK_CU(cuGraphicsUnmapResources(1, &p->cu_res[n], 0));
        if (ret < 0)
            goto error;
    }

 error:
    eret = CHECK_CU(cuCtxPopCurrent(&dummy));
    if (eret < 0)
        return eret;

    return ret;
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;
    CUcontext dummy;

    // Don't bail if any CUDA calls fail. This is all best effort.
    CHECK_CU(cuCtxPushCurrent(p->cuda_ctx));
    for (int n = 0; n < 2; n++) {
        if (p->cu_res[n] > 0)
            CHECK_CU(cuGraphicsUnregisterResource(p->cu_res[n]));
    }
    CHECK_CU(cuCtxPopCurrent(&dummy));

    gl->DeleteTextures(2, p->gl_textures);

    hwdec_devices_remove(hw->devs, &p->hwctx);
}

static int map_frame(struct gl_hwdec *hw, struct mp_image *hw_image,
                     struct gl_hwdec_frame *out_frame)
{
    struct priv *p = hw->priv;
    CUcontext dummy;
    int ret = 0, eret = 0;

    ret = CHECK_CU(cuCtxPushCurrent(p->cuda_ctx));
    if (ret < 0)
        return ret;

    *out_frame = (struct gl_hwdec_frame) { 0, };

    for (int n = 0; n < 2; n++) {
        // widthInBytes must account for the chroma plane
        // elements being two bytes wide.
        CUDA_MEMCPY2D cpy = {
            .srcMemoryType = CU_MEMORYTYPE_DEVICE,
            .dstMemoryType = CU_MEMORYTYPE_ARRAY,
            .srcDevice     = (CUdeviceptr)hw_image->planes[n],
            .srcPitch      = hw_image->stride[n],
            .srcY          = 0,
            .dstArray      = p->cu_array[n],
            .WidthInBytes  = mp_image_plane_w(&p->layout, n) * (n + 1),
            .Height        = mp_image_plane_h(&p->layout, n),
        };
        ret = CHECK_CU(cuMemcpy2D(&cpy));
        if (ret < 0)
            goto error;

        out_frame->planes[n] = (struct gl_hwdec_plane){
            .gl_texture = p->gl_textures[n],
            .gl_target = GL_TEXTURE_2D,
            .tex_w = mp_image_plane_w(&p->layout, n),
            .tex_h = mp_image_plane_h(&p->layout, n),
        };
    }


 error:
   eret = CHECK_CU(cuCtxPopCurrent(&dummy));
   if (eret < 0)
       return eret;

   return ret;
}

const struct gl_hwdec_driver gl_hwdec_cuda = {
    .name = "cuda",
    .api = HWDEC_CUDA,
    .imgfmt = IMGFMT_CUDA,
    .create = cuda_create,
    .reinit = reinit,
    .map_frame = map_frame,
    .destroy = destroy,
};
