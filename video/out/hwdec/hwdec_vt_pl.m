/*
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
 *               2017 Aman Gupta <ffmpeg@tmm1.net>
 *               2023 rcombs <rcombs@rcombs.me>
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

#include <assert.h>

#include <CoreVideo/CoreVideo.h>
#include <Metal/Metal.h>

#include <libavutil/hwcontext.h>

#include <libplacebo/renderer.h>

#include "config.h"

#include "video/out/gpu/hwdec.h"
#include "video/out/placebo/ra_pl.h"
#include "video/mp_image_pool.h"

#if HAVE_VULKAN
#include "video/out/vulkan/common.h"
#endif

#include "hwdec_vt.h"

static bool check_hwdec(const struct ra_hwdec *hw)
{
    pl_gpu gpu = ra_pl_get(hw->ra_ctx->ra);
    if (!gpu) {
        // This is not a libplacebo RA;
        return false;
    }

    if (!(gpu->import_caps.tex & PL_HANDLE_MTL_TEX)) {
        MP_VERBOSE(hw, "VideoToolbox libplacebo interop requires support for "
                       "PL_HANDLE_MTL_TEX import.\n");
        return false;
    }

    return true;
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params.hw_subfmt = 0;

    if (!mapper->dst_params.imgfmt) {
        MP_ERR(mapper, "Unsupported CVPixelBuffer format.\n");
        return -1;
    }

    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &p->desc)) {
        MP_ERR(mapper, "Unsupported texture format.\n");
        return -1;
    }

    for (int n = 0; n < p->desc.num_planes; n++) {
        if (!p->desc.planes[n]) {
            MP_ERR(mapper, "Format unsupported.\n");
            return -1;
        }
    }

    id<MTLDevice> mtl_device = nil;

#ifdef VK_EXT_METAL_OBJECTS_SPEC_VERSION
    pl_gpu gpu = ra_pl_get(mapper->ra);
    if (gpu) {
        pl_vulkan vulkan = pl_vulkan_get(gpu);
        if (vulkan && vulkan->device && vulkan->instance && vulkan->get_proc_addr) {
            PFN_vkExportMetalObjectsEXT pExportMetalObjects = (PFN_vkExportMetalObjectsEXT)vulkan->get_proc_addr(vulkan->instance, "vkExportMetalObjectsEXT");
            if (pExportMetalObjects) {
                VkExportMetalDeviceInfoEXT device_info = {
                    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT,
                    .pNext = NULL,
                    .mtlDevice = nil,
                };

                VkExportMetalObjectsInfoEXT objects_info = {
                    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
                    .pNext = &device_info,
                };

                pExportMetalObjects(vulkan->device, &objects_info);

                mtl_device = device_info.mtlDevice;
                [mtl_device retain];
            }
        }
    }
#endif

    if (!mtl_device) {
        mtl_device = MTLCreateSystemDefaultDevice();
    }

    CVReturn err = CVMetalTextureCacheCreate(
        kCFAllocatorDefault,
        NULL,
        mtl_device,
        NULL,
        &p->mtl_texture_cache);

    [mtl_device release];

    if (err != noErr) {
        MP_ERR(mapper, "Failure in CVOpenGLESTextureCacheCreate: %d\n", err);
        return -1;
    }

    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    for (int i = 0; i < p->desc.num_planes; i++) {
        ra_tex_free(mapper->ra, &mapper->tex[i]);
        if (p->mtl_planes[i]) {
            CFRelease(p->mtl_planes[i]);
            p->mtl_planes[i] = NULL;
        }
    }

    CVMetalTextureCacheFlush(p->mtl_texture_cache, 0);
}

static const struct {
    const char *glsl;
    MTLPixelFormat mtl;
} mtl_fmts[] = {
    {"r16f",           MTLPixelFormatR16Float     },
    {"r32f",           MTLPixelFormatR32Float     },
    {"rg16f",          MTLPixelFormatRG16Float    },
    {"rg32f",          MTLPixelFormatRG32Float    },
    {"rgba16f",        MTLPixelFormatRGBA16Float  },
    {"rgba32f",        MTLPixelFormatRGBA32Float  },
    {"r11f_g11f_b10f", MTLPixelFormatRG11B10Float },

    {"r8",             MTLPixelFormatR8Unorm      },
    {"r16",            MTLPixelFormatR16Unorm     },
    {"rg8",            MTLPixelFormatRG8Unorm     },
    {"rg16",           MTLPixelFormatRG16Unorm    },
    {"rgba8",          MTLPixelFormatRGBA8Unorm   },
    {"rgba16",         MTLPixelFormatRGBA16Unorm  },
    {"rgb10_a2",       MTLPixelFormatRGB10A2Unorm },

    {"r8_snorm",       MTLPixelFormatR8Snorm      },
    {"r16_snorm",      MTLPixelFormatR16Snorm     },
    {"rg8_snorm",      MTLPixelFormatRG8Snorm     },
    {"rg16_snorm",     MTLPixelFormatRG16Snorm    },
    {"rgba8_snorm",    MTLPixelFormatRGBA8Snorm   },
    {"rgba16_snorm",   MTLPixelFormatRGBA16Snorm  },

    {"r8ui",           MTLPixelFormatR8Uint       },
    {"r16ui",          MTLPixelFormatR16Uint      },
    {"r32ui",          MTLPixelFormatR32Uint      },
    {"rg8ui",          MTLPixelFormatRG8Uint      },
    {"rg16ui",         MTLPixelFormatRG16Uint     },
    {"rg32ui",         MTLPixelFormatRG32Uint     },
    {"rgba8ui",        MTLPixelFormatRGBA8Uint    },
    {"rgba16ui",       MTLPixelFormatRGBA16Uint   },
    {"rgba32ui",       MTLPixelFormatRGBA32Uint   },
    {"rgb10_a2ui",     MTLPixelFormatRGB10A2Uint  },

    {"r8i",            MTLPixelFormatR8Sint       },
    {"r16i",           MTLPixelFormatR16Sint      },
    {"r32i",           MTLPixelFormatR32Sint      },
    {"rg8i",           MTLPixelFormatRG8Sint      },
    {"rg16i",          MTLPixelFormatRG16Sint     },
    {"rg32i",          MTLPixelFormatRG32Sint     },
    {"rgba8i",         MTLPixelFormatRGBA8Sint    },
    {"rgba16i",        MTLPixelFormatRGBA16Sint   },
    {"rgba32i",        MTLPixelFormatRGBA32Sint   },

    { NULL,            MTLPixelFormatInvalid },
};

static MTLPixelFormat get_mtl_fmt(const char* glsl)
{
    if (!glsl)
        return MTLPixelFormatInvalid;

    for (int i = 0; mtl_fmts[i].glsl; i++) {
        if (!strcmp(glsl, mtl_fmts[i].glsl))
            return mtl_fmts[i].mtl;
    }

    return MTLPixelFormatInvalid;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    pl_gpu gpu = ra_pl_get(mapper->owner->ra_ctx->ra);

    CVPixelBufferRelease(p->pbuf);
    p->pbuf = (CVPixelBufferRef)mapper->src->planes[3];
    CVPixelBufferRetain(p->pbuf);

    const bool planar = CVPixelBufferIsPlanar(p->pbuf);
    const int planes  = CVPixelBufferGetPlaneCount(p->pbuf);
    assert((planar && planes == p->desc.num_planes) || p->desc.num_planes == 1);

    for (int i = 0; i < p->desc.num_planes; i++) {
        const struct ra_format *fmt = p->desc.planes[i];

        pl_fmt plfmt = ra_pl_fmt_get(fmt);
        MTLPixelFormat format = get_mtl_fmt(plfmt->glsl_format);

        if (!format) {
            MP_ERR(mapper, "Format unsupported.\n");
            return -1;
        }

        size_t width  = CVPixelBufferGetWidthOfPlane(p->pbuf, i),
               height = CVPixelBufferGetHeightOfPlane(p->pbuf, i);

        CVReturn err = CVMetalTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault,
            p->mtl_texture_cache,
            p->pbuf,
            NULL,
            format,
            width,
            height,
            i,
            &p->mtl_planes[i]);

        if (err != noErr) {
            MP_ERR(mapper, "error creating texture for plane %d: %d\n", i, err);
            return -1;
        }

        struct pl_tex_params tex_params = {
            .w = width,
            .h = height,
            .d = 0,
            .format = plfmt,
            .sampleable = true,
            .import_handle = PL_HANDLE_MTL_TEX,
            .shared_mem = (struct pl_shared_mem) {
                .handle = {
                    .handle = CVMetalTextureGetTexture(p->mtl_planes[i]),
                },
            },
        };

        pl_tex pltex = pl_tex_create(gpu, &tex_params);
        if (!pltex)
            return -1;

        struct ra_tex *ratex = talloc_ptrtype(NULL, ratex);
        int ret = mppl_wrap_tex(mapper->ra, pltex, ratex);
        if (!ret) {
            pl_tex_destroy(gpu, &pltex);
            talloc_free(ratex);
            return -1;
        }
        mapper->tex[i] = ratex;
    }

    return 0;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    CVPixelBufferRelease(p->pbuf);
    if (p->mtl_texture_cache) {
        CFRelease(p->mtl_texture_cache);
        p->mtl_texture_cache = NULL;
    }
}

bool vt_pl_init(const struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    if (!check_hwdec(hw))
        return false;

    p->interop_init   = mapper_init;
    p->interop_uninit = mapper_uninit;
    p->interop_map    = mapper_map;
    p->interop_unmap  = mapper_unmap;

    return true;
}
