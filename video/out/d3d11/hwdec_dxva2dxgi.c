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

#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxva2api.h>

#include "common/common.h"
#include "osdep/windows_utils.h"
#include "video/hwdec.h"
#include "video/d3d.h"
#include "video/out/d3d11/ra_d3d11.h"
#include "video/out/gpu/hwdec.h"

struct priv_owner {
    struct mp_hwdec_ctx hwctx;
    ID3D11Device *dev11;
    IDirect3DDevice9Ex *dev9;
};

struct queue_surf {
    ID3D11Texture2D *tex11;
    ID3D11Query *idle11;
    ID3D11Texture2D *stage11;
    IDirect3DTexture9 *tex9;
    IDirect3DSurface9 *surf9;
    IDirect3DSurface9 *stage9;
    struct ra_tex *tex;

    bool busy11; // The surface is currently being used by D3D11
};

struct priv {
    ID3D11Device *dev11;
    ID3D11DeviceContext *ctx11;
    IDirect3DDevice9Ex *dev9;

    // Surface queue stuff. Following Microsoft recommendations, a queue of
    // surfaces is used to share images between D3D9 and D3D11. This allows
    // multiple D3D11 frames to be in-flight at once.
    struct queue_surf **queue;
    int queue_len;
    int queue_pos;
};

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);
    SAFE_RELEASE(p->dev11);
    SAFE_RELEASE(p->dev9);
}

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    IDirect3D9Ex *d3d9ex = NULL;
    int ret = -1;
    HRESULT hr;

    if (!ra_is_d3d11(hw->ra))
        goto done;
    p->dev11 = ra_d3d11_get_device(hw->ra);
    if (!p->dev11)
        goto done;

    d3d_load_dlls();
    if (!d3d9_dll) {
        MP_FATAL(hw, "Failed to load \"d3d9.dll\": %s\n", mp_LastError_to_str());
        goto done;
    }
    if (!dxva2_dll) {
        MP_FATAL(hw, "Failed to load \"dxva2.dll\": %s\n", mp_LastError_to_str());
        goto done;
    }

    HRESULT (WINAPI *Direct3DCreate9Ex)(UINT SDKVersion, IDirect3D9Ex **ppD3D);
    Direct3DCreate9Ex = (void *)GetProcAddress(d3d9_dll, "Direct3DCreate9Ex");
    if (!Direct3DCreate9Ex) {
        MP_FATAL(hw, "Direct3D 9Ex not supported\n");
        goto done;
    }

    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9ex);
    if (FAILED(hr)) {
        MP_FATAL(hw, "Couldn't create Direct3D9Ex: %s\n", mp_HRESULT_to_str(hr));
        goto done;
    }

    D3DPRESENT_PARAMETERS pparams = {
        .BackBufferWidth = 16,
        .BackBufferHeight = 16,
        .BackBufferCount = 1,
        .SwapEffect = D3DSWAPEFFECT_DISCARD,
        .hDeviceWindow = GetDesktopWindow(),
        .Windowed = TRUE,
        .Flags = D3DPRESENTFLAG_VIDEO,
    };
    hr = IDirect3D9Ex_CreateDeviceEx(d3d9ex, D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL, GetDesktopWindow(), D3DCREATE_NOWINDOWCHANGES |
        D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING |
        D3DCREATE_DISABLE_PSGP_THREADING | D3DCREATE_MULTITHREADED, &pparams,
        NULL, &p->dev9);
    if (FAILED(hr)) {
        MP_FATAL(hw, "Failed to create Direct3D9Ex device: %s\n",
                 mp_HRESULT_to_str(hr));
        goto done;
    }

    // Check if it's possible to StretchRect() from NV12 to XRGB surfaces
    hr = IDirect3D9Ex_CheckDeviceFormatConversion(d3d9ex, D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL, MAKEFOURCC('N', 'V', '1', '2'), D3DFMT_X8R8G8B8);
    if (hr != S_OK) {
        MP_FATAL(hw, "Can't StretchRect from NV12 to XRGB surfaces\n");
        goto done;
    }

    p->hwctx = (struct mp_hwdec_ctx){
        .driver_name = hw->driver->name,
        .av_device_ref = d3d9_wrap_device_ref((IDirect3DDevice9 *)p->dev9),
    };
    hwdec_devices_add(hw->devs, &p->hwctx);

    ret = 0;
done:
    SAFE_RELEASE(d3d9ex);
    return ret;
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *o = mapper->owner->priv;
    struct priv *p = mapper->priv;

    ID3D11Device_AddRef(o->dev11);
    p->dev11 = o->dev11;
    IDirect3DDevice9Ex_AddRef(o->dev9);
    p->dev9 = o->dev9;
    ID3D11Device_GetImmediateContext(o->dev11, &p->ctx11);

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = IMGFMT_RGB0;
    mapper->dst_params.hw_subfmt = 0;
    return 0;
}

