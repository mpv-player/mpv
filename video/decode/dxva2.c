/*
 * Ported from FFmpeg ffmpeg_dxva2.c (2dbee1a3935a91842c22eb65fd13f77e8d590e07).
 * Original copyright header follows:
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <windows.h>

#define DXVA2API_USE_BITFIELDS

#include <stdint.h>

#include <d3d9.h>
#include <dxva2api.h>

#include <libavcodec/dxva2.h>

#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "video/fmt-conversion.h"
#include "video/mp_image_pool.h"
#include "video/hwdec.h"
#include "gpu_memcpy_sse4.h"

// A minor evil.
#ifndef FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO
#define FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO    2
#endif

/* define all the GUIDs used directly here,
   to avoid problems with inconsistent dxva2api.h versions in mingw-w64 and different MSVC version */
#include <initguid.h>
DEFINE_GUID(IID_IDirectXVideoDecoderService, 0xfc51a551,0xd5e7,0x11d9,0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);

DEFINE_GUID(DXVA2_ModeMPEG2_VLD,      0xee27417f, 0x5e28,0x4e65,0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9);
DEFINE_GUID(DXVA2_ModeMPEG2and1_VLD,  0x86695f12, 0x340e,0x4f04,0x9f,0xd3,0x92,0x53,0xdd,0x32,0x74,0x60);
DEFINE_GUID(DXVA2_ModeH264_E,         0x1b81be68, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_F,         0x1b81be69, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVADDI_Intel_ModeH264_E, 0x604F8E68, 0x4951,0x4C54,0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);
DEFINE_GUID(DXVA2_ModeVC1_D,          0x1b81beA3, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_D2010,      0x1b81beA4, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_NoEncrypt,          0x1b81beD0, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(GUID_NULL,                0x00000000, 0x0000,0x0000,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

typedef IDirect3D9* WINAPI pDirect3DCreate9(UINT);
typedef HRESULT WINAPI pCreateDeviceManager9(UINT *, IDirect3DDeviceManager9 **);

typedef struct dxva2_mode {
  const GUID     *guid;
  enum AVCodecID codec;
} dxva2_mode;

static const dxva2_mode dxva2_modes[] = {
    /* MPEG-2 */
    { &DXVA2_ModeMPEG2_VLD,      AV_CODEC_ID_MPEG2VIDEO },
    { &DXVA2_ModeMPEG2and1_VLD,  AV_CODEC_ID_MPEG2VIDEO },

    /* H.264 */
    { &DXVA2_ModeH264_F,         AV_CODEC_ID_H264 },
    { &DXVA2_ModeH264_E,         AV_CODEC_ID_H264 },
    /* Intel specific H.264 mode */
    { &DXVADDI_Intel_ModeH264_E, AV_CODEC_ID_H264 },

    /* VC-1 / WMV3 */
    { &DXVA2_ModeVC1_D2010,      AV_CODEC_ID_VC1  },
    { &DXVA2_ModeVC1_D2010,      AV_CODEC_ID_WMV3 },
    { &DXVA2_ModeVC1_D,          AV_CODEC_ID_VC1  },
    { &DXVA2_ModeVC1_D,          AV_CODEC_ID_WMV3 },

    { NULL,                      0 },
};

typedef struct surface_info {
    int used;
    uint64_t age;
} surface_info;

typedef struct DXVA2Context {
    struct mp_log *log;

    void (*copy_nv12)(struct mp_image *dest, uint8_t *src_bits,
                      unsigned src_pitch, unsigned surf_height);

    HMODULE d3dlib;
    HMODULE dxva2lib;

    HANDLE  deviceHandle;

    IDirect3D9                  *d3d9;
    IDirect3DDevice9            *d3d9device;
    IDirect3DDeviceManager9     *d3d9devmgr;
    IDirectXVideoDecoderService *decoder_service;
    IDirectXVideoDecoder        *decoder;

    GUID                        decoder_guid;
    DXVA2_ConfigPictureDecode   decoder_config;

    LPDIRECT3DSURFACE9          *surfaces;
    surface_info                *surface_infos;
    uint32_t                    num_surfaces;
    uint64_t                    surface_age;

    struct mp_image_pool       *sw_pool;

    struct dxva_context         *av_dxva_ctx;
} DXVA2Context;

typedef struct DXVA2SurfaceWrapper {
    DXVA2Context         *ctx;
    LPDIRECT3DSURFACE9   surface;
    IDirectXVideoDecoder *decoder;
} DXVA2SurfaceWrapper;

static void dxva2_destroy_decoder(struct lavc_ctx *s)
{
    DXVA2Context *ctx = s->hwdec_priv;
    int i;

    if (ctx->surfaces) {
        for (i = 0; i < ctx->num_surfaces; i++) {
            if (ctx->surfaces[i])
                IDirect3DSurface9_Release(ctx->surfaces[i]);
        }
    }
    av_freep(&ctx->surfaces);
    av_freep(&ctx->surface_infos);
    ctx->num_surfaces = 0;
    ctx->surface_age  = 0;

    if (ctx->decoder) {
        IDirectXVideoDecoder_Release(ctx->decoder);
        ctx->decoder = NULL;
    }
}

static void dxva2_uninit(struct lavc_ctx *s)
{
    DXVA2Context *ctx = s->hwdec_priv;
    if (!ctx)
        return;

    if (ctx->decoder)
        dxva2_destroy_decoder(s);

    if (ctx->decoder_service)
        IDirectXVideoDecoderService_Release(ctx->decoder_service);

    if (ctx->d3d9devmgr && ctx->deviceHandle != INVALID_HANDLE_VALUE)
        IDirect3DDeviceManager9_CloseDeviceHandle(ctx->d3d9devmgr, ctx->deviceHandle);

    if (ctx->d3d9devmgr)
        IDirect3DDeviceManager9_Release(ctx->d3d9devmgr);

    if (ctx->d3d9device)
        IDirect3DDevice9_Release(ctx->d3d9device);

    if (ctx->d3d9)
        IDirect3D9_Release(ctx->d3d9);

    if (ctx->d3dlib)
        FreeLibrary(ctx->d3dlib);

    if (ctx->dxva2lib)
        FreeLibrary(ctx->dxva2lib);

    av_freep(&s->avctx->hwaccel_context);
    talloc_free(ctx);
    s->hwdec_priv = NULL;
}

static void dxva2_release_img(void *ptr)
{
    DXVA2SurfaceWrapper *w   = ptr;
    DXVA2Context        *ctx = w->ctx;
    int i;

    for (i = 0; i < ctx->num_surfaces; i++) {
        if (ctx->surfaces[i] == w->surface) {
            ctx->surface_infos[i].used = 0;
            break;
        }
    }
    IDirect3DSurface9_Release(w->surface);
    IDirectXVideoDecoder_Release(w->decoder);
    av_free(w);
}

static struct mp_image *dxva2_allocate_image(struct lavc_ctx *s, int fmt,
                                             int img_w, int img_h)
{
    DXVA2Context *ctx = s->hwdec_priv;

    if (fmt != IMGFMT_DXVA2)
        return NULL;

    int i, old_unused = -1;
    for (i = 0; i < ctx->num_surfaces; i++) {
        surface_info *info = &ctx->surface_infos[i];
        if (!info->used && (old_unused == -1 || info->age < ctx->surface_infos[old_unused].age))
            old_unused = i;
    }
    if (old_unused == -1) {
        MP_ERR(ctx, "No free DXVA2 surface!\n");
        return NULL;
    }
    i = old_unused;

    DXVA2SurfaceWrapper *w = av_mallocz(sizeof(*w));
    if (!w)
        return NULL;

    w->ctx     = ctx;
    w->surface = ctx->surfaces[i];;
    IDirect3DSurface9_AddRef(w->surface);
    w->decoder = ctx->decoder;
    IDirectXVideoDecoder_AddRef(w->decoder);

    ctx->surface_infos[i].used = 1;
    ctx->surface_infos[i].age  = ctx->surface_age++;

    struct mp_image mpi = {0};
    mp_image_setfmt(&mpi, IMGFMT_DXVA2);
    mp_image_set_size(&mpi, img_w, img_h);
    mpi.planes[3] = (void *)w->surface;

    return mp_image_new_custom_ref(&mpi, w, dxva2_release_img);
}

static void copy_nv12_fallback(struct mp_image *dest, uint8_t *src_bits,
                               unsigned src_pitch, unsigned surf_height)
{
    struct mp_image buf = {0};
    mp_image_setfmt(&buf, IMGFMT_NV12);
    mp_image_set_size(&buf, dest->w, dest->h);

    buf.planes[0] = src_bits;
    buf.stride[0] = src_pitch;
    buf.planes[1] = src_bits + src_pitch * surf_height;
    buf.stride[1] = src_pitch;
    mp_image_copy(dest, &buf);
}

#pragma GCC push_options
#pragma GCC target("sse4.1")

static void copy_nv12_gpu_sse4(struct mp_image *dest, uint8_t *src_bits,
                               unsigned src_pitch, unsigned surf_height)
{
    const int lines = dest->h;
    const int stride_y = dest->stride[0];
    const int stride_uv = dest->stride[1];

    // If the strides match, the image can be copied in one go
    if (stride_y == src_pitch && stride_uv == src_pitch) {
        const size_t size = lines * src_pitch;
        gpu_memcpy(dest->planes[0], src_bits, size);
        gpu_memcpy(dest->planes[1], src_bits + src_pitch * surf_height, size / 2);

    } else {
        // Copy the Y plane line-by-line
        uint8_t *dest_y = dest->planes[0];
        const uint8_t *src_y = src_bits;
        const int bytes_per_line = dest->w;
        for (int i = 0; i < lines; i++) {
            gpu_memcpy(dest_y, src_y, bytes_per_line);
            dest_y += stride_y;
            src_y += src_pitch;
        }

        // Copy the UV plane line-by-line
        uint8_t *dest_uv = dest->planes[1];
        const uint8_t *src_uv = src_bits + src_pitch * surf_height;
        for (int i = 0; i < lines / 2; i++) {
            gpu_memcpy(dest_uv, src_uv, bytes_per_line);
            dest_uv += stride_uv;
            src_uv += src_pitch;
        }
    }
}

#pragma GCC pop_options

static struct mp_image *dxva2_retrieve_image(struct lavc_ctx *s,
                                             struct mp_image *img)
{
    DXVA2Context       *ctx = s->hwdec_priv;
    LPDIRECT3DSURFACE9 surface = (LPDIRECT3DSURFACE9)img->planes[3];
    D3DSURFACE_DESC    surfaceDesc;
    D3DLOCKED_RECT     LockedRect;
    HRESULT            hr;

    IDirect3DSurface9_GetDesc(surface, &surfaceDesc);

    struct mp_image *sw_img =
        mp_image_pool_get(ctx->sw_pool, IMGFMT_NV12, img->w, img->h);

    if (!sw_img)
        return img;

    hr = IDirect3DSurface9_LockRect(surface, &LockedRect, NULL, D3DLOCK_READONLY);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Unable to lock DXVA2 surface\n");
        talloc_free(sw_img);
        return img;
    }

    ctx->copy_nv12(sw_img, LockedRect.pBits, LockedRect.Pitch, surfaceDesc.Height);
    mp_image_copy_attributes(sw_img, img);

    IDirect3DSurface9_UnlockRect(surface);

    talloc_free(img);
    return sw_img;
}

static int dxva2_init(struct lavc_ctx *s)
{
    DXVA2Context *ctx;
    pDirect3DCreate9      *createD3D = NULL;
    pCreateDeviceManager9 *createDeviceManager = NULL;
    HRESULT hr;
    D3DPRESENT_PARAMETERS d3dpp = {0};
    D3DDISPLAYMODE        d3ddm;
    unsigned resetToken = 0;
    UINT adapter = D3DADAPTER_DEFAULT;

    ctx = talloc_zero(NULL, DXVA2Context);
    if (!ctx)
        return -1;
    s->hwdec_priv = ctx;

    ctx->log = mp_log_new(s, s->log, "dxva2");
    ctx->sw_pool = talloc_steal(ctx, mp_image_pool_new(17));

    if (av_get_cpu_flags() & AV_CPU_FLAG_SSE4) {
        // Use a memcpy implementation optimised for copying from GPU memory
        MP_DBG(ctx, "Using SSE4 memcpy\n");
        ctx->copy_nv12 = copy_nv12_gpu_sse4;
    } else {
        // Use the CRT memcpy. This can be slower than software decoding.
        MP_WARN(ctx, "Using fallback memcpy (slow)\n");
        ctx->copy_nv12 = copy_nv12_fallback;
    }

    ctx->deviceHandle = INVALID_HANDLE_VALUE;

    ctx->d3dlib = LoadLibrary(L"d3d9.dll");
    if (!ctx->d3dlib) {
        MP_ERR(ctx, "Failed to load D3D9 library\n");
        goto fail;
    }
    ctx->dxva2lib = LoadLibrary(L"dxva2.dll");
    if (!ctx->dxva2lib) {
        MP_ERR(ctx, "Failed to load DXVA2 library\n");
        goto fail;
    }

    createD3D = (pDirect3DCreate9 *)GetProcAddress(ctx->d3dlib, "Direct3DCreate9");
    if (!createD3D) {
        MP_ERR(ctx, "Failed to locate Direct3DCreate9\n");
        goto fail;
    }
    createDeviceManager = (pCreateDeviceManager9 *)GetProcAddress(ctx->dxva2lib, "DXVA2CreateDirect3DDeviceManager9");
    if (!createDeviceManager) {
        MP_ERR(ctx, "Failed to locate DXVA2CreateDirect3DDeviceManager9\n");
        goto fail;
    }

    ctx->d3d9 = createD3D(D3D_SDK_VERSION);
    if (!ctx->d3d9) {
        MP_ERR(ctx, "Failed to create IDirect3D object\n");
        goto fail;
    }

    IDirect3D9_GetAdapterDisplayMode(ctx->d3d9, adapter, &d3ddm);
    d3dpp.Windowed         = TRUE;
    d3dpp.BackBufferWidth  = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferCount  = 0;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    d3dpp.Flags            = D3DPRESENTFLAG_VIDEO;

    hr = IDirect3D9_CreateDevice(ctx->d3d9, adapter, D3DDEVTYPE_HAL, GetShellWindow(),
                                 D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
                                 &d3dpp, &ctx->d3d9device);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to create Direct3D device\n");
        goto fail;
    }

    hr = createDeviceManager(&resetToken, &ctx->d3d9devmgr);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to create Direct3D device manager\n");
        goto fail;
    }

    hr = IDirect3DDeviceManager9_ResetDevice(ctx->d3d9devmgr, ctx->d3d9device, resetToken);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to bind Direct3D device to device manager\n");
        goto fail;
    }

    hr = IDirect3DDeviceManager9_OpenDeviceHandle(ctx->d3d9devmgr, &ctx->deviceHandle);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to open device handle\n");
        goto fail;
    }

    hr = IDirect3DDeviceManager9_GetVideoService(ctx->d3d9devmgr, ctx->deviceHandle, &IID_IDirectXVideoDecoderService, (void **)&ctx->decoder_service);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to create IDirectXVideoDecoderService\n");
        goto fail;
    }

    s->avctx->hwaccel_context = av_mallocz(sizeof(struct dxva_context));
    if (!s->avctx->hwaccel_context)
        goto fail;

    return 0;
