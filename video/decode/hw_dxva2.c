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

#define DXVA2API_USE_BITFIELDS
#include <libavcodec/dxva2.h>
#include <libavutil/common.h>

#include "config.h"

#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "osdep/windows_utils.h"
#include "video/fmt-conversion.h"
#include "video/mp_image_pool.h"
#include "video/hwdec.h"

#include "d3d.h"

#if !HAVE_D3D_HWACCEL_NEW

#define ADDITIONAL_SURFACES HWDEC_EXTRA_SURFACES

struct priv {
    struct mp_log *log;

    IDirect3D9                  *d3d9;
    IDirect3DDevice9            *device;
    HANDLE                       device_handle;
    IDirect3DDeviceManager9     *device_manager;
    IDirectXVideoDecoderService *decoder_service;

    struct mp_image_pool        *decoder_pool;
    struct mp_image_pool        *sw_pool;
    int                          mpfmt_decoded;
};

struct dxva2_surface {
    IDirectXVideoDecoder *decoder;
    IDirect3DSurface9    *surface;
};

static void dxva2_release_img(void *arg)
{
    struct dxva2_surface *surface = arg;
    if (surface->surface)
        IDirect3DSurface9_Release(surface->surface);

    if (surface->decoder)
        IDirectXVideoDecoder_Release(surface->decoder);

    talloc_free(surface);
}

static struct mp_image *dxva2_new_ref(IDirectXVideoDecoder *decoder,
                                      IDirect3DSurface9 *d3d9_surface,
                                      int w, int h)
{
    if (!decoder || !d3d9_surface)
        return NULL;
    struct dxva2_surface *surface = talloc_zero(NULL, struct dxva2_surface);

    surface->surface = d3d9_surface;
    IDirect3DSurface9_AddRef(surface->surface);
    surface->decoder = decoder;
    IDirectXVideoDecoder_AddRef(surface->decoder);

    struct mp_image *mpi =
        mp_image_new_custom_ref(NULL, surface, dxva2_release_img);
    if (!mpi)
        abort();

    mp_image_setfmt(mpi, IMGFMT_DXVA2);
    mp_image_set_size(mpi, w, h);
    mpi->planes[3] = (void *)surface->surface;
    return mpi;
}

static struct mp_image *dxva2_allocate_image(struct lavc_ctx *s, int w, int h)
{
    struct priv *p = s->hwdec_priv;
    struct mp_image *img = mp_image_pool_get_no_alloc(p->decoder_pool,
                                                      IMGFMT_DXVA2, w, h);
    if (!img)
        MP_ERR(p, "Failed to allocate additional DXVA2 surface.\n");
    return img;
}

static struct mp_image *dxva2_retrieve_image(struct lavc_ctx *s,
                                             struct mp_image *img)
{
    HRESULT hr;
    struct priv *p = s->hwdec_priv;
    IDirect3DSurface9 *surface = img->imgfmt == IMGFMT_DXVA2 ?
        (IDirect3DSurface9 *)img->planes[3] : NULL;

    if (!surface) {
        MP_ERR(p, "Failed to get Direct3D surface from mp_image\n");
        return img;
    }

    D3DSURFACE_DESC surface_desc;
    IDirect3DSurface9_GetDesc(surface, &surface_desc);
    if (surface_desc.Width < img->w || surface_desc.Height < img->h) {
        MP_ERR(p, "Direct3D11 texture smaller than mp_image dimensions\n");
        return img;
    }

    struct mp_image *sw_img = mp_image_pool_get(p->sw_pool,
                                                p->mpfmt_decoded,
                                                surface_desc.Width,
                                                surface_desc.Height);
    if (!sw_img) {
        MP_ERR(p, "Failed to get %s surface from CPU pool\n",
               mp_imgfmt_to_name(p->mpfmt_decoded));
        return img;
    }

    D3DLOCKED_RECT lock;
    hr = IDirect3DSurface9_LockRect(surface, &lock, NULL, D3DLOCK_READONLY);
    if (FAILED(hr)) {
        MP_ERR(p, "Unable to lock DXVA2 surface: %s\n",
               mp_HRESULT_to_str(hr));
        talloc_free(sw_img);
        return img;
    }
    copy_nv12(sw_img, lock.pBits, lock.Pitch, surface_desc.Height);
    IDirect3DSurface9_UnlockRect(surface);

