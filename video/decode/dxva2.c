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

#include <ks.h>

#include <libavcodec/dxva2.h>

#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "osdep/windows_utils.h"
#include "video/fmt-conversion.h"
#include "video/dxva2.h"
#include "video/mp_image_pool.h"
#include "video/hwdec.h"
#include "video/d3d.h"

#define ADDITIONAL_SURFACES (4 + HWDEC_DELAY_QUEUE_COUNT)

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
DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main,  0x5b11d51b, 0x2f4c,0x4452,0xbc,0xc3,0x09,0xf2,0xa1,0x16,0x0c,0xc0);
DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main10,0x107af0e0, 0xef1a,0x4d19,0xab,0xa8,0x67,0xa1,0x63,0x07,0x3d,0x13);
DEFINE_GUID(DXVA2_NoEncrypt,          0x1b81beD0, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);

typedef IDirect3D9* WINAPI pDirect3DCreate9(UINT);
typedef HRESULT WINAPI pCreateDeviceManager9(UINT *, IDirect3DDeviceManager9 **);

typedef struct dxva2_mode {
  const GUID     *guid;
  const char     *name;
  enum AVCodecID codec;
  int            depth; // defaults to 8
} dxva2_mode;

#define MODE(id) &MP_CONCAT(DXVA2_Mode, id), # id

static const dxva2_mode dxva2_modes[] = {
    /* MPEG-2 */
    { MODE(MPEG2_VLD),      AV_CODEC_ID_MPEG2VIDEO },
    { MODE(MPEG2and1_VLD),  AV_CODEC_ID_MPEG2VIDEO },

    /* H.264 */
    { MODE(H264_F),         AV_CODEC_ID_H264 },
    { MODE(H264_E),         AV_CODEC_ID_H264 },
    /* Intel specific H.264 mode */
    { &DXVADDI_Intel_ModeH264_E, "Intel_ModeH264_E", AV_CODEC_ID_H264 },

    /* VC-1 / WMV3 */
    { MODE(VC1_D2010),      AV_CODEC_ID_VC1  },
    { MODE(VC1_D2010),      AV_CODEC_ID_WMV3 },
    { MODE(VC1_D),          AV_CODEC_ID_VC1  },
    { MODE(VC1_D),          AV_CODEC_ID_WMV3 },

    { MODE(HEVC_VLD_Main),  AV_CODEC_ID_HEVC },
    { MODE(HEVC_VLD_Main10),AV_CODEC_ID_HEVC, .depth = 10},

    { NULL,                 0 },
};

#undef MODE

struct dxva2_decoder {
    DXVA2_ConfigPictureDecode config;
    IDirectXVideoDecoder      *decoder;
    LPDIRECT3DSURFACE9        *surfaces;
    int                       num_surfaces;
    struct mp_image_pool      *pool;
};

typedef struct DXVA2Context {
    struct mp_log *log;

    HMODULE d3dlib;
    HMODULE dxva2lib;

    HANDLE  deviceHandle;

    IDirect3D9                  *d3d9;
    IDirect3DDevice9            *d3d9device;
    IDirect3DDeviceManager9     *d3d9devmgr;
    IDirectXVideoDecoderService *decoder_service;
    struct dxva2_decoder        *decoder;

    struct mp_image_pool        *sw_pool;
    int                         mp_format;
} DXVA2Context;