fail:
    dxva2_uninit(s);
    return -1;
}

static int dxva2_get_decoder_configuration(struct lavc_ctx *s,
                                           enum AVCodecID codec_id,
                                           const GUID *device_guid,
                                           const DXVA2_VideoDesc *desc,
                                           DXVA2_ConfigPictureDecode *config)
{
    DXVA2Context *ctx = s->hwdec_priv;
    unsigned cfg_count = 0, best_score = 0;
    DXVA2_ConfigPictureDecode *cfg_list = NULL;
    DXVA2_ConfigPictureDecode best_cfg = {{0}};
    HRESULT hr;
    int i;

    hr = IDirectXVideoDecoderService_GetDecoderConfigurations(ctx->decoder_service, device_guid, desc, NULL, &cfg_count, &cfg_list);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Unable to retrieve decoder configurations\n");
        return -1;
    }

    for (i = 0; i < cfg_count; i++) {
        DXVA2_ConfigPictureDecode *cfg = &cfg_list[i];

        unsigned score;
        if (cfg->ConfigBitstreamRaw == 1)
            score = 1;
        else if (codec_id == AV_CODEC_ID_H264 && cfg->ConfigBitstreamRaw == 2)
            score = 2;
        else
            continue;
        if (IsEqualGUID(&cfg->guidConfigBitstreamEncryption, &DXVA2_NoEncrypt))
            score += 16;
        if (score > best_score) {
            best_score = score;
            best_cfg   = *cfg;
        }
    }
    CoTaskMemFree(cfg_list);

    if (!best_score) {
        MP_ERR(ctx, "No valid decoder configuration available\n");
        return -1;
    }

    *config = best_cfg;
    return 0;
}

