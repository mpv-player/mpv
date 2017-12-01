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
};

const struct vd_lavc_hwdec mp_vd_lavc_dxva2_copy = {
    .type = HWDEC_DXVA2_COPY,
    .copying = true,
    .image_format = IMGFMT_DXVA2,
    .generic_hwaccel = true,
    .create_dev = d3d9_create_dev,
    .set_hwframes = true,
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};