    mp_image_set_size(sw_img, img->w, img->h);
    mp_image_copy_attributes(sw_img, img);
    talloc_free(img);
    return sw_img;
}

static const struct d3d_decoded_format d3d9_formats[] = {
    {MKTAG('N','V','1','2'), "NV12", 8,  IMGFMT_NV12},
    {MKTAG('P','0','1','0'), "P010", 10, IMGFMT_P010},
    {MKTAG('P','0','1','6'), "P016", 16, IMGFMT_P010},
};

static void dump_decoder_info(struct lavc_ctx *s,
                              GUID *device_guids, UINT n_guids)
{
    struct priv *p = s->hwdec_priv;
    MP_VERBOSE(p, "%u decoder devices:\n", (unsigned)n_guids);
    for (UINT i = 0; i < n_guids; i++) {
        GUID *guid = &device_guids[i];
        char *description = d3d_decoder_guid_to_desc(guid);

        D3DFORMAT *formats = NULL;
        UINT     n_formats = 0;
        HRESULT hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(
            p->decoder_service, guid, &n_formats, &formats);
        if (FAILED(hr)) {
            MP_ERR(p, "Failed to get render targets for decoder %s:%s\n",
                   description, mp_HRESULT_to_str(hr));
        }

        char fmts[256] = {0};
        for (UINT j = 0; j < n_formats; j++) {
            mp_snprintf_cat(fmts, sizeof(fmts),
                            " %s", mp_tag_str(formats[j]));
        }
        CoTaskMemFree(formats);

        MP_VERBOSE(p, "%s %s\n", description, fmts);
    }
}

static bool dxva2_format_supported(struct lavc_ctx *s, const GUID *guid,
                                   const struct d3d_decoded_format *format)
{
    bool ret = false;
    struct priv *p = s->hwdec_priv;
    D3DFORMAT *formats = NULL;
    UINT     n_formats = 0;
    HRESULT hr = IDirectXVideoDecoderService_GetDecoderRenderTargets(
        p->decoder_service, guid, &n_formats, &formats);
    if (FAILED(hr)) {
        MP_ERR(p, "Callback failed to get render targets for decoder %s: %s",
               d3d_decoder_guid_to_desc(guid), mp_HRESULT_to_str(hr));
        return 0;
    }

    for (int i = 0; i < n_formats; i++) {
        ret = formats[i] == format->dxfmt;
        if (ret)
            break;
    }

    CoTaskMemFree(formats);
    return ret;
}

