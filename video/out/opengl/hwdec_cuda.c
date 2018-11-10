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
#include "formats.h"
#include "options/m_config.h"
#if HAVE_GL
#include "ra_gl.h"
#endif
#if HAVE_VULKAN
#include "video/out/vulkan/formats.h"
#include "video/out/vulkan/ra_vk.h"
#include "video/out/vulkan/utils.h"
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
};

struct ext_vk {
#if HAVE_WIN32_DESKTOP
    HANDLE handle;
#else
    int fd;
#endif
    CUexternalMemory mem;
    CUdeviceptr buf;

    struct ra_buf *rbuf;

    struct vk_external_semaphore signal;
    CUexternalSemaphore ss;

    struct vk_external_semaphore wait;
    CUexternalSemaphore ws;
};

struct priv {
    struct mp_image layout;
    CUgraphicsResource cu_res[4];
    CUarray cu_array[4];

    CUcontext display_ctx;

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

static int cuda_init(struct ra_hwdec *hw)
{
    CUdevice display_dev;
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
    p->is_vk = ra_vk_get(hw->ra) != NULL;
    if (p->is_vk) {
        if (!ra_vk_get(hw->ra)->has_ext_external_memory_export) {
            MP_VERBOSE(hw, "CUDA hwdec with Vulkan requires the %s extension\n",
                       MP_VK_EXTERNAL_MEMORY_EXPORT_EXTENSION_NAME);
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
    if (p->is_gl) {
        unsigned int device_count;
        ret = CHECK_CU(cu->cuGLGetDevices(&device_count, &display_dev, 1,
                                          CU_GL_DEVICE_LIST_ALL));
        if (ret < 0)
            return -1;

        ret = CHECK_CU(cu->cuCtxCreate(&p->display_ctx, CU_CTX_SCHED_BLOCKING_SYNC,
                                       display_dev));
        if (ret < 0)
            return -1;

        p->decode_ctx = p->display_ctx;

        int decode_dev_idx = -1;
        mp_read_option_raw(hw->global, "cuda-decode-device", &m_option_type_choice,
                           &decode_dev_idx);

        if (decode_dev_idx > -1) {
            CUdevice decode_dev;
            ret = CHECK_CU(cu->cuDeviceGet(&decode_dev, decode_dev_idx));
            if (ret < 0)
                goto error;

            if (decode_dev != display_dev) {
                MP_INFO(hw, "Using separate decoder and display devices\n");

                // Pop the display context. We won't use it again during init()
                ret = CHECK_CU(cu->cuCtxPopCurrent(&dummy));
                if (ret < 0)
                    return -1;

                ret = CHECK_CU(cu->cuCtxCreate(&p->decode_ctx, CU_CTX_SCHED_BLOCKING_SYNC,
                                               decode_dev));
                if (ret < 0)
                    return -1;
            }
        }
    } else if (p->is_vk) {
#if HAVE_VULKAN
        uint8_t vk_uuid[VK_UUID_SIZE];
        struct mpvk_ctx *vk = ra_vk_get(hw->ra);

        mpvk_get_phys_device_uuid(vk, vk_uuid);

        int count;
        ret = CHECK_CU(cu->cuDeviceGetCount(&count));
        if (ret < 0)
            return -1;

        display_dev = -1;
        for (int i = 0; i < count; i++) {
            CUdevice dev;
            ret = CHECK_CU(cu->cuDeviceGet(&dev, i));
            if (ret < 0)
                continue;

            CUuuid uuid;
            ret = CHECK_CU(cu->cuDeviceGetUuid(&uuid, dev));
            if (ret < 0)
                continue;

            if (memcmp(vk_uuid, uuid.bytes, VK_UUID_SIZE) == 0) {
                display_dev = dev;
                break;
            }
        }

        if (display_dev == -1) {
            MP_ERR(hw, "Could not match Vulkan display device in CUDA.\n");
            return -1;
        }

        ret = CHECK_CU(cu->cuCtxCreate(&p->display_ctx, CU_CTX_SCHED_BLOCKING_SYNC,
                                       display_dev));
        if (ret < 0)
            return -1;

        p->decode_ctx = p->display_ctx;
#endif
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
    int ret = 0;

    struct ext_vk *evk = &p->evk[n];

    struct ra_buf_params params = {
        .type = RA_BUF_TYPE_SHARED_MEMORY,
        .size = mp_image_plane_h(&p->layout, n) *
                mp_image_plane_w(&p->layout, n) *
                mapper->tex[n]->params.format->pixel_size,
    };

    struct ra_buf *buf = ra_buf_create(mapper->ra, &params);
    if (!buf) {
        goto error;
    }
    evk->rbuf = buf;

    struct vk_external_mem mem_info;

    bool success = ra_vk_buf_get_external_info(mapper->ra, buf, &mem_info);
    if (!success) {
        ret = -1;
        goto error;
    }

#if HAVE_WIN32_DESKTOP
    evk->handle = mem_info.mem_handle;
    MP_DBG(mapper, "vk_external_info[%d]: %p %zu %zu\n", n, evk->handle, mem_info.size, mem_info.offset);
#else
    evk->fd = mem_info.mem_fd;
    MP_DBG(mapper, "vk_external_info[%d]: %d %zu %zu\n", n, evk->fd, mem_info.size, mem_info.offset);
#endif

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC ext_desc = {
#if HAVE_WIN32_DESKTOP
        .type = IsWindows8OrGreater()
            ? CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32
            : CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT,
        .handle.win32.handle = evk->handle,
#else
        .type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD,
        .handle.fd = evk->fd,
#endif
        .size = mem_info.mem_size,
        .flags = 0,
    };
    ret = CHECK_CU(cu->cuImportExternalMemory(&evk->mem, &ext_desc));
    if (ret < 0)
        goto error;

    CUDA_EXTERNAL_MEMORY_BUFFER_DESC buf_desc = {
        .offset = mem_info.offset,
        .size = mem_info.size,
        .flags = 0,
    };
    ret = CHECK_CU(cu->cuExternalMemoryGetMappedBuffer(&evk->buf, evk->mem, &buf_desc));
    if (ret < 0) {
        goto error;
    }

    ret = ra_vk_create_external_semaphore(mapper->ra, &evk->signal);
    if (ret == 0) {
        goto error;
    }

    CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC s_desc = {
        .type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD,
#if HAVE_WIN32_DESKTOP
        .handle.win32.handle = evk->signal.handle,
#else
        .handle.fd = evk->signal.fd,
#endif
    };

    ret = CHECK_CU(cu->cuImportExternalSemaphore(&evk->ss, &s_desc));
    if (ret < 0)
        goto error;

    ret = ra_vk_create_external_semaphore(mapper->ra, &evk->wait);
    if (ret == 0) {
        goto error;
    }

    CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC w_desc = {
        .type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD,
#if HAVE_WIN32_DESKTOP
        .handle.win32.handle = evk->wait.handle,
#else
        .handle.fd = evk->wait.fd,
#endif
    };
    ret = CHECK_CU(cu->cuImportExternalSemaphore(&evk->ws, &w_desc));
    if (ret < 0)
        goto error;

    return true;

error:
    MP_ERR(mapper, "cuda_ext_vk_init failed\n");
    return false;
}

static void cuda_ext_vk_uninit(struct ra_hwdec_mapper *mapper, int n)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    CudaFunctions *cu = p_owner->cu;

    struct ext_vk *evk = &p->evk[n];
    if (evk) {
        if (evk->mem > 0) {
           CHECK_CU(cu->cuDestroyExternalMemory(evk->mem));
#if HAVE_WIN32_DESKTOP
        }
        if (evk->handle) {
            // Handle must always be closed by us.
            CloseHandle(evk->handle);
        }
#else
        } else if (evk->fd > -1) {
            // fd should only be closed if external memory was not imported
            close(evk->fd);
        }
#endif
        if (evk->rbuf) {
            ra_buf_free(mapper->ra, &evk->rbuf);
        }

        if (evk->ss) {
            CHECK_CU(cu->cuDestroyExternalSemaphore(evk->ss));
#if HAVE_WIN32_DESKTOP
        }
        if (evk->signal.handle) {
            // Handle must always be closed by us.
            CloseHandle(evk->signal.handle);
        }
#else
        } else if (evk->signal.fd > -1) {
            close(evk->signal.fd);
        }
#endif
        if(evk->signal.s) {
            vkDestroySemaphore(ra_vk_get(mapper->ra)->dev, evk->signal.s, NULL);
        }

        if (evk->ws) {
            CHECK_CU(cu->cuDestroyExternalSemaphore(evk->ws));
#if HAVE_WIN32_DESKTOP
        }
        if (evk->wait.handle) {
            // Handle must always be closed by us.
            CloseHandle(evk->wait.handle);
        }
#else
        } else if (evk->wait.fd > -1) {
            close(evk->wait.fd);
        }
#endif
        if(evk->wait.s) {
            vkDestroySemaphore(ra_vk_get(mapper->ra)->dev, evk->wait.s, NULL);
         }
    }
}
#endif // HAVE_VULKAN

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
        const struct ra_format *format = desc.planes[n];

        struct ra_tex_params params = {
            .dimensions = 2,
            .w = mp_image_plane_w(&p->layout, n),
            .h = mp_image_plane_h(&p->layout, n),
            .d = 1,
            .format = format,
            .render_src = true,
            .host_mutable = p_owner->is_vk,
            .src_linear = format->linear_filter,
        };

        mapper->tex[n] = ra_tex_create(mapper->ra, &params);
        if (!mapper->tex[n]) {
            ret = -1;
            goto error;
        }

        if (p_owner->is_gl) {
#if HAVE_GL
            GLuint texture;
            GLenum target;
            ra_gl_get_raw_tex(mapper->ra, mapper->tex[n], &texture, &target);

            ret = CHECK_CU(cu->cuGraphicsGLRegisterImage(&p->cu_res[n], texture, target,
                                                         CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
            if (ret < 0)
                goto error;

            ret = CHECK_CU(cu->cuGraphicsMapResources(1, &p->cu_res[n], 0));
            if (ret < 0)
                goto error;

            ret = CHECK_CU(cu->cuGraphicsSubResourceGetMappedArray(&p->cu_array[n], p->cu_res[n],
                                                                   0, 0));
            if (ret < 0)
                goto error;

            ret = CHECK_CU(cu->cuGraphicsUnmapResources(1, &p->cu_res[n], 0));
            if (ret < 0)
                goto error;
#endif
        } else if (p_owner->is_vk) {
#if HAVE_VULKAN
            ret = cuda_ext_vk_init(mapper, format, n);
            if (ret < 0)
                goto error;
#endif
        }
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

#if HAVE_VULKAN
    // We need to flush the vulkan pipeline to avoid destroying
    // semaphores that are waiting to be signalled. This is considered
    // a Vulkan error.
    if (p_owner->is_vk) {
        mpvk_poll_commands(ra_vk_get(mapper->ra), UINT64_MAX);
    }
#endif

    // Don't bail if any CUDA calls fail. This is all best effort.
    CHECK_CU(cu->cuCtxPushCurrent(p->display_ctx));
    for (int n = 0; n < 4; n++) {
        if (p->cu_res[n] > 0)
            CHECK_CU(cu->cuGraphicsUnregisterResource(p->cu_res[n]));
        p->cu_res[n] = 0;
#if HAVE_VULKAN
        cuda_ext_vk_uninit(mapper, n);
#endif
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
    bool is_gl = p_owner->is_gl;
    bool is_vk = p_owner->is_vk;

    ret = CHECK_CU(cu->cuCtxPushCurrent(p->display_ctx));
    if (ret < 0)
        return ret;

    for (int n = 0; n < p->layout.num_planes; n++) {
#if HAVE_VULKAN
        if (is_vk) {
            ret = ra_vk_hold(mapper->ra, mapper->tex[n], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_ACCESS_MEMORY_WRITE_BIT, p->evk[n].wait.s);
            if (!ret)
                goto error;

            CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS wp = { 0, };
            ret = CHECK_CU(cu->cuWaitExternalSemaphoresAsync(&p->evk[n].ws, &wp, 1, 0));
            if (ret < 0)
                goto error;
        }
#endif
        CUDA_MEMCPY2D cpy = {
            .srcMemoryType = CU_MEMORYTYPE_DEVICE,
            .srcDevice     = (CUdeviceptr)mapper->src->planes[n],
            .srcPitch      = mapper->src->stride[n],
            .srcY          = 0,
            .WidthInBytes  = mp_image_plane_w(&p->layout, n) *
                             mapper->tex[n]->params.format->pixel_size,
            .Height        = mp_image_plane_h(&p->layout, n),
        };

        if (is_gl) {
            cpy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
            cpy.dstArray = p->cu_array[n];
        } else if (is_vk) {
#if HAVE_VULKAN
            cpy.dstMemoryType = CU_MEMORYTYPE_DEVICE;
            cpy.dstDevice = p->evk[n].buf;
            cpy.dstPitch  = mp_image_plane_w(&p->layout, n) *
                            mapper->tex[n]->params.format->pixel_size;
#endif
        }

        ret = CHECK_CU(cu->cuMemcpy2DAsync(&cpy, 0));
        if (ret < 0)
            goto error;
#if HAVE_VULKAN
        if (is_vk) {
            CUDA_EXTERNAL_SEMAPHORE_SIGNAL_PARAMS sp = { 0, };
            ret = CHECK_CU(cu->cuSignalExternalSemaphoresAsync(&p->evk[n].ss, &sp, 1, 0));
            if (ret < 0)
                goto error;

            ra_vk_release(mapper->ra, mapper->tex[n], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_ACCESS_MEMORY_WRITE_BIT, p->evk[n].signal.s);

            struct ra_tex_upload_params params = {
                .tex = mapper->tex[n],
                .invalidate = true,
                .buf = p->evk[n].rbuf,
            };
            mapper->ra->fns->tex_upload(mapper->ra, &params);
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