static void dxva2_uninit(struct lavc_ctx *s)
{
    DXVA2Context *ctx = s->hwdec_priv;
    if (!ctx)
        return;

    talloc_free(ctx->decoder);

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

static struct mp_image *dxva2_allocate_image(struct lavc_ctx *s, int w, int h)
{
    DXVA2Context *ctx = s->hwdec_priv;

    struct mp_image *img = mp_image_pool_get_no_alloc(ctx->decoder->pool,
                                                      IMGFMT_DXVA2, w, h);
    if (!img)
        MP_ERR(ctx, "Failed to allocate additional DXVA2 surface.\n");
    return img;
}

static void copy_nv12(struct mp_image *dest, uint8_t *src_bits,
                      unsigned src_pitch, unsigned surf_height)
{
    struct mp_image buf = {0};
    mp_image_setfmt(&buf, dest->imgfmt);
    mp_image_set_size(&buf, dest->w, dest->h);

    buf.planes[0] = src_bits;
    buf.stride[0] = src_pitch;
    buf.planes[1] = src_bits + src_pitch * surf_height;
    buf.stride[1] = src_pitch;
    mp_image_copy_gpu(dest, &buf);
}

static struct mp_image *dxva2_retrieve_image(struct lavc_ctx *s,
                                             struct mp_image *img)
{
    DXVA2Context       *ctx = s->hwdec_priv;
    LPDIRECT3DSURFACE9 surface = d3d9_surface_in_mp_image(img);
    D3DSURFACE_DESC    surfaceDesc;
    D3DLOCKED_RECT     LockedRect;
    HRESULT            hr;

    IDirect3DSurface9_GetDesc(surface, &surfaceDesc);

    if (surfaceDesc.Width < img->w || surfaceDesc.Height < img->h)
        return img;

    struct mp_image *sw_img = mp_image_pool_get(ctx->sw_pool, ctx->mp_format,
                                                surfaceDesc.Width,
                                                surfaceDesc.Height);

    if (!sw_img)
        return img;

    hr = IDirect3DSurface9_LockRect(surface, &LockedRect, NULL, D3DLOCK_READONLY);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Unable to lock DXVA2 surface: %s\n",
               mp_HRESULT_to_str(hr));
        talloc_free(sw_img);
        return img;
    }

    copy_nv12(sw_img, LockedRect.pBits, LockedRect.Pitch, surfaceDesc.Height);
    mp_image_set_size(sw_img, img->w, img->h);
    mp_image_copy_attributes(sw_img, img);

    IDirect3DSurface9_UnlockRect(surface);

    talloc_free(img);
    return sw_img;
}

