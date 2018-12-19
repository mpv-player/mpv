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

#include <unistd.h>

#include <ffnvcodec/dynlink_loader.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>

#include "video/out/gpu/hwdec.h"
#include "video/out/gpu/utils.h"
#include "options/m_config.h"
#if HAVE_GL
#include "video/out/opengl/formats.h"
#include "video/out/opengl/ra_gl.h"
#endif
#if HAVE_VULKAN
#include "video/out/placebo/ra_pl.h"
#endif

#if HAVE_WIN32_DESKTOP
#include <versionhelpers.h>
#endif

struct priv_owner {
    struct mp_hwdec_ctx hwctx;
    CudaFunctions *cu;
    CUcontext display_ctx;
    CUcontext decode_ctx;

    bool is_gl;
    bool is_vk;

    bool (*ext_init)(struct ra_hwdec_mapper *mapper,
                     const struct ra_format *format, int n);
    void (*ext_uninit)(struct ra_hwdec_mapper *mapper, int n);

    enum pl_handle_type handle_type;
};

struct ext_gl {
#if HAVE_GL
    CUgraphicsResource cu_res;
#endif
};

struct ext_vk {
#if HAVE_VULKAN
    CUexternalMemory mem;
    CUmipmappedArray mma;

    const struct pl_tex *pltex;

    const struct pl_sync *sync;

    CUexternalSemaphore ss;
    CUexternalSemaphore ws;
#endif
};

struct priv {
    struct mp_image layout;
    CUarray cu_array[4];

    CUcontext display_ctx;

    struct ext_gl egl[4];
    struct ext_vk evk[4];
};

static int check_cu(struct ra_hwdec *hw, CUresult err, const char *func)
{
    const char *err_name;
    const char *err_string;

    struct priv_owner *p = hw->priv;

    MP_TRACE(hw, "Calling %s\n", func);

    if (err == CUDA_SUCCESS)
        return 0;

    p->cu->cuGetErrorName(err, &err_name);
    p->cu->cuGetErrorString(err, &err_string);

    MP_ERR(hw, "%s failed", func);
    if (err_name && err_string)
        MP_ERR(hw, " -> %s: %s", err_name, err_string);
    MP_ERR(hw, "\n");

    return -1;
}

#define CHECK_CU(x) check_cu(hw, (x), #x)

static bool cuda_ext_gl_init(struct ra_hwdec_mapper *mapper,
                             const struct ra_format *format, int n);
static bool cuda_ext_vk_init(struct ra_hwdec_mapper *mapper,
                             const struct ra_format *format, int n);
static void cuda_ext_gl_uninit(struct ra_hwdec_mapper *mapper, int n);
static void cuda_ext_vk_uninit(struct ra_hwdec_mapper *mapper, int n);

static bool gl_init(struct ra_hwdec *hw) {
#if HAVE_GL
    int ret = 0;
    struct priv_owner *p = hw->priv;
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

    int decode_dev_idx = -1;
    mp_read_option_raw(hw->global, "cuda-decode-device", &m_option_type_choice,
                       &decode_dev_idx);

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

    p->ext_init = cuda_ext_gl_init;
    p->ext_uninit = cuda_ext_gl_uninit;

    return true;
#else
    return false;
#endif
}

static bool vk_init(struct ra_hwdec *hw) {
#if HAVE_VULKAN
    int ret = 0;
    struct priv_owner *p = hw->priv;
    CudaFunctions *cu = p->cu;

    int device_count;
    ret = CHECK_CU(cu->cuDeviceGetCount(&device_count));
    if (ret < 0)
        return false;

    CUdevice display_dev = -1;
    for (int i = 0; i < device_count; i++) {
        CUdevice dev;
        ret = CHECK_CU(cu->cuDeviceGet(&dev, i));
        if (ret < 0)
            continue;

        CUuuid uuid;
        ret = CHECK_CU(cu->cuDeviceGetUuid(&uuid, dev));
        if (ret < 0)
            continue;

        const struct pl_gpu *gpu = ra_pl_get(hw->ra);
        if (memcmp(gpu->uuid, uuid.bytes, sizeof (gpu->uuid)) == 0) {
            display_dev = dev;
            break;
        }
    }

    if (display_dev == -1) {
        MP_ERR(hw, "Could not match Vulkan display device in CUDA.\n");
        return false;
    }

    ret = CHECK_CU(cu->cuCtxCreate(&p->display_ctx, CU_CTX_SCHED_BLOCKING_SYNC,
                                   display_dev));
    if (ret < 0)
        return false;

    p->decode_ctx = p->display_ctx;

    p->ext_init = cuda_ext_vk_init;
    p->ext_uninit = cuda_ext_vk_uninit;

    return true;
#else
    return false;
#endif
}

