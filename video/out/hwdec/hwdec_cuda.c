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
 * or CUDA->Vulkan interop for frame data that is stored in CUDA
 * device memory. Although it is not explicit in the code here, the
 * only practical way to get data in this form is from the
 * nvdec/cuvid decoder.
 */

#include "config.h"
#include "hwdec_cuda.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>

int check_cu(const struct ra_hwdec *hw, CUresult err, const char *func)
{
    const char *err_name;
    const char *err_string;

    struct cuda_hw_priv *p = hw->priv;
    int level = hw->probing ? MSGL_V : MSGL_ERR;

    MP_TRACE(hw, "Calling %s\n", func);

    if (err == CUDA_SUCCESS)
        return 0;

    p->cu->cuGetErrorName(err, &err_name);
    p->cu->cuGetErrorString(err, &err_string);

    MP_MSG(hw, level, "%s failed", func);
    if (err_name && err_string)
        MP_MSG(hw, level, " -> %s: %s", err_name, err_string);
    MP_MSG(hw, level, "\n");

    return -1;
}

#define CHECK_CU(x) check_cu(hw, (x), #x)

static const struct cuda_interop_fn *interop_fns[] = {
#if HAVE_GL
    &cuda_gl_fn,
#endif
#if HAVE_VULKAN
    &cuda_vk_fn,
#endif
    NULL
};

static int cuda_init(struct ra_hwdec *hw)
{
    AVBufferRef *hw_device_ctx = NULL;
    CUcontext dummy;
    int ret = 0;
    struct cuda_hw_priv *p = hw->priv;
    CudaFunctions *cu = NULL;
    int level = hw->probing ? MSGL_V : MSGL_ERR;
    bool initialized = false;

    // Initialise CUDA context from backend.
    // Note that the interop check doesn't require the CUDA backend to be initialized.
    // This is important because cuInit wakes up the dgpu (even if the cuda hwdec won't be used!)
    // Doing this allows us to check if CUDA should be used without waking up the dgpu, avoiding
    // a few seconds of delay and improving battery life for laptops!
    for (int i = 0; interop_fns[i]; i++) {
        if (!interop_fns[i]->check(hw))
            continue;

        if (!initialized) {
            ret = cuda_load_functions(&p->cu, NULL);
            if (ret != 0) {
                MP_MSG(hw, level, "Failed to load CUDA symbols\n");
                return -1;
            }

            cu = p->cu;
            ret = CHECK_CU(cu->cuInit(0));
            if (ret < 0)
                return -1;

            initialized = true;
        }

        if (interop_fns[i]->init(hw))
            break;
    }

    if (!p->ext_init || !p->ext_uninit) {
        MP_MSG(hw, level,
               "CUDA hwdec only works with OpenGL or Vulkan backends.\n");
        return -1;
    }

    hw_device_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
    if (!hw_device_ctx)
        goto error;

    AVHWDeviceContext *device_ctx = (void *)hw_device_ctx->data;

    AVCUDADeviceContext *device_hwctx = device_ctx->hwctx;
    device_hwctx->cuda_ctx = p->decode_ctx;

    ret = av_hwdevice_ctx_init(hw_device_ctx);
    if (ret < 0) {
        MP_MSG(hw, level, "av_hwdevice_ctx_init failed\n");
        goto error;
    }

    ret = CHECK_CU(cu->cuCtxPopCurrent(&dummy));
    if (ret < 0)
        goto error;

    p->hwctx = (struct mp_hwdec_ctx) {
        .driver_name = hw->driver->name,
        .av_device_ref = hw_device_ctx,
        .hw_imgfmt = IMGFMT_CUDA,
    };
    hwdec_devices_add(hw->devs, &p->hwctx);
    return 0;

 error:
    av_buffer_unref(&hw_device_ctx);
    CHECK_CU(cu->cuCtxPopCurrent(&dummy));

    return -1;
}

static void cuda_uninit(struct ra_hwdec *hw)
{
    struct cuda_hw_priv *p = hw->priv;
    CudaFunctions *cu = p->cu;

    hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);

    if (p->decode_ctx && p->decode_ctx != p->display_ctx)
        CHECK_CU(cu->cuCtxDestroy(p->decode_ctx));

    if (p->display_ctx)
        CHECK_CU(cu->cuCtxDestroy(p->display_ctx));

    cuda_free_functions(&p->cu);
}