static int dxva2_create_decoder(struct lavc_ctx *s, int w, int h,
                                enum AVCodecID codec_id, int profile)
{
    DXVA2Context *ctx = s->hwdec_priv;
    struct dxva_context *dxva_ctx = s->avctx->hwaccel_context;
    GUID *guid_list = NULL;
    unsigned guid_count = 0, i, j;
    GUID device_guid = GUID_NULL;
    D3DFORMAT target_format = 0;
    DXVA2_VideoDesc desc = { 0 };
    DXVA2_ConfigPictureDecode config;
    HRESULT hr;
    int surface_alignment;
    int ret;

    hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(ctx->decoder_service, &guid_count, &guid_list);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to retrieve decoder device GUIDs\n");
        goto fail;
    }

    for (i = 0; dxva2_modes[i].guid; i++) {
        D3DFORMAT *target_list = NULL;
        unsigned target_count = 0;
        const dxva2_mode *mode = &dxva2_modes[i];
        if (mode->codec != codec_id)
            continue;

        for (j = 0; j < guid_count; j++) {
            if (IsEqualGUID(mode->guid, &guid_list[j]))
                break;
        }
        if (j == guid_count)
            continue;

        hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(ctx->decoder_service, mode->guid, &target_count, &target_list);
        if (FAILED(hr)) {
            continue;
        }
        for (j = 0; j < target_count; j++) {
            const D3DFORMAT format = target_list[j];
            if (format == MKTAG('N','V','1','2')) {
                target_format = format;
                break;
            }
        }
        CoTaskMemFree(target_list);
        if (target_format) {
            device_guid = *mode->guid;
            break;
        }
    }
    CoTaskMemFree(guid_list);

    if (IsEqualGUID(&device_guid, &GUID_NULL)) {
        MP_ERR(ctx, "No decoder device for codec found\n");
        goto fail;
    }

    desc.SampleWidth  = w;
    desc.SampleHeight = h;
    desc.Format       = target_format;

    ret = dxva2_get_decoder_configuration(s, codec_id, &device_guid, &desc, &config);
    if (ret < 0) {
        goto fail;
    }

    /* decoding MPEG-2 requires additional alignment on some Intel GPUs,
       but it causes issues for H.264 on certain AMD GPUs..... */
    if (codec_id == AV_CODEC_ID_MPEG2VIDEO)
        surface_alignment = 32;
    else
        surface_alignment = 16;

    /* 4 base work surfaces */
    ctx->num_surfaces = 4;

    /* add surfaces based on number of possible refs */
    if (codec_id == AV_CODEC_ID_H264)
        ctx->num_surfaces += 16;
    else
        ctx->num_surfaces += 2;

    ctx->surfaces      = av_mallocz(ctx->num_surfaces * sizeof(*ctx->surfaces));
    ctx->surface_infos = av_mallocz(ctx->num_surfaces * sizeof(*ctx->surface_infos));

    if (!ctx->surfaces || !ctx->surface_infos) {
        MP_ERR(ctx, "Unable to allocate surface arrays\n");
        goto fail;
    }

    hr = IDirectXVideoDecoderService_CreateSurface(ctx->decoder_service,
                                                   FFALIGN(w, surface_alignment),
                                                   FFALIGN(h, surface_alignment),
                                                   ctx->num_surfaces - 1,
                                                   target_format, D3DPOOL_DEFAULT, 0,
                                                   DXVA2_VideoDecoderRenderTarget,
                                                   ctx->surfaces, NULL);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to create %d video surfaces\n", ctx->num_surfaces);
        goto fail;
    }

    hr = IDirectXVideoDecoderService_CreateVideoDecoder(ctx->decoder_service, &device_guid,
                                                        &desc, &config, ctx->surfaces,
                                                        ctx->num_surfaces, &ctx->decoder);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to create DXVA2 video decoder\n");
        goto fail;
    }

    ctx->decoder_guid   = device_guid;
    ctx->decoder_config = config;

    dxva_ctx->cfg           = &ctx->decoder_config;
    dxva_ctx->decoder       = ctx->decoder;
    dxva_ctx->surface       = ctx->surfaces;
    dxva_ctx->surface_count = ctx->num_surfaces;

    if (IsEqualGUID(&ctx->decoder_guid, &DXVADDI_Intel_ModeH264_E))
        dxva_ctx->workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;

    return 0;