static int dxva2_init_decoder(struct lavc_ctx *s, int w, int h)
{
    HRESULT hr;
    int ret = -1;
    struct priv *p = s->hwdec_priv;
    TA_FREEP(&p->decoder_pool);

    int  n_surfaces = hwdec_get_max_refs(s) + ADDITIONAL_SURFACES;
    IDirect3DSurface9    **surfaces = NULL;
    IDirectXVideoDecoder  *decoder  = NULL;
    void                  *tmp      = talloc_new(NULL);

    UINT n_guids;
    GUID *device_guids;
    hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(
        p->decoder_service, &n_guids, &device_guids);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to retrieve decoder device GUIDs: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    dump_decoder_info(s, device_guids, n_guids);

    struct d3d_decoder_fmt fmt =
        d3d_select_decoder_mode(s, device_guids, n_guids,
                                d3d9_formats, MP_ARRAY_SIZE(d3d9_formats),
                                dxva2_format_supported);
    CoTaskMemFree(device_guids);
    if (!fmt.format) {
        MP_ERR(p, "Failed to find a suitable decoder\n");
        goto done;
    }

    p->mpfmt_decoded = fmt.format->mpfmt;
    struct mp_image_pool *decoder_pool =
        talloc_steal(tmp, mp_image_pool_new(n_surfaces));
    DXVA2_ConfigPictureDecode *decoder_config =
        talloc_zero(decoder_pool, DXVA2_ConfigPictureDecode);

    int w_align = w, h_align = h;
    d3d_surface_align(s, &w_align, &h_align);
    DXVA2_VideoDesc video_desc ={
        .SampleWidth  = w,
        .SampleHeight = h,
        .Format       = fmt.format->dxfmt,
    };
    UINT                     n_configs  = 0;
    DXVA2_ConfigPictureDecode *configs = NULL;
    hr = IDirectXVideoDecoderService_GetDecoderConfigurations(
        p->decoder_service, fmt.guid, &video_desc, NULL,
        &n_configs, &configs);
    if (FAILED(hr)) {
        MP_ERR(p, "Unable to retrieve decoder configurations: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    unsigned max_score = 0;
    for (UINT i = 0; i < n_configs; i++) {
        unsigned score = d3d_decoder_config_score(
            s, &configs[i].guidConfigBitstreamEncryption,
            configs[i].ConfigBitstreamRaw);
        if (score > max_score) {
            max_score       = score;
            *decoder_config = configs[i];
        }
    }
    CoTaskMemFree(configs);
    if (!max_score) {
        MP_ERR(p, "Failed to find a suitable decoder configuration\n");
        goto done;
    }

    surfaces = talloc_zero_array(decoder_pool, IDirect3DSurface9*, n_surfaces);
    hr = IDirectXVideoDecoderService_CreateSurface(
        p->decoder_service,
        w_align, h_align,
        n_surfaces - 1, fmt.format->dxfmt, D3DPOOL_DEFAULT, 0,
        DXVA2_VideoDecoderRenderTarget, surfaces, NULL);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to create %d video surfaces: %s\n",
               n_surfaces, mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = IDirectXVideoDecoderService_CreateVideoDecoder(
        p->decoder_service, fmt.guid, &video_desc, decoder_config,
        surfaces, n_surfaces, &decoder);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to create DXVA2 video decoder: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    for (int i = 0; i < n_surfaces; i++) {
        struct mp_image *img = dxva2_new_ref(decoder, surfaces[i], w, h);
        if (!img) {
            MP_ERR(p, "Failed to create DXVA2 image\n");
            goto done;
        }
        mp_image_pool_add(decoder_pool, img); // transferred to pool
    }

    // Pass required information on to ffmpeg.
    struct dxva_context *dxva_ctx = s->avctx->hwaccel_context;
    dxva_ctx->cfg           = decoder_config;
    dxva_ctx->decoder       = decoder;
    dxva_ctx->surface_count = n_surfaces;
    dxva_ctx->surface       = surfaces;
    dxva_ctx->workaround    = is_clearvideo(fmt.guid) ?
                              FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO : 0;

    p->decoder_pool = talloc_steal(NULL, decoder_pool);
    ret = 0;
done:
    // On success, `p->decoder_pool` mp_images still hold refs to `surfaces` and
    // `decoder`, so the pointers in the ffmpeg `dxva_context` strcture remain
    // valid for the lifetime of the pool.
    if (surfaces) {
        for (int i = 0; i < n_surfaces; i++)
            IDirect3DSurface9_Release(surfaces[i]);
    }
    if (decoder)
        IDirectXVideoDecoder_Release(decoder);

    talloc_free(tmp);
    return ret;
}

static void destroy_device(struct lavc_ctx *s)
{
    struct priv *p = s->hwdec_priv;

    if (p->device)
        IDirect3DDevice9_Release(p->device);

    if (p->d3d9)
        IDirect3D9_Release(p->d3d9);
}

static bool create_device(struct lavc_ctx *s)
{
    struct priv *p = s->hwdec_priv;

    d3d_load_dlls();
    if (!d3d9_dll) {
        MP_ERR(p, "Failed to load D3D9 library\n");
        return false;
    }

    HRESULT (WINAPI *Direct3DCreate9Ex)(UINT, IDirect3D9Ex **) =
        (void *)GetProcAddress(d3d9_dll, "Direct3DCreate9Ex");
    if (!Direct3DCreate9Ex) {
        MP_ERR(p, "Failed to locate Direct3DCreate9Ex\n");
        return false;
    }

    IDirect3D9Ex *d3d9ex = NULL;
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9ex);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to create IDirect3D9Ex object\n");
        return false;
    }

    UINT adapter = D3DADAPTER_DEFAULT;
    D3DDISPLAYMODEEX modeex = {0};
    IDirect3D9Ex_GetAdapterDisplayModeEx(d3d9ex, adapter, &modeex, NULL);

    D3DPRESENT_PARAMETERS present_params = {
        .Windowed         = TRUE,
        .BackBufferWidth  = 640,
        .BackBufferHeight = 480,
        .BackBufferCount  = 0,
        .BackBufferFormat = modeex.Format,
        .SwapEffect       = D3DSWAPEFFECT_DISCARD,
        .Flags            = D3DPRESENTFLAG_VIDEO,
    };

    IDirect3DDevice9Ex *exdev = NULL;
    hr = IDirect3D9Ex_CreateDeviceEx(d3d9ex, adapter,
                                     D3DDEVTYPE_HAL,
                                     GetShellWindow(),
                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                     D3DCREATE_MULTITHREADED |
                                     D3DCREATE_FPU_PRESERVE,
                                     &present_params,
                                     NULL,
                                     &exdev);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to create Direct3D device: %s\n",
               mp_HRESULT_to_str(hr));
        IDirect3D9_Release(d3d9ex);
        return false;
    }

    p->d3d9 = (IDirect3D9 *)d3d9ex;
    p->device = (IDirect3DDevice9 *)exdev;
    return true;
}