static int cuda_init(struct ra_hwdec *hw)
{
    AVBufferRef *hw_device_ctx = NULL;
    CUcontext dummy;
    int ret = 0;
    struct priv_owner *p = hw->priv;
    CudaFunctions *cu;

#if HAVE_GL
    p->is_gl = ra_is_gl(hw->ra);
    if (p->is_gl) {
        GL *gl = ra_gl_get(hw->ra);
        if (gl->version < 210 && gl->es < 300) {
            MP_VERBOSE(hw, "need OpenGL >= 2.1 or OpenGL-ES >= 3.0\n");
            return -1;
        }
    }
#endif

#if HAVE_VULKAN
    p->handle_type =
#if HAVE_WIN32_DESKTOP
        IsWindows8OrGreater() ? PL_HANDLE_WIN32 : PL_HANDLE_WIN32_KMT;
#else
        PL_HANDLE_FD;
#endif

    const struct pl_gpu *gpu = ra_pl_get(hw->ra);
    p->is_vk = gpu != NULL;
    if (p->is_vk) {
        if (!(gpu->export_caps.tex & p->handle_type)) {
            MP_VERBOSE(hw, "CUDA hwdec with Vulkan requires exportable texture memory of type 0x%X.\n",
                       p->handle_type);
            return -1;
        } else if (!(gpu->export_caps.sync & p->handle_type)) {
            MP_VERBOSE(hw, "CUDA hwdec with Vulkan requires exportable semaphores of type 0x%X.\n",
                       p->handle_type);
            return -1;
        }
    }
#endif

    if (!p->is_gl && !p->is_vk) {
        MP_VERBOSE(hw, "CUDA hwdec only works with OpenGL or Vulkan backends.\n");
        return -1;
    }

    ret = cuda_load_functions(&p->cu, NULL);
    if (ret != 0) {
        MP_VERBOSE(hw, "Failed to load CUDA symbols\n");
        return -1;
    }
    cu = p->cu;

    if (p->is_vk && !cu->cuImportExternalMemory) {
        MP_ERR(hw, "CUDA hwdec with Vulkan requires driver version 410.48 or newer.\n");
        return -1;
    }

    ret = CHECK_CU(cu->cuInit(0));
    if (ret < 0)
        return -1;

    // Allocate display context
    if (p->is_gl && !gl_init(hw)) {
        return -1;
    } else if (p->is_vk && !vk_init(hw)) {
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
        MP_ERR(hw, "av_hwdevice_ctx_init failed\n");
        goto error;
    }

    ret = CHECK_CU(cu->cuCtxPopCurrent(&dummy));
    if (ret < 0)
        goto error;

    p->hwctx = (struct mp_hwdec_ctx) {
        .driver_name = hw->driver->name,
        .av_device_ref = hw_device_ctx,
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
    struct priv_owner *p = hw->priv;
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

#if HAVE_VULKAN
static bool cuda_ext_vk_init(struct ra_hwdec_mapper *mapper,
                             const struct ra_format *format, int n)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;
    int mem_fd = -1, wait_fd = -1, signal_fd = -1;
    int ret = 0;

    struct ext_vk *evk = &p->evk[n];

    const struct pl_gpu *gpu = ra_pl_get(mapper->ra);

    struct pl_tex_params tex_params = {
        .w = mp_image_plane_w(&p->layout, n),
        .h = mp_image_plane_h(&p->layout, n),
        .d = 0,
        .format = ra_pl_fmt_get(format),
        .sampleable = true,
        .sample_mode = format->linear_filter ? PL_TEX_SAMPLE_LINEAR
                                             : PL_TEX_SAMPLE_NEAREST,
        .export_handle = p_owner->handle_type,
    };

    evk->pltex = pl_tex_create(gpu, &tex_params);
    if (!evk->pltex) {
        goto error;
    }

    struct ra_tex *ratex = talloc_ptrtype(NULL, ratex);
    ret = mppl_wrap_tex(mapper->ra, evk->pltex, ratex);
    if (!ret) {
        pl_tex_destroy(gpu, &evk->pltex);
        talloc_free(ratex);
        goto error;
    }
    mapper->tex[n] = ratex;

#if !HAVE_WIN32_DESKTOP
    if (evk->pltex->params.export_handle == PL_HANDLE_FD) {
        mem_fd = dup(evk->pltex->shared_mem.handle.fd);
        if (mem_fd < 0) {
            goto error;
        }
    }
#endif

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC ext_desc = {
#if HAVE_WIN32_DESKTOP
        .type = IsWindows8OrGreater()
            ? CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32
            : CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT,
        .handle.win32.handle = evk->pltex->shared_mem.handle.handle,
#else
        .type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD,
        .handle.fd = mem_fd,
#endif
        .size = evk->pltex->shared_mem.size,
        .flags = 0,
    };
    ret = CHECK_CU(cu->cuImportExternalMemory(&evk->mem, &ext_desc));
    if (ret < 0)
        goto error;
    // CUDA takes ownership of imported memory
    mem_fd = -1;

    CUarray_format cufmt;
    switch (format->pixel_size / format->num_components) {
    case 1:
        cufmt = CU_AD_FORMAT_UNSIGNED_INT8;
        break;
    case 2:
        cufmt = CU_AD_FORMAT_UNSIGNED_INT16;
        break;
    default:
        ret = -1;
        goto error;
    }

    CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC tex_desc = {
        .offset = evk->pltex->shared_mem.offset,
        .arrayDesc = {
            .Width = mp_image_plane_w(&p->layout, n),
            .Height = mp_image_plane_h(&p->layout, n),
            .Depth = 0,
            .Format = cufmt,
            .NumChannels = format->num_components,
            .Flags = 0,
        },
        .numLevels = 1,
    };

    ret = CHECK_CU(cu->cuExternalMemoryGetMappedMipmappedArray(&evk->mma, evk->mem, &tex_desc));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cu->cuMipmappedArrayGetLevel(&p->cu_array[n], evk->mma, 0));
    if (ret < 0)
        goto error;

    evk->sync = pl_sync_create(gpu, p_owner->handle_type);
    if (!evk->sync) {
        ret = -1;
        goto error;
    }

#if !HAVE_WIN32_DESKTOP
    if (evk->sync->handle_type == PL_HANDLE_FD) {
        wait_fd = dup(evk->sync->wait_handle.fd);
        signal_fd = dup(evk->sync->signal_handle.fd);
    }
#endif

    CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC w_desc = {
#if HAVE_WIN32_DESKTOP
        .type = IsWindows8OrGreater()
            ? CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32
            : CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT,
        .handle.win32.handle = evk->sync->wait_handle.handle,
#else
        .type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD,
        .handle.fd = wait_fd,
#endif
    };
    ret = CHECK_CU(cu->cuImportExternalSemaphore(&evk->ws, &w_desc));
    if (ret < 0)
        goto error;
    wait_fd = -1;

    CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC s_desc = {
#if HAVE_WIN32_DESKTOP
        .type = IsWindows8OrGreater()
            ? CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32
            : CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT,
        .handle.win32.handle = evk->sync->signal_handle.handle,
#else
        .type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD,
        .handle.fd = signal_fd,
#endif
    };

    ret = CHECK_CU(cu->cuImportExternalSemaphore(&evk->ss, &s_desc));
    if (ret < 0)
        goto error;
    signal_fd = -1;

    return true;

error:
    MP_ERR(mapper, "cuda_ext_vk_init failed\n");
    if (mem_fd > -1)
        close(mem_fd);
    if (wait_fd > -1)
        close(wait_fd);
    if (signal_fd > -1)
        close(signal_fd);
    return false;
}