#undef CHECK_CU
#define CHECK_CU(x) check_cu((mapper)->owner, (x), #x)

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    struct cuda_mapper_priv *p = mapper->priv;
    CUcontext dummy;
    CudaFunctions *cu = p_owner->cu;
    int ret = 0, eret = 0;

    p->display_ctx = p_owner->display_ctx;

    int imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = imgfmt;
    mapper->dst_params.hw_subfmt = 0;

    mp_image_set_params(&p->layout, &mapper->dst_params);

    struct ra_imgfmt_desc desc;
    if (!ra_get_imgfmt_desc(mapper->ra, imgfmt, &desc)) {
        MP_ERR(mapper, "Unsupported format: %s\n", mp_imgfmt_to_name(imgfmt));
        return -1;
    }

    ret = CHECK_CU(cu->cuCtxPushCurrent(p->display_ctx));
    if (ret < 0)
        return ret;

    for (int n = 0; n < desc.num_planes; n++) {
        if (!p_owner->ext_init(mapper, desc.planes[n], n))
            goto error;
    }

 error:
    eret = CHECK_CU(cu->cuCtxPopCurrent(&dummy));
    if (eret < 0)
        return eret;

    return ret;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct cuda_mapper_priv *p = mapper->priv;
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    CudaFunctions *cu = p_owner->cu;
    CUcontext dummy;

    // Don't bail if any CUDA calls fail. This is all best effort.
    CHECK_CU(cu->cuCtxPushCurrent(p->display_ctx));
    for (int n = 0; n < 4; n++) {
        p_owner->ext_uninit(mapper, n);
        ra_tex_free(mapper->ra, &mapper->tex[n]);
    }
    CHECK_CU(cu->cuCtxPopCurrent(&dummy));
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct cuda_mapper_priv *p = mapper->priv;
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    CudaFunctions *cu = p_owner->cu;
    CUcontext dummy;
    int ret = 0, eret = 0;

    ret = CHECK_CU(cu->cuCtxPushCurrent(p->display_ctx));
    if (ret < 0)
        return ret;

    for (int n = 0; n < p->layout.num_planes; n++) {
        if (p_owner->ext_wait) {
            if (!p_owner->ext_wait(mapper, n))
                goto error;
        }

        CUDA_MEMCPY2D cpy = {
            .srcMemoryType = CU_MEMORYTYPE_DEVICE,
            .srcDevice     = (CUdeviceptr)mapper->src->planes[n],
            .srcPitch      = mapper->src->stride[n],
            .srcY          = 0,
            .dstMemoryType = CU_MEMORYTYPE_ARRAY,
            .dstArray      = p->cu_array[n],
            .WidthInBytes  = mp_image_plane_w(&p->layout, n) *
                             mapper->tex[n]->params.format->pixel_size,
            .Height        = mp_image_plane_h(&p->layout, n),
        };

        ret = CHECK_CU(cu->cuMemcpy2DAsync(&cpy, 0));
        if (ret < 0)
            goto error;

        if (p_owner->ext_signal) {
            if (!p_owner->ext_signal(mapper, n))
                goto error;
        }
    }
    if (p_owner->do_full_sync)
        CHECK_CU(cu->cuStreamSynchronize(0));

    // fall through
 error:

    // Regardless of success or failure, we no longer need the source image,
    // because this hwdec makes an explicit memcpy into the mapper textures
    mp_image_unrefp(&mapper->src);

    eret = CHECK_CU(cu->cuCtxPopCurrent(&dummy));
    if (eret < 0)
        return eret;

    return ret;
}

const struct ra_hwdec_driver ra_hwdec_cuda = {
    .name = "cuda",
    .imgfmts = {IMGFMT_CUDA, 0},
    .device_type = AV_HWDEVICE_TYPE_CUDA,
    .priv_size = sizeof(struct cuda_hw_priv),
    .init = cuda_init,
    .uninit = cuda_uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct cuda_mapper_priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
