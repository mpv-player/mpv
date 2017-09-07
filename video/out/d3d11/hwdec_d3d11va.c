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
#include <d3d11.h>
#include <d3d11_1.h>

#include "config.h"

#include "common/common.h"
#include "osdep/windows_utils.h"
#include "video/hwdec.h"
#include "video/decode/d3d.h"
#include "video/out/d3d11/ra_d3d11.h"
#include "video/out/gpu/hwdec.h"

struct priv_owner {
    struct mp_hwdec_ctx hwctx;
    ID3D11Device *device;
    ID3D11Device1 *device1;
};

struct priv {
    ID3D11DeviceContext1 *ctx;
    ID3D11Texture2D *copy_tex;
};

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    if (p->hwctx.ctx)
        hwdec_devices_remove(hw->devs, &p->hwctx);
    SAFE_RELEASE(p->device);
    SAFE_RELEASE(p->device1);
}

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    HRESULT hr;

    if (!ra_is_d3d11(hw->ra))
        return -1;
    p->device = ra_d3d11_get_device(hw->ra);
    if (!p->device)
        return -1;

    // D3D11VA requires Direct3D 11.1, so this should always succeed
    hr = ID3D11Device_QueryInterface(p->device, &IID_ID3D11Device1,
                                     (void**)&p->device1);
    if (FAILED(hr)) {
        MP_ERR(hw, "Failed to get D3D11.1 interface: %s\n",
               mp_HRESULT_to_str(hr));
        return -1;
    }

    ID3D10Multithread *multithread;
    hr = ID3D11Device_QueryInterface(p->device, &IID_ID3D10Multithread,
                                     (void **)&multithread);
    if (FAILED(hr)) {
        MP_ERR(hw, "Failed to get Multithread interface: %s\n",
               mp_HRESULT_to_str(hr));
        return -1;
    }
    ID3D10Multithread_SetMultithreadProtected(multithread, TRUE);
    ID3D10Multithread_Release(multithread);

    p->hwctx = (struct mp_hwdec_ctx){
        .type = HWDEC_D3D11VA,
        .driver_name = hw->driver->name,
        .ctx = p->device,
        .av_device_ref = d3d11_wrap_device_ref(p->device),
    };
    hwdec_devices_add(hw->devs, &p->hwctx);
    return 0;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    for (int i = 0; i < 4; i++)
        ra_tex_free(mapper->ra, &mapper->tex[i]);
    SAFE_RELEASE(p->copy_tex);
    SAFE_RELEASE(p->ctx);
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *o = mapper->owner->priv;
    struct priv *p = mapper->priv;
    HRESULT hr;

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params.hw_subfmt = 0;

    struct ra_imgfmt_desc desc = {0};
    struct mp_image layout = {0};

    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &desc))
        return -1;

    mp_image_set_params(&layout, &mapper->dst_params);

    DXGI_FORMAT copy_fmt;
    switch (mapper->dst_params.imgfmt) {
    case IMGFMT_NV12: copy_fmt = DXGI_FORMAT_NV12; break;
    case IMGFMT_P010: copy_fmt = DXGI_FORMAT_P010; break;
    default: return -1;
    }

    // We copy decoder images to an intermediate texture. This is slower than
    // the zero-copy path, but according to MSDN, decoder textures should not
    // be bound to SRVs, so it is technically correct, and it works around some
    // driver "bugs" that can happen with the zero-copy path. It also allows
    // samplers to work correctly when the decoder image includes padding.
    D3D11_TEXTURE2D_DESC copy_desc = {
        .Width = mapper->dst_params.w,
        .Height = mapper->dst_params.h,
        .MipLevels = 1,
        .ArraySize = 1,
        .SampleDesc.Count = 1,
        .Format = copy_fmt,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };
    hr = ID3D11Device_CreateTexture2D(o->device, &copy_desc, NULL, &p->copy_tex);
    if (FAILED(hr)) {
        MP_FATAL(mapper, "Could not create shader resource texture\n");
        return -1;
    }

    for (int i = 0; i < desc.num_planes; i++) {
        mapper->tex[i] = ra_d3d11_wrap_tex_video(mapper->ra, p->copy_tex,
                                                 mp_image_plane_w(&layout, i),
                                                 mp_image_plane_h(&layout, i),
                                                 desc.planes[i]);
        if (!mapper->tex[i]) {
            MP_FATAL(mapper, "Could not create RA texture view\n");
            return -1;
        }
    }

    ID3D11Device1_GetImmediateContext1(o->device1, &p->ctx);

    return 0;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    ID3D11Texture2D *tex = (void *)mapper->src->planes[0];
    int subresource = (intptr_t)mapper->src->planes[1];

    ID3D11DeviceContext1_CopySubresourceRegion1(p->ctx,
        (ID3D11Resource *)p->copy_tex, 0, 0, 0, 0,
        (ID3D11Resource *)tex, subresource, (&(D3D11_BOX) {
            .left = 0,
            .top = 0,
            .front = 0,
            .right = mapper->dst_params.w,
            .bottom = mapper->dst_params.h,
            .back = 1,
        }), D3D11_COPY_DISCARD);

    return 0;
}

const struct ra_hwdec_driver ra_hwdec_d3d11va = {
    .name = "d3d11va",
    .priv_size = sizeof(struct priv_owner),
    .api = HWDEC_D3D11VA,
    .imgfmts = {IMGFMT_D3D11VA, IMGFMT_D3D11NV12, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
    },
};