static void cuda_ext_vk_uninit(struct ra_hwdec_mapper *mapper, int n)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;

    struct ext_vk *evk = &p->evk[n];
    if (evk->mma) {
        CHECK_CU(cu->cuMipmappedArrayDestroy(evk->mma));
        evk->mma = 0;
    }
    if (evk->mem) {
        CHECK_CU(cu->cuDestroyExternalMemory(evk->mem));
        evk->mem = 0;
    }
    if (evk->ss) {
        CHECK_CU(cu->cuDestroyExternalSemaphore(evk->ss));
        evk->ss = 0;
    }
    if (evk->ws) {
        CHECK_CU(cu->cuDestroyExternalSemaphore(evk->ws));
        evk->ws = 0;
    }
    pl_sync_destroy(ra_pl_get(mapper->ra), &evk->sync);
}
#endif // HAVE_VULKAN

#if HAVE_GL
static bool cuda_ext_gl_init(struct ra_hwdec_mapper *mapper,
                             const struct ra_format *format, int n)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;
    int ret = 0;
    CUcontext dummy;

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

    ret = CHECK_CU(cu->cuGraphicsGLRegisterImage(&p->egl[n].cu_res, texture, target,
                                                 CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cu->cuGraphicsMapResources(1, &p->egl[n].cu_res, 0));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cu->cuGraphicsSubResourceGetMappedArray(&p->cu_array[n], p->egl[n].cu_res,
                                                           0, 0));
    if (ret < 0)
        goto error;

    ret = CHECK_CU(cu->cuGraphicsUnmapResources(1, &p->egl[n].cu_res, 0));
    if (ret < 0)
        goto error;

    return true;