static void surf_destroy(struct ra_hwdec_mapper *mapper,
                         struct queue_surf *surf)
{
    if (!surf)
        return;
    SAFE_RELEASE(surf->tex11);
    SAFE_RELEASE(surf->idle11);
    SAFE_RELEASE(surf->stage11);
    SAFE_RELEASE(surf->tex9);
    SAFE_RELEASE(surf->surf9);
    SAFE_RELEASE(surf->stage9);
    ra_tex_free(mapper->ra, &surf->tex);
    talloc_free(surf);
}

static struct queue_surf *surf_create(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    IDXGIResource *res11 = NULL;
    bool success = false;
    HRESULT hr;

    struct queue_surf *surf = talloc_ptrtype(p, surf);

    D3D11_TEXTURE2D_DESC desc11 = {
        .Width = mapper->src->w,
        .Height = mapper->src->h,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8X8_UNORM,
        .SampleDesc.Count = 1,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
        .MiscFlags = D3D11_RESOURCE_MISC_SHARED,
    };
    hr = ID3D11Device_CreateTexture2D(p->dev11, &desc11, NULL, &surf->tex11);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to create D3D11 texture: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    // Try to use a 16x16 staging texture, unless the source surface is
    // smaller. Ideally, a 1x1 texture would be sufficient, but Microsoft's
    // D3D9ExDXGISharedSurf example uses 16x16 to avoid driver bugs.
    D3D11_TEXTURE2D_DESC sdesc11 = {
        .Width = MPMIN(16, desc11.Width),
        .Height = MPMIN(16, desc11.Height),
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8X8_UNORM,
        .SampleDesc.Count = 1,
        .Usage = D3D11_USAGE_STAGING,
        .CPUAccessFlags = D3D11_CPU_ACCESS_READ,
    };
    hr = ID3D11Device_CreateTexture2D(p->dev11, &sdesc11, NULL, &surf->stage11);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to create D3D11 staging texture: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = ID3D11Texture2D_QueryInterface(surf->tex11, &IID_IDXGIResource,
                                        (void**)&res11);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to get share handle: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    HANDLE share_handle;
    hr = IDXGIResource_GetSharedHandle(res11, &share_handle);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to get share handle: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = ID3D11Device_CreateQuery(p->dev11,
        &(D3D11_QUERY_DESC) { D3D11_QUERY_EVENT }, &surf->idle11);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to create D3D11 query: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    // Share the D3D11 texture with D3D9Ex
    hr = IDirect3DDevice9Ex_CreateTexture(p->dev9, desc11.Width, desc11.Height,
        1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT,
        &surf->tex9, &share_handle);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to create D3D9 texture: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    hr = IDirect3DTexture9_GetSurfaceLevel(surf->tex9, 0, &surf->surf9);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to get D3D9 surface: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    // As above, try to use a 16x16 staging texture to avoid driver bugs
    hr = IDirect3DDevice9Ex_CreateRenderTarget(p->dev9,
        MPMIN(16, desc11.Width), MPMIN(16, desc11.Height), D3DFMT_X8R8G8B8,
        D3DMULTISAMPLE_NONE, 0, TRUE, &surf->stage9, NULL);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed to create D3D9 staging surface: %s\n",
               mp_HRESULT_to_str(hr));
        goto done;
    }

    surf->tex = ra_d3d11_wrap_tex(mapper->ra, (ID3D11Resource *)surf->tex11);
    if (!surf->tex)
        goto done;

    success = true;
done:
    if (!success)
        surf_destroy(mapper, surf);
    SAFE_RELEASE(res11);
    return success ? surf : NULL;
}

// true if the surface is currently in-use by the D3D11 graphics pipeline
static bool surf_is_idle11(struct ra_hwdec_mapper *mapper,
                           struct queue_surf *surf)
{
    struct priv *p = mapper->priv;
    HRESULT hr;
    BOOL idle;

    if (!surf->busy11)
        return true;

    hr = ID3D11DeviceContext_GetData(p->ctx11,
        (ID3D11Asynchronous *)surf->idle11, &idle, sizeof(idle),
        D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (FAILED(hr) || hr == S_FALSE || !idle)
        return false;

    surf->busy11 = false;
    return true;
}

// If the surface is currently in-use by the D3D11 graphics pipeline, wait for
// it to become idle. Should only be called in the queue-underflow case.
static bool surf_wait_idle11(struct ra_hwdec_mapper *mapper,
                             struct queue_surf *surf)
{
    struct priv *p = mapper->priv;
    HRESULT hr;