static void dxva2_uninit(struct lavc_ctx *s)
{
    struct priv *p = s->hwdec_priv;
    if (!p)
        return;

    av_freep(&s->avctx->hwaccel_context);
    talloc_free(p->decoder_pool);

    if (p->decoder_service)
        IDirectXVideoDecoderService_Release(p->decoder_service);

    if (p->device_manager && p->device_handle != INVALID_HANDLE_VALUE)
        IDirect3DDeviceManager9_CloseDeviceHandle(p->device_manager, p->device_handle);

    if (p->device_manager)
        IDirect3DDeviceManager9_Release(p->device_manager);

    destroy_device(s);

    TA_FREEP(&s->hwdec_priv);
}

static int dxva2_init(struct lavc_ctx *s)
{
    HRESULT hr;
    struct priv *p = talloc_zero(NULL, struct priv);
    if (!p)
        return -1;

    s->hwdec_priv = p;
    p->device_handle = INVALID_HANDLE_VALUE;
    p->log           = mp_log_new(s, s->log, "dxva2");

    if (s->hwdec->type == HWDEC_DXVA2_COPY) {
        mp_check_gpu_memcpy(p->log, NULL);
        p->sw_pool = talloc_steal(p, mp_image_pool_new(17));
    }

    p->device = hwdec_devices_load(s->hwdec_devs, s->hwdec->type);
    if (p->device) {
        IDirect3D9_AddRef(p->device);
        MP_VERBOSE(p, "Using VO-supplied device %p.\n", p->device);
    } else if (s->hwdec->type == HWDEC_DXVA2) {
        MP_ERR(p, "No Direct3D device provided for native dxva2 decoding\n");
        goto fail;
    } else {
        if (!create_device(s))
            goto fail;
    }

    d3d_load_dlls();
    if (!dxva2_dll) {
        MP_ERR(p, "Failed to load DXVA2 library\n");
        goto fail;
    }

    HRESULT (WINAPI *CreateDeviceManager9)(UINT *, IDirect3DDeviceManager9 **) =
        (void *)GetProcAddress(dxva2_dll, "DXVA2CreateDirect3DDeviceManager9");
    if (!CreateDeviceManager9) {
        MP_ERR(p, "Failed to locate DXVA2CreateDirect3DDeviceManager9\n");
        goto fail;
    }

    unsigned reset_token = 0;
    hr = CreateDeviceManager9(&reset_token, &p->device_manager);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to create Direct3D device manager: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    hr = IDirect3DDeviceManager9_ResetDevice(p->device_manager,
                                             p->device, reset_token);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to bind Direct3D device to device manager: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    hr = IDirect3DDeviceManager9_OpenDeviceHandle(p->device_manager,
                                                  &p->device_handle);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to open device handle: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    hr = IDirect3DDeviceManager9_GetVideoService(
        p->device_manager, p->device_handle, &IID_IDirectXVideoDecoderService,
        (void **)&p->decoder_service);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to create IDirectXVideoDecoderService: %s\n",
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

static int dxva2_probe(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                       const char *codec)
{
    // dxva2-copy can do without external context; dxva2 requires it.
    if (hwdec->type == HWDEC_DXVA2) {
        if (!hwdec_devices_load(ctx->hwdec_devs, HWDEC_DXVA2))
            return HWDEC_ERR_NO_CTX;
    } else {
        hwdec_devices_load(ctx->hwdec_devs, HWDEC_DXVA2_COPY);
    }
    return d3d_probe_codec(codec);
}

const struct vd_lavc_hwdec mp_vd_lavc_dxva2 = {
    .type           = HWDEC_DXVA2,
    .image_format   = IMGFMT_DXVA2,
    .probe          = dxva2_probe,
    .init           = dxva2_init,
    .uninit         = dxva2_uninit,
    .init_decoder   = dxva2_init_decoder,
    .allocate_image = dxva2_allocate_image,
};

const struct vd_lavc_hwdec mp_vd_lavc_dxva2_copy = {
    .type           = HWDEC_DXVA2_COPY,
    .copying        = true,
    .image_format   = IMGFMT_DXVA2,
    .probe          = dxva2_probe,
    .init           = dxva2_init,
    .uninit         = dxva2_uninit,
    .init_decoder   = dxva2_init_decoder,
    .allocate_image = dxva2_allocate_image,
    .process_image  = dxva2_retrieve_image,
    .delay_queue    = HWDEC_DELAY_QUEUE_COUNT,
};

#else /* !HAVE_D3D_HWACCEL_NEW */

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_dxva2.h>

static void d3d9_free_av_device_ref(AVHWDeviceContext *ctx)
{
    AVDXVA2DeviceContext *hwctx = ctx->hwctx;

    if (hwctx->devmgr)
        IDirect3DDeviceManager9_Release(hwctx->devmgr);
}

AVBufferRef *d3d9_wrap_device_ref(IDirect3DDevice9 *device)
{
    HRESULT hr;

    d3d_load_dlls();
    if (!dxva2_dll)
        return NULL;

    HRESULT (WINAPI *DXVA2CreateDirect3DDeviceManager9)(UINT *, IDirect3DDeviceManager9 **) =
        (void *)GetProcAddress(dxva2_dll, "DXVA2CreateDirect3DDeviceManager9");
    if (!DXVA2CreateDirect3DDeviceManager9)
        return NULL;

    AVBufferRef *device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_DXVA2);
    if (!device_ref)
        return NULL;

    AVHWDeviceContext *ctx = (void *)device_ref->data;
    AVDXVA2DeviceContext *hwctx = ctx->hwctx;

    UINT reset_token = 0;
    hr = DXVA2CreateDirect3DDeviceManager9(&reset_token, &hwctx->devmgr);
    if (FAILED(hr))
        goto fail;

    IDirect3DDeviceManager9_ResetDevice(hwctx->devmgr, device, reset_token);
    if (FAILED(hr))
        goto fail;

    ctx->free = d3d9_free_av_device_ref;

    if (av_hwdevice_ctx_init(device_ref) < 0)
        goto fail;

    return device_ref;

fail:
    d3d9_free_av_device_ref(ctx);
    av_buffer_unref(&device_ref);
    return NULL;
}

