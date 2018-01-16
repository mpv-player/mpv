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

#include <pthread.h>

#include "config.h"

#include <libavcodec/avcodec.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>

#if HAVE_D3D9_HWACCEL
#include <libavutil/hwcontext_dxva2.h>
#endif

#include "common/common.h"
#include "common/av_common.h"
#include "video/fmt-conversion.h"
#include "video/hwdec.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"
#include "osdep/windows_utils.h"

#include "d3d.h"

HMODULE d3d11_dll, d3d9_dll, dxva2_dll;
PFN_D3D11_CREATE_DEVICE d3d11_D3D11CreateDevice;

static pthread_once_t d3d_load_once = PTHREAD_ONCE_INIT;

#if !HAVE_UWP
static void d3d_do_load(void)
{
    d3d11_dll = LoadLibrary(L"d3d11.dll");
    d3d9_dll  = LoadLibrary(L"d3d9.dll");
    dxva2_dll = LoadLibrary(L"dxva2.dll");

    if (d3d11_dll) {
        d3d11_D3D11CreateDevice =
            (void *)GetProcAddress(d3d11_dll, "D3D11CreateDevice");
    }
}
#else
static void d3d_do_load(void)
{

    d3d11_D3D11CreateDevice = D3D11CreateDevice;
}
#endif

void d3d_load_dlls(void)
{
    pthread_once(&d3d_load_once, d3d_do_load);
}

// Test if Direct3D11 can be used by us. Basically, this prevents trying to use
// D3D11 on Win7, and then failing somewhere in the process.
bool d3d11_check_decoding(ID3D11Device *dev)
{
    HRESULT hr;
    // We assume that NV12 is always supported, if hw decoding is supported at
    // all.
    UINT supported = 0;
    hr = ID3D11Device_CheckFormatSupport(dev, DXGI_FORMAT_NV12, &supported);
    return !FAILED(hr) && (supported & D3D11_BIND_DECODER);
}

static void d3d11_refine_hwframes(AVBufferRef *hw_frames_ctx)
{
    AVHWFramesContext *fctx = (void *)hw_frames_ctx->data;

    if (fctx->format == AV_PIX_FMT_D3D11) {
        AVD3D11VAFramesContext *hwctx = fctx->hwctx;

        // According to hwcontex_d3d11va.h, yuv420p means DXGI_FORMAT_420_OPAQUE,
        // which has no shader support.
        if (fctx->sw_format != AV_PIX_FMT_YUV420P)
            hwctx->BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
}

AVBufferRef *d3d11_wrap_device_ref(ID3D11Device *device)
{
    AVBufferRef *device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!device_ref)
        return NULL;

    AVHWDeviceContext *ctx = (void *)device_ref->data;
    AVD3D11VADeviceContext *hwctx = ctx->hwctx;

    ID3D11Device_AddRef(device);
    hwctx->device = device;

    if (av_hwdevice_ctx_init(device_ref) < 0)
        av_buffer_unref(&device_ref);

    return device_ref;
}

static void d3d11_complete_image_params(struct mp_image *img)
{
    AVHWFramesContext *hw_frames = (void *)img->hwctx->data;

    // According to hwcontex_d3d11va.h, this means DXGI_FORMAT_420_OPAQUE.
    img->params.hw_flags = hw_frames->sw_format == AV_PIX_FMT_YUV420P
                         ? MP_IMAGE_HW_FLAG_OPAQUE : 0;
}

static struct AVBufferRef *d3d11_create_standalone(struct mpv_global *global,
        struct mp_log *plog, struct hwcontext_create_dev_params *params)
{
    ID3D11Device *device = NULL;
    HRESULT hr;

    d3d_load_dlls();
    if (!d3d11_D3D11CreateDevice) {
        mp_err(plog, "Failed to load D3D11 library\n");
        return NULL;
    }

    hr = d3d11_D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                 D3D11_CREATE_DEVICE_VIDEO_SUPPORT, NULL, 0,
                                 D3D11_SDK_VERSION, &device, NULL, NULL);
    if (FAILED(hr)) {
        mp_err(plog, "Failed to create D3D11 Device: %s\n",
               mp_HRESULT_to_str(hr));
        return NULL;
    }

    AVBufferRef *avref = d3d11_wrap_device_ref(device);
    ID3D11Device_Release(device);
    if (!avref)
        mp_err(plog, "Failed to allocate AVHWDeviceContext.\n");

    return avref;
}

const struct hwcontext_fns hwcontext_fns_d3d11 = {
    .av_hwdevice_type       = AV_HWDEVICE_TYPE_D3D11VA,
    .complete_image_params  = d3d11_complete_image_params,
    .refine_hwframes        = d3d11_refine_hwframes,
    .create_dev             = d3d11_create_standalone,
};

#if HAVE_D3D9_HWACCEL

#define DXVA2API_USE_BITFIELDS
#include <libavutil/common.h>

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

static struct AVBufferRef *d3d9_create_standalone(struct mpv_global *global,
        struct mp_log *plog, struct hwcontext_create_dev_params *params)
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

    AVBufferRef *avref = d3d9_wrap_device_ref((IDirect3DDevice9 *)exdev);
    IDirect3DDevice9Ex_Release(exdev);
    if (!avref)
        mp_err(plog, "Failed to allocate AVHWDeviceContext.\n");

    return avref;
}

const struct hwcontext_fns hwcontext_fns_dxva2 = {
    .av_hwdevice_type       = AV_HWDEVICE_TYPE_DXVA2,
    .create_dev             = d3d9_create_standalone,
};

#endif /* HAVE_D3D9_HWACCEL */
