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
#include "video/out/gpu/hwdec.h"
#include "ra_gl.h"
#include "video/hwdec.h"
#include "video/d3d.h"

// for  WGL_ACCESS_READ_ONLY_NV
#include <GL/wglext.h>

#define SHARED_SURFACE_D3DFMT D3DFMT_X8R8G8B8

struct priv_owner {
    struct mp_hwdec_ctx hwctx;
    IDirect3DDevice9Ex *device;
    HANDLE device_h;
};

struct priv {
    IDirect3DDevice9Ex *device;
    HANDLE device_h;
    IDirect3DSurface9 *rtarget;
    HANDLE rtarget_h;
    GLuint texture;
};

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);

    if (p->device)
        IDirect3DDevice9Ex_Release(p->device);
}

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    if (!ra_is_gl(hw->ra))
        return -1;
    GL *gl = ra_gl_get(hw->ra);
    if (!(gl->mpgl_caps & MPGL_CAP_DXINTEROP))
        return -1;

    // AMD drivers won't open multiple dxinterop HANDLES on the same D3D device,
    // so we request the one already in use by context_dxinterop
    p->device_h = ra_get_native_resource(hw->ra, "dxinterop_device_HANDLE");
    if (!p->device_h)
        return -1;

    // But we also still need the actual D3D device
    p->device = ra_get_native_resource(hw->ra, "IDirect3DDevice9Ex");
    if (!p->device)
        return -1;
    IDirect3DDevice9Ex_AddRef(p->device);

    p->hwctx = (struct mp_hwdec_ctx){
        .driver_name = hw->driver->name,
        .av_device_ref = d3d9_wrap_device_ref((IDirect3DDevice9 *)p->device),
    };
    hwdec_devices_add(hw->devs, &p->hwctx);
    return 0;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);

    if (p->rtarget_h && p->device_h) {
        if (!gl->DXUnlockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
            MP_ERR(mapper, "Failed unlocking texture for access by OpenGL: %s\n",
                   mp_LastError_to_str());
        }
    }

    if (p->rtarget_h) {
        if (!gl->DXUnregisterObjectNV(p->device_h, p->rtarget_h)) {
            MP_ERR(mapper, "Failed to unregister Direct3D surface with OpenGL: %s\n",
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

    ra_tex_free(mapper->ra, &mapper->tex[0]);
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);
    HRESULT hr;

    p->device = p_owner->device;
    p->device_h = p_owner->device_h;

    HANDLE share_handle = NULL;
    hr = IDirect3DDevice9Ex_CreateRenderTarget(
        p->device,
        mapper->src_params.w, mapper->src_params.h,
        SHARED_SURFACE_D3DFMT, D3DMULTISAMPLE_NONE, 0, FALSE,
        &p->rtarget, &share_handle);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Failed creating offscreen Direct3D surface: %s\n",
               mp_HRESULT_to_str(hr));
        return -1;
    }

    if (share_handle &&
        !gl->DXSetResourceShareHandleNV(p->rtarget, share_handle)) {
        MP_ERR(mapper, "Failed setting Direct3D/OpenGL share handle for surface: %s\n",
               mp_LastError_to_str());
        return -1;
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
        MP_ERR(mapper, "Failed to register Direct3D surface with OpenGL: %s\n",
               mp_LastError_to_str());
        return -1;
    }

    if (!gl->DXLockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
        MP_ERR(mapper, "Failed locking texture for access by OpenGL %s\n",
               mp_LastError_to_str());
        return -1;
    }

    struct ra_tex_params params = {
        .dimensions = 2,
        .w = mapper->src_params.w,
        .h = mapper->src_params.h,
        .d = 1,
        .format = ra_find_unorm_format(mapper->ra, 1, 4),
        .render_src = true,
        .src_linear = true,
    };
    if (!params.format)
        return -1;

    mapper->tex[0] = ra_create_wrapped_tex(mapper->ra, &params, p->texture);
    if (!mapper->tex[0])
        return -1;

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = IMGFMT_RGB0;
    mapper->dst_params.hw_subfmt = 0;

    return 0;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = ra_gl_get(mapper->ra);
    HRESULT hr;

    if (!gl->DXUnlockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
        MP_ERR(mapper, "Failed unlocking texture for access by OpenGL: %s\n",
               mp_LastError_to_str());
        return -1;
    }

    IDirect3DSurface9* hw_surface = (IDirect3DSurface9 *)mapper->src->planes[3];
    RECT rc = {0, 0, mapper->src->w, mapper->src->h};
    hr = IDirect3DDevice9Ex_StretchRect(p->device,
                                        hw_surface, &rc,
                                        p->rtarget, &rc,
                                        D3DTEXF_NONE);
    if (FAILED(hr)) {
        MP_ERR(mapper, "Direct3D RGB conversion failed: %s", mp_HRESULT_to_str(hr));
        return -1;
    }

    if (!gl->DXLockObjectsNV(p->device_h, 1, &p->rtarget_h)) {
        MP_ERR(mapper, "Failed locking texture for access by OpenGL: %s\n",
               mp_LastError_to_str());
        return -1;
    }

    return 0;
}

const struct ra_hwdec_driver ra_hwdec_dxva2gldx = {
    .name = "dxva2-dxinterop",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_DXVA2, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
    },
};