static void d3d9_destroy_dev(struct mp_hwdec_ctx *ctx)
{
    av_buffer_unref(&ctx->av_device_ref);
    IDirect3DDevice9_Release((IDirect3DDevice9 *)ctx->ctx);
    talloc_free(ctx);
}

static struct mp_hwdec_ctx *d3d9_create_dev(struct mpv_global *global,
                                            struct mp_log *plog, bool probing)
{
    d3d_load_dlls();
    if (!d3d9_dll || !dxva2_dll) {
        mp_err(plog, "Failed to load D3D9 library\n");
        return NULL;
    }

    HRESULT (WINAPI *Direct3DCreate9Ex)(UINT, IDirect3D9Ex **) =
        (void *)GetProcAddress(d3d9_dll, "Direct3DCreate9Ex");
    if (!Direct3DCreate9Ex) {
        mp_err(plog, "Failed to locate Direct3DCreate9Ex\n");
        return NULL;
    }

    IDirect3D9Ex *d3d9ex = NULL;
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9ex);
    if (FAILED(hr)) {
        mp_err(plog, "Failed to create IDirect3D9Ex object\n");
        return NULL;
    }

    UINT adapter = D3DADAPTER_DEFAULT;
    D3DDISPLAYMODEEX modeex = {0};
    IDirect3D9Ex_GetAdapterDisplayModeEx(d3d9ex, adapter, &modeex, NULL);

    D3DPRESENT_PARAMETERS present_params = {
        .Windowed         = TRUE,
        .BackBufferWidth  = 640,
        .BackBufferHeight = 480,
        .BackBufferCount  = 0,
        .BackBufferFormat = modeex.Format,
        .SwapEffect       = D3DSWAPEFFECT_DISCARD,
        .Flags            = D3DPRESENTFLAG_VIDEO,
    };

    IDirect3DDevice9Ex *exdev = NULL;
    hr = IDirect3D9Ex_CreateDeviceEx(d3d9ex, adapter,
                                     D3DDEVTYPE_HAL,
                                     GetShellWindow(),
                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                     D3DCREATE_MULTITHREADED |
                                     D3DCREATE_FPU_PRESERVE,
                                     &present_params,
                                     NULL,
                                     &exdev);
    IDirect3D9_Release(d3d9ex);
    if (FAILED(hr)) {
        mp_err(plog, "Failed to create Direct3D device: %s\n",
               mp_HRESULT_to_str(hr));
        return NULL;
    }

    struct mp_hwdec_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct mp_hwdec_ctx) {
        .type = HWDEC_D3D11VA_COPY,
        .ctx = exdev,
        .destroy = d3d9_destroy_dev,
        .av_device_ref = d3d9_wrap_device_ref((IDirect3DDevice9 *)exdev),
    };

    if (!ctx->av_device_ref) {
        mp_err(plog, "Failed to allocate AVHWDeviceContext.\n");
        d3d9_destroy_dev(ctx);
        return NULL;
    }

    return ctx;
}