error:
    CHECK_CU(cu->cuCtxPopCurrent(&dummy));
    return false;
}

static void cuda_ext_gl_uninit(struct ra_hwdec_mapper *mapper, int n)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;

    struct ext_gl *egl = &p->egl[n];
    if (egl->cu_res) {
        CHECK_CU(cu->cuGraphicsUnregisterResource(p->egl[n].cu_res));
        egl->cu_res = 0;
    }
}
#endif // HAVE_GL

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
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
    struct priv *p = mapper->priv;
    struct priv_owner *p_owner = mapper->owner->priv;
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
    struct priv *p = mapper->priv;
    struct priv_owner *p_owner = mapper->owner->priv;
    CudaFunctions *cu = p_owner->cu;
    CUcontext dummy;
    int ret = 0, eret = 0;

    ret = CHECK_CU(cu->cuCtxPushCurrent(p->display_ctx));
    if (ret < 0)
        return ret;

    for (int n = 0; n < p->layout.num_planes; n++) {
#if HAVE_VULKAN
        if (p_owner->is_vk) {
            ret = pl_tex_export(ra_pl_get(mapper->ra),
                                p->evk[n].pltex, p->evk[n].sync);
            if (!ret)
                goto error;

            CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS wp = { 0, };
            ret = CHECK_CU(cu->cuWaitExternalSemaphoresAsync(&p->evk[n].ws,
                                                             &wp, 1, 0));
            if (ret < 0)
                goto error;
        }
#endif
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
#if HAVE_VULKAN
        if (p_owner->is_vk) {
            CUDA_EXTERNAL_SEMAPHORE_SIGNAL_PARAMS sp = { 0, };
            ret = CHECK_CU(cu->cuSignalExternalSemaphoresAsync(&p->evk[n].ss,
                                                               &sp, 1, 0));
            if (ret < 0)
                goto error;
        }
#endif
    }

 error:
   eret = CHECK_CU(cu->cuCtxPopCurrent(&dummy));
   if (eret < 0)
       return eret;

   return ret;
}

const struct ra_hwdec_driver ra_hwdec_cuda = {
    .name = "cuda-nvdec",
    .imgfmts = {IMGFMT_CUDA, 0},
    .priv_size = sizeof(struct priv_owner),
    .init = cuda_init,
    .uninit = cuda_uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
