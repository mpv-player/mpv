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
#include <libplacebo/vulkan.h>

#if HAVE_WIN32_DESKTOP
#include <versionhelpers.h>
#define HANDLE_TYPE PL_HANDLE_WIN32
#else
#define HANDLE_TYPE PL_HANDLE_FD
#endif

#define CHECK_CU(x) check_cu((mapper)->owner, (x), #x)

struct ext_vk {
    CUexternalMemory mem;
    CUmipmappedArray mma;

    pl_tex pltex;
    pl_vulkan_sem vk_sem;
    union pl_handle sem_handle;
    CUexternalSemaphore cuda_sem;
};

static bool cuda_ext_vk_init(struct ra_hwdec_mapper *mapper,
                             const struct ra_format *format, int n)
{
    struct cuda_hw_priv *p_owner = mapper->owner->priv;
    struct cuda_mapper_priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;
    int mem_fd = -1;
    int ret = 0;

    struct ext_vk *evk = talloc_ptrtype(NULL, evk);
    p->ext[n] = evk;

    pl_gpu gpu = ra_pl_get(mapper->ra);

    struct pl_tex_params tex_params = {
        .w = mp_image_plane_w(&p->layout, n),
        .h = mp_image_plane_h(&p->layout, n),
        .d = 0,
        .format = ra_pl_fmt_get(format),
        .sampleable = true,
        .export_handle = HANDLE_TYPE,
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
    mem_fd = dup(evk->pltex->shared_mem.handle.fd);
    if (mem_fd < 0)
        goto error;
#endif

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC ext_desc = {
#if HAVE_WIN32_DESKTOP
        .type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32,
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

    evk->vk_sem.sem = pl_vulkan_sem_create(gpu, pl_vulkan_sem_params(
        .type = VK_SEMAPHORE_TYPE_TIMELINE,
        .export_handle = HANDLE_TYPE,
        .out_handle = &(evk->sem_handle),
    ));
    if (evk->vk_sem.sem == VK_NULL_HANDLE) {
         ret = -1;
         goto error;
     }
     // The returned FD or Handle is owned by the caller (us).

    CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC w_desc = {
#if HAVE_WIN32_DESKTOP
        .type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TIMELINE_SEMAPHORE_WIN32,
        .handle.win32.handle = evk->sem_handle.handle,
#else
        .type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TIMELINE_SEMAPHORE_FD,
        .handle.fd = evk->sem_handle.fd,
#endif
    };
    ret = CHECK_CU(cu->cuImportExternalSemaphore(&evk->cuda_sem, &w_desc));
    if (ret < 0)
        goto error;
    // CUDA takes ownership of an imported FD *but not* an imported Handle.
    evk->sem_handle.fd = -1;

    return true;

error:
    MP_ERR(mapper, "cuda_ext_vk_init failed\n");
    if (mem_fd > -1)
        close(mem_fd);
#if HAVE_WIN32_DESKTOP
    if (evk->sem_handle.handle != NULL)
        CloseHandle(evk->sem_handle.handle);
#else
    if (evk->sem_handle.fd > -1)
        close(evk->sem_handle.fd);
#endif
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
        if (evk->cuda_sem) {
            CHECK_CU(cu->cuDestroyExternalSemaphore(evk->cuda_sem));
            evk->cuda_sem = 0;
        }
        pl_vulkan_sem_destroy(ra_pl_get(mapper->ra), &evk->vk_sem.sem);
#if HAVE_WIN32_DESKTOP
        CloseHandle(evk->sem_handle.handle);
#endif
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

    evk->vk_sem.value += 1;
    ret = pl_vulkan_hold_ex(ra_pl_get(mapper->ra), pl_vulkan_hold_params(
        .tex = evk->pltex,
        .layout = VK_IMAGE_LAYOUT_GENERAL,
        .qf = VK_QUEUE_FAMILY_EXTERNAL,
        .semaphore = evk->vk_sem,
    ));
    if (!ret)
        return false;

    CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS wp = {
        .params = {
            .fence = {
                .value = evk->vk_sem.value
            }
        }
     };
     ret = CHECK_CU(cu->cuWaitExternalSemaphoresAsync(&evk->cuda_sem,
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

    evk->vk_sem.value += 1;
    CUDA_EXTERNAL_SEMAPHORE_SIGNAL_PARAMS sp = {
        .params = {
            .fence = {
                .value = evk->vk_sem.value
            }
        }
    };
    ret = CHECK_CU(cu->cuSignalExternalSemaphoresAsync(&evk->cuda_sem,
                                                       &sp, 1, 0));
    if (ret != 0)
        return false;

    pl_vulkan_release_ex(ra_pl_get(mapper->ra), pl_vulkan_release_params(
        .tex = evk->pltex,
        .layout = VK_IMAGE_LAYOUT_GENERAL,
        .qf = VK_QUEUE_FAMILY_EXTERNAL,
        .semaphore = evk->vk_sem,
    ));
    return ret == 0;
}

#undef CHECK_CU
#define CHECK_CU(x) check_cu(hw, (x), #x)

static bool cuda_vk_check(const struct ra_hwdec *hw) {
    pl_gpu gpu = ra_pl_get(hw->ra_ctx->ra);
    if (gpu == NULL)
        return false; // This is not a Vulkan RA.

    if (!(gpu->export_caps.tex & HANDLE_TYPE)) {
        MP_VERBOSE(hw, "CUDA hwdec with Vulkan requires exportable texture memory of type 0x%X.\n",
                   HANDLE_TYPE);
        return false;
    } else if (!(gpu->export_caps.sync & HANDLE_TYPE)) {
        MP_VERBOSE(hw, "CUDA hwdec with Vulkan requires exportable semaphores of type 0x%X.\n",
                   HANDLE_TYPE);
        return false;
    }

    return true;
}

static bool cuda_vk_init(const struct ra_hwdec *hw) {
    int ret = 0;
    int level = hw->probing ? MSGL_V : MSGL_ERR;
    struct cuda_hw_priv *p = hw->priv;
    CudaFunctions *cu = p->cu;
    pl_gpu gpu = ra_pl_get(hw->ra_ctx->ra);

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

struct cuda_interop_fn cuda_vk_fn = {
    .check = cuda_vk_check,
    .init = cuda_vk_init
};