const struct vd_lavc_hwdec mp_vd_lavc_dxva2 = {
    .type = HWDEC_DXVA2,
    .image_format = IMGFMT_DXVA2,
    .generic_hwaccel = true,
    .set_hwframes = true,
    .static_pool = true,
    .hwframes_refine = d3d_hwframes_refine,
    .pixfmt_map = (const enum AVPixelFormat[][2]) {
        {AV_PIX_FMT_YUV420P10, AV_PIX_FMT_P010},
        {AV_PIX_FMT_YUV420P,   AV_PIX_FMT_NV12},
        {AV_PIX_FMT_YUVJ420P,  AV_PIX_FMT_NV12},
        {AV_PIX_FMT_NONE}
    },
};

const struct vd_lavc_hwdec mp_vd_lavc_dxva2_copy = {
    .type = HWDEC_DXVA2_COPY,
    .copying = true,
    .image_format = IMGFMT_DXVA2,
    .generic_hwaccel = true,
    .create_dev = d3d9_create_dev,
    .set_hwframes = true,
    .static_pool = true,
    .hwframes_refine = d3d_hwframes_refine,
    .pixfmt_map = (const enum AVPixelFormat[][2]) {
        {AV_PIX_FMT_YUV420P10, AV_PIX_FMT_P010},
        {AV_PIX_FMT_YUV420P,   AV_PIX_FMT_NV12},
        {AV_PIX_FMT_YUVJ420P,  AV_PIX_FMT_NV12},
        {AV_PIX_FMT_NONE}
    },
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};

#endif /* else #if !HAVE_D3D_HWACCEL_NEW */
