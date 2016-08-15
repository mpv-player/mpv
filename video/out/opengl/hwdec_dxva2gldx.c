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

#include <d3d9.h>
#include <assert.h>

#include "common/common.h"
#include "osdep/windows_utils.h"
#include "hwdec.h"
#include "video/hwdec.h"

// for  WGL_ACCESS_READ_ONLY_NV
#include <GL/wglext.h>

#define SHARED_SURFACE_D3DFMT D3DFMT_X8R8G8B8
#define SHARED_SURFACE_MPFMT  IMGFMT_RGB0
struct priv {
    struct mp_hwdec_ctx hwctx;
    IDirect3DDevice9Ex *device;
    HANDLE device_h;

    IDirect3DSurface9 *rtarget;
    HANDLE rtarget_h;
    GLuint texture;
};

static void destroy_objects(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;

    if (p->rtarget_h && p->device_h) {
        if (!gl->DXUnlockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
            MP_ERR(hw, "Failed unlocking texture for access by OpenGL: %s\n",
                   mp_LastError_to_str());
        }
    }

    if (p->rtarget_h) {
        if (!gl->DXUnregisterObjectNV(p->device_h, p->rtarget_h)) {
            MP_ERR(hw, "Failed to unregister Direct3D surface with OpenGL: %s\n",
                   mp_LastError_to_str());
        } else {
            p->rtarget_h = 0;
        }
    }

    gl->DeleteTextures(1, &p->texture);
    p->texture = 0;

    if (p->rtarget) {
        IDirect3DSurface9_Release(p->rtarget);
        p->rtarget = NULL;
    }
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    destroy_objects(hw);

    hwdec_devices_remove(hw->devs, &p->hwctx);

    if (p->device)
        IDirect3DDevice9Ex_Release(p->device);
}

static int create(struct gl_hwdec *hw)
{
    GL *gl = hw->gl;
    if (!gl->MPGetNativeDisplay || !(gl->mpgl_caps & MPGL_CAP_DXINTEROP))
        return -1;

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    // AMD drivers won't open multiple dxinterop HANDLES on the same D3D device,
    // so we request the one already in use by context_dxinterop
    p->device_h = gl->MPGetNativeDisplay("dxinterop_device_HANDLE");
    if (!p->device_h)
        return -1;

    // But we also still need the actual D3D device
    p->device = gl->MPGetNativeDisplay("IDirect3DDevice9Ex");
    if (!p->device)
        return -1;
    IDirect3DDevice9Ex_AddRef(p->device);

    p->hwctx = (struct mp_hwdec_ctx){
        .type = HWDEC_DXVA2,
        .driver_name = hw->driver->name,
        .ctx = (IDirect3DDevice9 *)p->device,
    };
    hwdec_devices_add(hw->devs, &p->hwctx);
    return 0;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->gl;
    HRESULT hr;

    destroy_objects(hw);

    HANDLE share_handle = NULL;
    hr = IDirect3DDevice9Ex_CreateRenderTarget(
        p->device,
        params->w, params->h,
        SHARED_SURFACE_D3DFMT, D3DMULTISAMPLE_NONE, 0, FALSE,
        &p->rtarget, &share_handle);
    if (FAILED(hr)) {
        MP_ERR(hw, "Failed creating offscreen Direct3D surface: %s\n",
               mp_HRESULT_to_str(hr));
        goto fail;
    }

    if (share_handle &&
        !gl->DXSetResourceShareHandleNV(p->rtarget, share_handle)) {
        MP_ERR(hw, "Failed setting Direct3D/OpenGL share handle for surface: %s\n",
               mp_LastError_to_str());
        goto fail;
    }

    gl->GenTextures(1, &p->texture);
    gl->BindTexture(GL_TEXTURE_2D, p->texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    p->rtarget_h = gl->DXRegisterObjectNV(p->device_h, p->rtarget, p->texture,
                                          GL_TEXTURE_2D,
                                          WGL_ACCESS_READ_ONLY_NV);
    if (!p->rtarget_h) {
        MP_ERR(hw, "Failed to register Direct3D surface with OpenGL: %s\n",
               mp_LastError_to_str());
        goto fail;
    }

    if (!gl->DXLockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
        MP_ERR(hw, "Failed locking texture for access by OpenGL %s\n",
               mp_LastError_to_str());
        goto fail;
    }

    params->imgfmt = SHARED_SURFACE_MPFMT;
    params->hw_subfmt = 0;

    return 0;
fail:
    destroy_objects(hw);
    return -1;
}

static int map_frame(struct gl_hwdec *hw, struct mp_image *hw_image,
                     struct gl_hwdec_frame *out_frame)
{
    assert(hw_image && hw_image->imgfmt == hw->driver->imgfmt);
    GL *gl = hw->gl;
    struct priv *p = hw->priv;
    HRESULT hr;

    if (!gl->DXUnlockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
        MP_ERR(hw, "Failed unlocking texture for access by OpenGL: %s\n",
               mp_LastError_to_str());
        return -1;
    }

    IDirect3DSurface9* hw_surface = (IDirect3DSurface9 *)hw_image->planes[3];
    RECT rc = {0, 0, hw_image->w, hw_image->h};
    hr = IDirect3DDevice9Ex_StretchRect(p->device,
                                        hw_surface, &rc,
                                        p->rtarget, &rc,
                                        D3DTEXF_NONE);
    if (FAILED(hr)) {
        MP_ERR(hw, "Direct3D RGB conversion failed: %s", mp_HRESULT_to_str(hr));
        return -1;
    }

    if (!gl->DXLockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
        MP_ERR(hw, "Failed locking texture for access by OpenGL: %s\n",
               mp_LastError_to_str());
        return -1;
    }

    *out_frame = (struct gl_hwdec_frame){
        .planes = {
            {
                .gl_texture = p->texture,
                .gl_target = GL_TEXTURE_2D,
                .tex_w = hw_image->w,
                .tex_h = hw_image->h,
            },
        },
    };
    return 0;
}

const struct gl_hwdec_driver gl_hwdec_dxva2gldx = {
    .name = "dxva2-dxinterop",
    .api = HWDEC_DXVA2,
    .imgfmt = IMGFMT_DXVA2,
    .create = create,
    .reinit = reinit,
    .map_frame = map_frame,
    .destroy = destroy,
};
