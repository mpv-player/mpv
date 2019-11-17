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

#include "config.h"
#include "hwdec_cuda.h"
#include "video/out/placebo/ra_pl.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <unistd.h>

#if HAVE_WIN32_DESKTOP
#include <versionhelpers.h>
#endif

#define CHECK_CU(x) check_cu((mapper)->owner, (x), #x)

struct ext_vk {
    CUexternalMemory mem;
    CUmipmappedArray mma;

    const struct pl_tex *pltex;

    const struct pl_sync *sync;

    CUexternalSemaphore ss;
    CUexternalSemaphore ws;
};

static bool cuda_ext_vk_init(struct ra_hwdec_mapper *mapper,
                             const struct ra_format *format, int n)
{
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    struct cuda_mapper_priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;
    int mem_fd = -1, wait_fd = -1, signal_fd = -1;
    int ret = 0;

    struct ext_vk *evk = talloc_ptrtype(NULL, evk);
    p->ext[n] = evk;

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

static void cuda_ext_vk_uninit(const struct ra_hwdec_mapper *mapper, int n)
{
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    struct cuda_mapper_priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;

    struct ext_vk *evk = p->ext[n];
    if (evk) {
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
    talloc_free(evk);
}

static bool cuda_ext_vk_wait(const struct ra_hwdec_mapper *mapper, int n)
{
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    struct cuda_mapper_priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;
    int ret;
    struct ext_vk *evk = p->ext[n];

    ret = pl_tex_export(ra_pl_get(mapper->ra),
                        evk->pltex, evk->sync);
    if (!ret)
        return false;

    CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS wp = { 0, };
    ret = CHECK_CU(cu->cuWaitExternalSemaphoresAsync(&evk->ws,
                                                     &wp, 1, 0));
    return ret == 0;
}

static bool cuda_ext_vk_signal(const struct ra_hwdec_mapper *mapper, int n)
{
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    struct cuda_mapper_priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;
    int ret;
    struct ext_vk *evk = p->ext[n];

    CUDA_EXTERNAL_SEMAPHORE_SIGNAL_PARAMS sp = { 0, };
    ret = CHECK_CU(cu->cuSignalExternalSemaphoresAsync(&evk->ss,
                                                       &sp, 1, 0));
    return ret == 0;
}

#undef CHECK_CU
#define CHECK_CU(x) check_cu(hw, (x), #x)

bool cuda_vk_init(const struct ra_hwdec *hw) {
    int ret = 0;
    int level = hw->probing ? MSGL_V : MSGL_ERR;
    struct cuda_hw_priv *p = hw->priv;
    CudaFunctions *cu = p->cu;

    p->handle_type =
#if HAVE_WIN32_DESKTOP
        IsWindows8OrGreater() ? PL_HANDLE_WIN32 : PL_HANDLE_WIN32_KMT;
#else
        PL_HANDLE_FD;
#endif

    const struct pl_gpu *gpu = ra_pl_get(hw->ra);
    if (gpu != NULL) {
        if (!(gpu->export_caps.tex & p->handle_type)) {
            MP_VERBOSE(hw, "CUDA hwdec with Vulkan requires exportable texture memory of type 0x%X.\n",
                       p->handle_type);
            return false;
        } else if (!(gpu->export_caps.sync & p->handle_type)) {
            MP_VERBOSE(hw, "CUDA hwdec with Vulkan requires exportable semaphores of type 0x%X.\n",
                       p->handle_type);
            return false;
        }
    } else {
        // This is not a Vulkan RA.
        return false;
    }

    if (!cu->cuImportExternalMemory) {
        MP_MSG(hw, level, "CUDA hwdec with Vulkan requires driver version 410.48 or newer.\n");
        return false;
    }

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

        if (memcmp(gpu->uuid, uuid.bytes, sizeof (gpu->uuid)) == 0) {
            display_dev = dev;
            break;
        }
    }

    if (display_dev == -1) {
        MP_MSG(hw, level, "Could not match Vulkan display device in CUDA.\n");
        return false;
    }

    ret = CHECK_CU(cu->cuCtxCreate(&p->display_ctx, CU_CTX_SCHED_BLOCKING_SYNC,
                                   display_dev));
    if (ret < 0)
        return false;

    p->decode_ctx = p->display_ctx;

    p->ext_init = cuda_ext_vk_init;
    p->ext_uninit = cuda_ext_vk_uninit;
    p->ext_wait = cuda_ext_vk_wait;
    p->ext_signal = cuda_ext_vk_signal;

    return true;
}

