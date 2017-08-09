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

#include "cuda_dynamic.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>

#include "formats.h"
#include "hwdec.h"
#include "options/m_config.h"
#include "video.h"

struct priv {
    struct mp_hwdec_ctx hwctx;
    struct mp_image layout;
    GLuint gl_textures[4];
    CUgraphicsResource cu_res[4];
    CUarray cu_array[4];
    int plane_bytes[4];

    CUcontext display_ctx;
    CUcontext decode_ctx;
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

static int cuda_create(struct gl_hwdec *hw)
{
    CUdevice display_dev;
    AVBufferRef *hw_device_ctx = NULL;
    CUcontext dummy;
    unsigned int device_count;
    int ret = 0;

    if (hw->gl->version < 210 && hw->gl->es < 300) {
        MP_VERBOSE(hw, "need OpenGL >= 2.1 or OpenGL-ES >= 3.0\n");
        return -1;
    }

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    bool loaded = cuda_load();
    if (!loaded) {
        MP_VERBOSE(hw, "Failed to load CUDA symbols\n");
        return -1;
    }

    ret = CHECK_CU(cuInit(0));
    if (ret < 0)
        goto error;

    // Allocate display context
    ret = CHECK_CU(cuGLGetDevices(&device_count, &display_dev, 1,
                                  CU_GL_DEVICE_LIST_ALL));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cuCtxCreate(&p->display_ctx, CU_CTX_SCHED_BLOCKING_SYNC,
                               display_dev));
    if (ret < 0)
        goto error;

    p->decode_ctx = p->display_ctx;

    int decode_dev_idx = -1;
    mp_read_option_raw(hw->global, "cuda-decode-device", &m_option_type_choice,
                       &decode_dev_idx);

    if (decode_dev_idx > -1) {
        CUdevice decode_dev;
        ret = CHECK_CU(cuDeviceGet(&decode_dev, decode_dev_idx));
        if (ret < 0)
            goto error;

        if (decode_dev != display_dev) {
            MP_INFO(hw, "Using separate decoder and display devices\n");

            // Pop the display context. We won't use it again during init()
            ret = CHECK_CU(cuCtxPopCurrent(&dummy));
            if (ret < 0)
                goto error;

            ret = CHECK_CU(cuCtxCreate(&p->decode_ctx, CU_CTX_SCHED_BLOCKING_SYNC,
                                       decode_dev));
            if (ret < 0)
                goto error;
        }
    }

    hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
    if (!hw_device_ctx)
        goto error;

    AVHWDeviceContext *device_ctx = (void *)hw_device_ctx->data;

    AVCUDADeviceContext *device_hwctx = device_ctx->hwctx;
    device_hwctx->cuda_ctx = p->decode_ctx;

    ret = av_hwdevice_ctx_init(hw_device_ctx);
    if (ret < 0) {
        MP_ERR(hw, "av_hwdevice_ctx_init failed\n");
        goto error;
    }

    ret = CHECK_CU(cuCtxPopCurrent(&dummy));
    if (ret < 0)
        goto error;

    p->hwctx = (struct mp_hwdec_ctx) {
        .type = HWDEC_CUDA,
        .ctx = p->decode_ctx,
        .av_device_ref = hw_device_ctx,
    };
    p->hwctx.driver_name = hw->driver->name;
    hwdec_devices_add(hw->devs, &p->hwctx);
    return 0;

 error:
    av_buffer_unref(&hw_device_ctx);
    CHECK_CU(cuCtxPopCurrent(&dummy));

    return -1;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;
    CUcontext dummy;
    int ret = 0, eret = 0;

    assert(params->imgfmt == hw->driver->imgfmt);
    params->imgfmt = params->hw_subfmt;
    params->hw_subfmt = 0;

    mp_image_set_params(&p->layout, params);

    struct gl_imgfmt_desc desc;
    if (!gl_get_imgfmt_desc(gl, params->imgfmt, &desc)) {
        MP_ERR(hw, "Unsupported format: %s\n", mp_imgfmt_to_name(params->imgfmt));
        return -1;
    }

    ret = CHECK_CU(cuCtxPushCurrent(p->display_ctx));
    if (ret < 0)
        return ret;

    gl->GenTextures(4, p->gl_textures);
    for (int n = 0; n < desc.num_planes; n++) {
        const struct gl_format *fmt = desc.planes[n];

        p->plane_bytes[n] = gl_bytes_per_pixel(fmt->format, fmt->type);

        gl->BindTexture(GL_TEXTURE_2D, p->gl_textures[n]);
        GLenum filter = GL_LINEAR;
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->TexImage2D(GL_TEXTURE_2D, 0, fmt->internal_format,
                       mp_image_plane_w(&p->layout, n),
                       mp_image_plane_h(&p->layout, n),
                       0, fmt->format, fmt->type, NULL);
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
    CHECK_CU(cuCtxPushCurrent(p->display_ctx));
    for (int n = 0; n < 4; n++) {
        if (p->cu_res[n] > 0)
            CHECK_CU(cuGraphicsUnregisterResource(p->cu_res[n]));
        p->cu_res[n] = 0;
    }
    CHECK_CU(cuCtxPopCurrent(&dummy));

    if (p->decode_ctx != p->display_ctx) {
        CHECK_CU(cuCtxDestroy(p->decode_ctx));
    }

    CHECK_CU(cuCtxDestroy(p->display_ctx));

    gl->DeleteTextures(4, p->gl_textures);

    hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);
}

static int map_frame(struct gl_hwdec *hw, struct mp_image *hw_image,
                     struct gl_hwdec_frame *out_frame)
{
    struct priv *p = hw->priv;
    CUcontext dummy;
    int ret = 0, eret = 0;

    ret = CHECK_CU(cuCtxPushCurrent(p->display_ctx));
    if (ret < 0)
        return ret;

    *out_frame = (struct gl_hwdec_frame) { 0, };

    for (int n = 0; n < p->layout.num_planes; n++) {
        // widthInBytes must account for the chroma plane
        // elements being two samples wide.
        CUDA_MEMCPY2D cpy = {
            .srcMemoryType = CU_MEMORYTYPE_DEVICE,
            .dstMemoryType = CU_MEMORYTYPE_ARRAY,
            .srcDevice     = (CUdeviceptr)hw_image->planes[n],
            .srcPitch      = hw_image->stride[n],
            .srcY          = 0,
            .dstArray      = p->cu_array[n],
            .WidthInBytes  = mp_image_plane_w(&p->layout, n) * p->plane_bytes[n],
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