fail:
    dxva2_destroy_decoder(s);
    return -1;
}

static int dxva2_init_decoder(struct lavc_ctx *s, int fmt, int w, int h)
{
    DXVA2Context *ctx = s->hwdec_priv;

    enum AVCodecID codec = s->avctx->codec_id;
    int profile = s->avctx->profile;
    if (codec == AV_CODEC_ID_H264 &&
        (profile & ~FF_PROFILE_H264_CONSTRAINED) > FF_PROFILE_H264_HIGH)
    {
        MP_ERR(ctx, "Unsupported H.264 profile for DXVA2 HWAccel: %d\n", profile);
        return -1;
    }

    if (ctx->decoder)
        dxva2_destroy_decoder(s);

    if (dxva2_create_decoder(s, w, h, codec, profile) < 0) {
        MP_ERR(ctx, "Error creating the DXVA2 decoder\n");
        return -1;
    }

    return 0;
}

static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    for (int i = 0; dxva2_modes[i].guid; i++) {
        const dxva2_mode *mode = &dxva2_modes[i];
        if (mp_codec_to_av_codec_id(decoder) == mode->codec)
            return 0;
    }
    return HWDEC_ERR_NO_CODEC;
}

const struct vd_lavc_hwdec mp_vd_lavc_dxva2_copy = {
    .type = HWDEC_DXVA2_COPY,
    .image_format = IMGFMT_DXVA2,
    .probe = probe,
    .init = dxva2_init,
    .uninit = dxva2_uninit,
    .init_decoder = dxva2_init_decoder,
    .allocate_image = dxva2_allocate_image,
    .process_image = dxva2_retrieve_image,
};