static int create_device(struct lavc_ctx *s)
{
    DXVA2Context *ctx = s->hwdec_priv;
    pDirect3DCreate9      *createD3D = NULL;
    HRESULT hr;
    D3DPRESENT_PARAMETERS d3dpp = {0};
    D3DDISPLAYMODE        d3ddm;
    UINT adapter = D3DADAPTER_DEFAULT;

    if (s->hwdec_info && s->hwdec_info->hwctx && s->hwdec_info->hwctx->d3d_ctx) {
        ctx->d3d9device = s->hwdec_info->hwctx->d3d_ctx->d3d9_device;
        if (ctx->d3d9device) {
            IDirect3D9_AddRef(ctx->d3d9device);
            MP_VERBOSE(ctx, "Using VO-supplied device %p.\n", ctx->d3d9device);
            return 0;
        }
    }

    ctx->d3dlib = LoadLibrary(L"d3d9.dll");
    if (!ctx->d3dlib) {
        MP_ERR(ctx, "Failed to load D3D9 library\n");
        goto fail;
    }

    createD3D = (pDirect3DCreate9 *)GetProcAddress(ctx->d3dlib, "Direct3DCreate9");
    if (!createD3D) {
        MP_ERR(ctx, "Failed to locate Direct3DCreate9\n");
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
        MP_ERR(ctx, "Failed to create Direct3D device: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    return 0;

fail:
    return -1;
}

static int dxva2_init(struct lavc_ctx *s)
{
    DXVA2Context *ctx;
    pCreateDeviceManager9 *createDeviceManager = NULL;
    HRESULT hr;
    unsigned resetToken = 0;

    ctx = talloc_zero(NULL, DXVA2Context);
    if (!ctx)
        return -1;
    s->hwdec_priv = ctx;

    ctx->log = mp_log_new(s, s->log, "dxva2");

    if (s->hwdec->type == HWDEC_DXVA2_COPY) {
        mp_check_gpu_memcpy(ctx->log, NULL);
        ctx->sw_pool = talloc_steal(ctx, mp_image_pool_new(17));
    }

    ctx->deviceHandle = INVALID_HANDLE_VALUE;

    ctx->dxva2lib = LoadLibrary(L"dxva2.dll");
    if (!ctx->dxva2lib) {
        MP_ERR(ctx, "Failed to load DXVA2 library\n");
        goto fail;
    }

    if (create_device(s) < 0)
        goto fail;

    createDeviceManager = (pCreateDeviceManager9 *)GetProcAddress(ctx->dxva2lib, "DXVA2CreateDirect3DDeviceManager9");
    if (!createDeviceManager) {
        MP_ERR(ctx, "Failed to locate DXVA2CreateDirect3DDeviceManager9\n");
        goto fail;
    }

    hr = createDeviceManager(&resetToken, &ctx->d3d9devmgr);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to create Direct3D device manager: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    hr = IDirect3DDeviceManager9_ResetDevice(ctx->d3d9devmgr, ctx->d3d9device, resetToken);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to bind Direct3D device to device manager: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    hr = IDirect3DDeviceManager9_OpenDeviceHandle(ctx->d3d9devmgr, &ctx->deviceHandle);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to open device handle: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    hr = IDirect3DDeviceManager9_GetVideoService(ctx->d3d9devmgr, ctx->deviceHandle, &IID_IDirectXVideoDecoderService, (void **)&ctx->decoder_service);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to create IDirectXVideoDecoderService: %s\n",
               mp_HRESULT_to_str(hr));
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
        MP_ERR(ctx, "Unable to retrieve decoder configurations: %s\n",
               mp_HRESULT_to_str(hr));
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

static void dxva2_destroy_decoder(void *arg)
{
    struct dxva2_decoder *decoder = arg;
    if (decoder->decoder)
        IDirectXVideoDecoder_Release(decoder->decoder);

    if (decoder->surfaces) {
        for (int i = 0; i < decoder->num_surfaces; i++)
            IDirect3DSurface9_Release(decoder->surfaces[i]);
    }
}

static int dxva2_create_decoder(struct lavc_ctx *s, int w, int h,
                                enum AVCodecID codec_id, int profile)
{
    DXVA2Context *ctx = s->hwdec_priv;
    struct dxva_context *dxva_ctx = s->avctx->hwaccel_context;
    void *tmp = talloc_new(NULL);
    GUID *guid_list = NULL;
    unsigned guid_count = 0, i, j;
    GUID device_guid = GUID_NULL;
    D3DFORMAT target_format = 0;
    DXVA2_VideoDesc desc = { 0 };
    HRESULT hr;
    struct dxva2_decoder *decoder;
    int surface_alignment;
    int ret = -1;

    hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(ctx->decoder_service, &guid_count, &guid_list);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to retrieve decoder device GUIDs: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    // dump all decoder info
    MP_VERBOSE(ctx, "%d decoder devices:\n", (int)guid_count);
    for (j = 0; j < guid_count; j++) {
        GUID *guid = &guid_list[j];

        const char *name = "<unknown>";
        for (i = 0; dxva2_modes[i].guid; i++) {
            if (IsEqualGUID(dxva2_modes[i].guid, guid))
                name = dxva2_modes[i].name;
        }

        D3DFORMAT *target_list = NULL;
        unsigned target_count = 0;
        hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(ctx->decoder_service, guid, &target_count, &target_list);
        if (FAILED(hr))
            continue;
        char fmts[256] = {0};
        for (i = 0; i < target_count; i++)
            mp_snprintf_cat(fmts, sizeof(fmts), " %s", mp_tag_str(target_list[i]));
        CoTaskMemFree(target_list);
        MP_VERBOSE(ctx, "%s %s %s\n", mp_GUID_to_str(guid), name, fmts);
    }

    // find a suitable decoder
    for (i = 0; dxva2_modes[i].guid; i++) {
        D3DFORMAT *target_list = NULL;
        unsigned target_count = 0;
        const dxva2_mode *mode = &dxva2_modes[i];
        int depth = mode->depth;
        if (mode->codec != codec_id)
            continue;

        for (j = 0; j < guid_count; j++) {
            if (IsEqualGUID(mode->guid, &guid_list[j]))
                break;
        }
        if (j == guid_count)
            continue;

        if (codec_id == AV_CODEC_ID_HEVC) {
            if ((mode->depth > 8) != (s->avctx->profile == FF_PROFILE_HEVC_MAIN_10))
                continue;
        }

        hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(ctx->decoder_service, mode->guid, &target_count, &target_list);
        if (FAILED(hr)) {
            continue;
        }
        for (j = 0; j < target_count; j++) {
            const D3DFORMAT format = target_list[j];
            if (depth <= 8) {
                if (format == MKTAG('N','V','1','2')) {
                    ctx->mp_format = IMGFMT_NV12;
                    target_format = format;
                }
            } else {
                int p010 = mp_imgfmt_find(1, 1, 2, 10, MP_IMGFLAG_YUV_NV);
                if (p010 && (format == MKTAG('P','0','1','0') ||
                             format == MKTAG('P','0','1','6')))
                {
                    // There is no FFmpeg format that is like NV12 and supports
                    // 16 bit per component, but vo_opengl will use the lower
                    // bits in P010 anyway.
                    ctx->mp_format = p010;
                    target_format = format;
                }
            }
            if (target_format)
                break;
        }
        CoTaskMemFree(target_list);
        if (target_format && ctx->mp_format) {
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

    decoder = talloc_zero(tmp, struct dxva2_decoder);
    talloc_set_destructor(decoder, dxva2_destroy_decoder);
    if (dxva2_get_decoder_configuration(s, codec_id, &device_guid, &desc,
                                        &decoder->config) < 0) {
        goto fail;
    }

    /* decoding MPEG-2 requires additional alignment on some Intel GPUs,
       but it causes issues for H.264 on certain AMD GPUs..... */
    if (codec_id == AV_CODEC_ID_MPEG2VIDEO)
        surface_alignment = 32;
    /* the HEVC DXVA2 spec asks for 128 pixel aligned surfaces to ensure
       all coding features have enough room to work with */
    else if  (codec_id == AV_CODEC_ID_HEVC)
        surface_alignment = 128;
    else
        surface_alignment = 16;

    decoder->num_surfaces = hwdec_get_max_refs(s) + ADDITIONAL_SURFACES;

    decoder->surfaces = talloc_array(decoder, LPDIRECT3DSURFACE9, decoder->num_surfaces);
    hr = IDirectXVideoDecoderService_CreateSurface(
        ctx->decoder_service,
        FFALIGN(w, surface_alignment), FFALIGN(h, surface_alignment),
        decoder->num_surfaces - 1, target_format, D3DPOOL_DEFAULT, 0,
        DXVA2_VideoDecoderRenderTarget, decoder->surfaces, NULL);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to create %d video surfaces: %s\n",
               decoder->num_surfaces, mp_HRESULT_to_str(hr));
        goto fail;
    }

    hr = IDirectXVideoDecoderService_CreateVideoDecoder(
        ctx->decoder_service, &device_guid, &desc, &decoder->config,
        decoder->surfaces, decoder->num_surfaces, &decoder->decoder);
    if (FAILED(hr)) {
        MP_ERR(ctx, "Failed to create DXVA2 video decoder: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }
    decoder->pool = talloc_steal(decoder, mp_image_pool_new(decoder->num_surfaces));
    for (i = 0; i < decoder->num_surfaces; i++) {
        struct mp_image *img = dxva2_new_ref(decoder->decoder, decoder->surfaces[i], w, h);
        if (!img) {
            MP_ERR(ctx, "Failed to create DXVA2 image\n");
            goto fail;
        }
        mp_image_pool_add(decoder->pool, img);
    }

    // Pass required information on to ffmpeg.
    dxva_ctx->cfg           = &decoder->config;
    dxva_ctx->decoder       = decoder->decoder;
    dxva_ctx->surface       = decoder->surfaces;
    dxva_ctx->surface_count = decoder->num_surfaces;

    if (IsEqualGUID(&device_guid, &DXVADDI_Intel_ModeH264_E))
        dxva_ctx->workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;

    ctx->decoder = talloc_steal(NULL, decoder);
    ret = 0;
fail:
    talloc_free(tmp);
    return ret;
}

static int dxva2_init_decoder(struct lavc_ctx *s, int w, int h)
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
    if (codec == AV_CODEC_ID_HEVC && profile != FF_PROFILE_HEVC_MAIN &&
                                     profile != FF_PROFILE_HEVC_MAIN_10)
    {
        MP_ERR(ctx, "Unsupported H.265 profile for DXVA2 HWAccel: %d\n", profile);
        return -1;
    }

    talloc_free(ctx->decoder);
    ctx->decoder = NULL;

    if (dxva2_create_decoder(s, w, h, codec, profile) < 0) {
        MP_ERR(ctx, "Error creating the DXVA2 decoder\n");
        return -1;
    }

    return 0;
}

static int probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder)
{
    hwdec_request_api(info, "dxva2");
    // dxva2-copy can do without external context; dxva2 requires it.
    if (hwdec->type != HWDEC_DXVA2_COPY) {
        if (!info || !info->hwctx || !info->hwctx->d3d_ctx)
            return HWDEC_ERR_NO_CTX;
    }
    for (int i = 0; dxva2_modes[i].guid; i++) {
        const dxva2_mode *mode = &dxva2_modes[i];
        if (mp_codec_to_av_codec_id(decoder) == mode->codec)
            return 0;
    }
    return HWDEC_ERR_NO_CODEC;
}

const struct vd_lavc_hwdec mp_vd_lavc_dxva2 = {
    .type = HWDEC_DXVA2,
    .image_format = IMGFMT_DXVA2,
    .probe = probe,
    .init = dxva2_init,
    .uninit = dxva2_uninit,
    .init_decoder = dxva2_init_decoder,
    .allocate_image = dxva2_allocate_image,
};

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