    ID3D11DeviceContext_CopySubresourceRegion(p->ctx11,
        (ID3D11Resource *)surf->stage11, 0, 0, 0, 0,
        (ID3D11Resource *)surf->tex11, 0, (&(D3D11_BOX){
            .right = MPMIN(16, mapper->src->w),
            .bottom = MPMIN(16, mapper->src->h),
            .back = 1,
        }));

    // Block until the surface becomes idle (see surf_wait_idle9())
    D3D11_MAPPED_SUBRESOURCE map = {0};
    hr = ID3D11DeviceContext_Map(p->ctx11, (ID3D11Resource *)surf->stage11, 0,
                                 D3D11_MAP_READ, 0, &map);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Couldn't map D3D11 staging texture: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }

    ID3D11DeviceContext_Unmap(p->ctx11, (ID3D11Resource *)surf->stage11, 0);
    surf->busy11 = false;
    return true;
}

static bool surf_wait_idle9(struct ra_hwdec_mapper *mapper,
                            struct queue_surf *surf)
{
    struct priv *p = mapper->priv;
    HRESULT hr;

    // Rather than polling for the surface to become idle, copy part of the
    // surface to a staging texture and map it. This should block until the
    // surface becomes idle. Microsoft's ISurfaceQueue does this as well.
    RECT rc = {0, 0, MPMIN(16, mapper->src->w), MPMIN(16, mapper->src->h)};
    hr = IDirect3DDevice9Ex_StretchRect(p->dev9, surf->surf9, &rc, surf->stage9,
                                        &rc, D3DTEXF_NONE);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Couldn't copy to D3D9 staging texture: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }

    D3DLOCKED_RECT lock;
    hr = IDirect3DSurface9_LockRect(surf->stage9, &lock, NULL, D3DLOCK_READONLY);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Couldn't map D3D9 staging texture: %s\n",
               mp_HRESULT_to_str(hr));
        return false;
    }

    IDirect3DSurface9_UnlockRect(surf->stage9);
    p->queue[p->queue_pos]->busy11 = true;
    return true;
}

static struct queue_surf *surf_acquire(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    if (!p->queue_len || !surf_is_idle11(mapper, p->queue[p->queue_pos])) {
        if (p->queue_len < 16) {
            struct queue_surf *surf = surf_create(mapper);
            if (!surf)
                return NULL;

            // The next surface is busy, so grow the queue
            MP_TARRAY_INSERT_AT(p, p->queue, p->queue_len, p->queue_pos, surf);
            MP_DBG(mapper, "Queue grew to %d surfaces\n", p->queue_len);
        } else {
            // For sanity, don't let the queue grow beyond 16 surfaces. It
            // should never get this big. If it does, wait for the surface to
            // become idle rather than polling it.
            if (!surf_wait_idle11(mapper, p->queue[p->queue_pos]))
                return NULL;
            MP_WARN(mapper, "Queue underflow!\n");
        }
    }
    return p->queue[p->queue_pos];
}

static void surf_release(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    ID3D11DeviceContext_End(p->ctx11,
        (ID3D11Asynchronous *)p->queue[p->queue_pos]->idle11);

    // The current surface is now in-flight, move to the next surface
    p->queue_pos++;
    if (p->queue_pos >= p->queue_len)
        p->queue_pos = 0;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    for (int i = 0; i < p->queue_len; i++)
        surf_destroy(mapper, p->queue[i]);

    SAFE_RELEASE(p->ctx11);
    SAFE_RELEASE(p->dev9);
    SAFE_RELEASE(p->dev11);
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    HRESULT hr;

    struct queue_surf *surf = surf_acquire(mapper);
    if (!surf)
        return -1;

    RECT rc = {0, 0, mapper->src->w, mapper->src->h};
    IDirect3DSurface9* hw_surface = (IDirect3DSurface9 *)mapper->src->planes[3];

    hr = IDirect3DDevice9Ex_StretchRect(p->dev9, hw_surface, &rc, surf->surf9,
                                        &rc, D3DTEXF_NONE);
    if (FAILED(hr)) {
        MP_ERR(mapper, "StretchRect() failed: %s\n", mp_HRESULT_to_str(hr));
        return -1;
    }

    if (!surf_wait_idle9(mapper, surf))
        return -1;

    mapper->tex[0] = surf->tex;
    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    if (p->queue_pos < p->queue_len &&
        p->queue[p->queue_pos]->tex == mapper->tex[0])
    {
        surf_release(mapper);
        mapper->tex[0] = NULL;
    }
}

const struct ra_hwdec_driver ra_hwdec_dxva2dxgi = {
    .name = "dxva2-dxgi",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_DXVA2, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
