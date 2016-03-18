/*
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

#include <libavcodec/d3d11va.h>

#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "osdep/windows_utils.h"
#include "video/fmt-conversion.h"
#include "video/mp_image_pool.h"
#include "video/hwdec.h"

#include "video/d3d11va.h"
#include "d3d.h"

#define ADDITIONAL_SURFACES (4 + HWDEC_DELAY_QUEUE_COUNT)

struct d3d11va_decoder {
    ID3D11VideoDecoder   *decoder;
    struct mp_image_pool *pool;
    ID3D11Texture2D      *staging;
    int                   mpfmt_decoded;
};

struct priv {
    struct mp_log *log;

    HMODULE                 d3d11_dll;
    ID3D11Device           *device;
    ID3D11DeviceContext    *device_ctx;
    ID3D11VideoDevice      *video_dev;
    ID3D11VideoContext     *video_ctx;

    struct d3d11va_decoder *decoder;
    struct mp_image_pool   *sw_pool;
};

static struct mp_image *d3d11va_allocate_image(struct lavc_ctx *s, int w, int h)
{
    struct priv *p = s->hwdec_priv;
    struct mp_image *img = mp_image_pool_get_no_alloc(p->decoder->pool,
                                                      IMGFMT_D3D11VA, w, h);
    if (!img)
        MP_ERR(p, "Failed to get free D3D11VA surface\n");
    return img;
}

static struct mp_image *d3d11va_retrieve_image(struct lavc_ctx *s,
                                               struct mp_image *img)
{
    HRESULT hr;
    struct priv *p = s->hwdec_priv;
    ID3D11Texture2D              *staging = p->decoder->staging;
    ID3D11Texture2D              *texture = d3d11_texture_in_mp_image(img);
    ID3D11VideoDecoderOutputView *surface = d3d11_surface_in_mp_image(img);

    if (!texture || !surface) {
        MP_ERR(p, "Failed to get Direct3D texture and surface from mp_image\n");
        return img;
    }

    D3D11_TEXTURE2D_DESC texture_desc;
    ID3D11Texture2D_GetDesc(texture, &texture_desc);
    if (texture_desc.Width < img->w || texture_desc.Height < img->h) {
        MP_ERR(p, "Direct3D11 texture smaller than mp_image dimensions\n");
        return img;
    }

    // copy to the staging texture
    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC surface_desc;
    ID3D11VideoDecoderOutputView_GetDesc(surface, &surface_desc);
    ID3D11DeviceContext_CopySubresourceRegion(
        p->device_ctx,
        (ID3D11Resource *)staging, 0, 0, 0, 0,
        (ID3D11Resource *)texture, surface_desc.Texture2D.ArraySlice, NULL);

    struct mp_image *sw_img = mp_image_pool_get(p->sw_pool,
                                                p->decoder->mpfmt_decoded,
                                                texture_desc.Width,
                                                texture_desc.Height);
    if (!sw_img) {
        MP_ERR(p, "Failed to get %s surface from CPU pool\n",
               mp_imgfmt_to_name(p->decoder->mpfmt_decoded));
        return img;
    }

    // copy staging texture to the cpu mp_image
    D3D11_MAPPED_SUBRESOURCE lock;
    hr = ID3D11DeviceContext_Map(p->device_ctx, (ID3D11Resource *)staging,
                                 0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to map D3D11 surface: %s\n", mp_HRESULT_to_str(hr));
        talloc_free(sw_img);
        return img;
    }
    copy_nv12(sw_img, lock.pData, lock.RowPitch, texture_desc.Height);
    ID3D11DeviceContext_Unmap(p->device_ctx, (ID3D11Resource *)staging, 0);

    mp_image_set_size(sw_img, img->w, img->h);
    mp_image_copy_attributes(sw_img, img);
    talloc_free(img);
    return sw_img;
}

struct d3d11_format {
    DXGI_FORMAT format;
    const char *name;
    int         depth;
};

#define DFMT(name) MP_CONCAT(DXGI_FORMAT_, name), # name
static const struct d3d11_format d3d11_formats[] = {
    {DFMT(NV12),  8},
    {DFMT(P010), 10},
    {DFMT(P016), 16},
};
#undef DFMT

static BOOL d3d11_format_supported(struct lavc_ctx *s, const GUID *guid,
                                   const struct d3d11_format *format)
{
    struct priv *p = s->hwdec_priv;
    BOOL is_supported = FALSE;
    HRESULT hr = ID3D11VideoDevice_CheckVideoDecoderFormat(
        p->video_dev, guid, format->format, &is_supported);
    if (FAILED(hr)) {
        MP_ERR(p, "Check decoder output format %s for decoder %s: %s\n",
               format->name, d3d_decoder_guid_to_desc(guid),
               mp_HRESULT_to_str(hr));
    }
    return is_supported;
}

static void dump_decoder_info(struct lavc_ctx *s, const GUID *guid)
{
    struct priv *p = s->hwdec_priv;
    char fmts[256] = {0};
    for (int i = 0; i < MP_ARRAY_SIZE(d3d11_formats); i++) {
        const struct d3d11_format *format = &d3d11_formats[i];
        if (d3d11_format_supported(s, guid, format))
            mp_snprintf_cat(fmts, sizeof(fmts), " %s", format->name);
    }
    MP_VERBOSE(p, "%s %s\n", d3d_decoder_guid_to_desc(guid), fmts);
}

static DWORD get_dxfmt_cb(struct lavc_ctx *s, const GUID *guid, int depth)
{
    for (int i = 0; i < MP_ARRAY_SIZE(d3d11_formats); i++) {
        const struct d3d11_format *format = &d3d11_formats[i];
        if (depth <= format->depth &&
            d3d11_format_supported(s, guid, format)) {
            return format->format;
        }
    }
    return 0;
}

static void d3d11va_destroy_decoder(void *arg)
{
    struct d3d11va_decoder *decoder = arg;

    if (decoder->decoder)
        ID3D11VideoDecoder_Release(decoder->decoder);

    if (decoder->staging)
        ID3D11Texture2D_Release(decoder->staging);
}

static int d3d11va_init_decoder(struct lavc_ctx *s, int w, int h)
{
    HRESULT hr;
    int ret = -1;
    struct priv *p = s->hwdec_priv;
    TA_FREEP(&p->decoder);

    void *tmp = talloc_new(NULL);

    UINT n_guids = ID3D11VideoDevice_GetVideoDecoderProfileCount(p->video_dev);
    GUID *device_guids = talloc_array(tmp, GUID, n_guids);
    for (UINT i = 0; i < n_guids; i++) {
        GUID *guid = &device_guids[i];
        hr = ID3D11VideoDevice_GetVideoDecoderProfile(p->video_dev, i, guid);
        if (FAILED(hr)) {
            MP_ERR(p, "Failed to get VideoDecoderProfile %d: %s\n",
                   i, mp_HRESULT_to_str(hr));
            goto done;
        }
        dump_decoder_info(s, guid);
    }

    struct d3d_decoder_fmt fmt =
        d3d_select_decoder_mode(s, device_guids, n_guids, get_dxfmt_cb);
    if (fmt.mpfmt_decoded == IMGFMT_NONE) {
        MP_ERR(p, "Failed to find a suitable decoder\n");
        goto done;
    }

    struct d3d11va_decoder *decoder = talloc_zero(tmp, struct d3d11va_decoder);
    talloc_set_destructor(decoder, d3d11va_destroy_decoder);
    decoder->mpfmt_decoded = fmt.mpfmt_decoded;

    int n_surfaces = hwdec_get_max_refs(s) + ADDITIONAL_SURFACES;
    int w_align = w, h_align = h;
    d3d_surface_align(s, &w_align, &h_align);

    ID3D11Texture2D *texture = NULL;
    D3D11_TEXTURE2D_DESC tex_desc = {
        .Width            = w_align,
        .Height           = h_align,
        .MipLevels        = 1,
        .Format           = fmt.dxfmt_decoded,
        .SampleDesc.Count = 1,
        .MiscFlags        = 0,
        .ArraySize        = n_surfaces,
        .Usage            = D3D11_USAGE_DEFAULT,
        .BindFlags        = D3D11_BIND_DECODER,
        .CPUAccessFlags   = 0,
    };
    hr = ID3D11Device_CreateTexture2D(p->device, &tex_desc, NULL, &texture);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to create Direct3D11 texture with %d surfaces: %s\n",
               n_surfaces, mp_HRESULT_to_str(hr));
        goto done;
    }

    if (s->hwdec->type == HWDEC_D3D11VA_COPY) {
        // create staging texture shared with the CPU with mostly the same
        // parameters as the above decoder-bound texture
        ID3D11Texture2D_GetDesc(texture, &tex_desc);
        tex_desc.MipLevels      = 1;
        tex_desc.MiscFlags      = 0;
        tex_desc.ArraySize      = 1;
        tex_desc.Usage          = D3D11_USAGE_STAGING;
        tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        tex_desc.BindFlags      = 0;
        hr = ID3D11Device_CreateTexture2D(p->device, &tex_desc, NULL,
                                          &decoder->staging);
        if (FAILED(hr)) {
            MP_ERR(p, "Failed to create staging texture: %s\n",
                   mp_HRESULT_to_str(hr));
            goto done;
        }
    }

    // pool to hold the mp_image wrapped surfaces
    decoder->pool = talloc_steal(decoder, mp_image_pool_new(n_surfaces));
    // array of the same surfaces (needed by ffmpeg)
    ID3D11VideoDecoderOutputView **surfaces =
        talloc_array_ptrtype(decoder->pool, surfaces, n_surfaces);

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC view_desc = {
        .DecodeProfile = *fmt.guid,
        .ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D,
    };
    for (int i = 0; i < n_surfaces; i++) {
        ID3D11VideoDecoderOutputView **surface = &surfaces[i];
        view_desc.Texture2D.ArraySlice = i;
        hr = ID3D11VideoDevice_CreateVideoDecoderOutputView(
            p->video_dev, (ID3D11Resource *)texture, &view_desc, surface);
        if (FAILED(hr)) {
            MP_ERR(p, "Failed getting decoder output view %d: %s\n",
                   i, mp_HRESULT_to_str(hr));
            goto done;
        }
        struct mp_image *img = d3d11va_new_ref(*surface, w, h);
        ID3D11VideoDecoderOutputView_Release(*surface); // transferred to img
        if (!img) {
            MP_ERR(p, "Failed to create D3D11VA image %d\n", i);
            goto done;
        }
        mp_image_pool_add(decoder->pool, img); // transferred to pool
    }

    D3D11_VIDEO_DECODER_DESC decoder_desc = {
        .Guid         = *fmt.guid,
        .SampleWidth  = w,
        .SampleHeight = h,
        .OutputFormat = fmt.dxfmt_decoded,
    };
    UINT n_cfg;
    hr = ID3D11VideoDevice_GetVideoDecoderConfigCount(p->video_dev,
                                                      &decoder_desc, &n_cfg);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to get number of decoder configurations: %s)",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    // pick the config with the highest score
    D3D11_VIDEO_DECODER_CONFIG *decoder_config =
        talloc_zero(decoder, D3D11_VIDEO_DECODER_CONFIG);
    unsigned max_score = 0;
    for (UINT i = 0; i < n_cfg; i++) {
        D3D11_VIDEO_DECODER_CONFIG cfg;
        hr = ID3D11VideoDevice_GetVideoDecoderConfig(p->video_dev,
                                                     &decoder_desc,
                                                     i, &cfg);
        if (FAILED(hr)) {
            MP_ERR(p, "Failed to get decoder config %d: %s\n",
                   i, mp_HRESULT_to_str(hr));
            goto done;
        }
        unsigned score = d3d_decoder_config_score(
            s, &cfg.guidConfigBitstreamEncryption, cfg.ConfigBitstreamRaw);
        if (score > max_score) {
            max_score       = score;
            *decoder_config = cfg;
        }
    }
    if (!max_score) {
        MP_ERR(p, "Failed to find a suitable decoder configuration\n");
        goto done;
    }

    hr = ID3D11VideoDevice_CreateVideoDecoder(p->video_dev, &decoder_desc,
                                              decoder_config,
                                              &decoder->decoder);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to create video decoder: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    struct AVD3D11VAContext *avd3d11va_ctx = s->avctx->hwaccel_context;
    avd3d11va_ctx->decoder       = decoder->decoder;
    avd3d11va_ctx->video_context = p->video_ctx;
    avd3d11va_ctx->cfg           = decoder_config;
    avd3d11va_ctx->surface_count = n_surfaces;
    avd3d11va_ctx->surface       = surfaces;
    avd3d11va_ctx->workaround    = is_clearvideo(fmt.guid) ?
                                   FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO : 0;

    p->decoder = talloc_steal(NULL, decoder);
    ret = 0;
done:
    // still referenced by pool images / surfaces
    if (texture)
        ID3D11Texture2D_Release(texture);

    talloc_free(tmp);
    return ret;
}

static void destroy_device(struct lavc_ctx *s)
{
    struct priv *p = s->hwdec_priv;

    if (p->device)
        ID3D11Device_Release(p->device);

    if (p->device_ctx)
        ID3D11DeviceContext_Release(p->device_ctx);

    if (p->d3d11_dll)
        FreeLibrary(p->d3d11_dll);
}

static bool create_device(struct lavc_ctx *s, BOOL thread_safe)
{
    HRESULT hr;
    struct priv *p = s->hwdec_priv;

    p->d3d11_dll = LoadLibrary(L"d3d11.dll");
    if (!p->d3d11_dll) {
        MP_ERR(p, "Failed to load D3D11 library\n");
        return false;
    }

    PFN_D3D11_CREATE_DEVICE CreateDevice =
        (void *)GetProcAddress(p->d3d11_dll, "D3D11CreateDevice");
    if (!CreateDevice) {
        MP_ERR(p, "Failed to get D3D11CreateDevice symbol from DLL: %s\n",
               mp_LastError_to_str());
        return false;
    }

    hr = CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                      D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0,
                      D3D11_SDK_VERSION, &p->device, NULL, &p->device_ctx);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to create D3D11 Device: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }

    ID3D10Multithread *multithread;
    hr = ID3D11Device_QueryInterface(p->device, &IID_ID3D10Multithread,
                                     (void **)&multithread);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to get Multithread interface: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }
    ID3D10Multithread_SetMultithreadProtected(multithread, thread_safe);
    ID3D10Multithread_Release(multithread);
    return true;
}

static void d3d11va_uninit(struct lavc_ctx *s)
{
    struct priv *p = s->hwdec_priv;
    if (!p)
        return;

    talloc_free(p->decoder);
    av_freep(&s->avctx->hwaccel_context);

    if (p->video_dev)
        ID3D11VideoDevice_Release(p->video_dev);

    if (p->video_ctx)
        ID3D11VideoContext_Release(p->video_ctx);

    destroy_device(s);

    TA_FREEP(&s->hwdec_priv);
}

static int d3d11va_init(struct lavc_ctx *s)
{
    HRESULT hr;
    struct priv *p = talloc_zero(NULL, struct priv);
    if (!p)
        return -1;

    s->hwdec_priv = p;
    p->log = mp_log_new(s, s->log, "d3d11va");
    if (s->hwdec->type == HWDEC_D3D11VA_COPY) {
        mp_check_gpu_memcpy(p->log, NULL);
        p->sw_pool = talloc_steal(p, mp_image_pool_new(17));
    }

    if (!create_device(s, FALSE))
        goto fail;

    hr = ID3D11DeviceContext_QueryInterface(p->device_ctx,
                                            &IID_ID3D11VideoContext,
                                            (void **)&p->video_ctx);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to get VideoContext interface: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    hr = ID3D11Device_QueryInterface(p->device,
                                     &IID_ID3D11VideoDevice,
                                     (void **)&p->video_dev);
    if (FAILED(hr)) {
        MP_ERR(p, "Failed to get VideoDevice interface. %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    s->avctx->hwaccel_context = av_d3d11va_alloc_context();
    if (!s->avctx->hwaccel_context) {
        MP_ERR(p, "Failed to allocate hwaccel_context\n");
        goto fail;
    }

    return 0;
fail:
    d3d11va_uninit(s);
    return -1;
}

static int d3d11va_probe(struct vd_lavc_hwdec *hwdec,
                         struct mp_hwdec_info *info,
                         const char *decoder)
{
    hwdec_request_api(info, "d3d11va");
    return d3d_probe_codec(decoder);
}

const struct vd_lavc_hwdec mp_vd_lavc_d3d11va_copy = {
    .type           = HWDEC_D3D11VA_COPY,
    .image_format   = IMGFMT_D3D11VA,
    .probe          = d3d11va_probe,
    .init           = d3d11va_init,
    .uninit         = d3d11va_uninit,
    .init_decoder   = d3d11va_init_decoder,
    .allocate_image = d3d11va_allocate_image,
    .process_image  = d3d11va_retrieve_image,
};
