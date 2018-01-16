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
#include "options/m_config.h"
#include "osdep/windows_utils.h"
#include "video/hwdec.h"
#include "video/d3d.h"
#include "video/out/d3d11/ra_d3d11.h"
#include "video/out/gpu/hwdec.h"

struct d3d11va_opts {
    int zero_copy;
};

#define OPT_BASE_STRUCT struct d3d11va_opts
const struct m_sub_options d3d11va_conf = {
    .opts = (const struct m_option[]) {
        OPT_FLAG("d3d11va-zero-copy", zero_copy, 0),
        {0}
    },
    .defaults = &(const struct d3d11va_opts) {
        .zero_copy = 0,
    },
    .size = sizeof(struct d3d11va_opts)
};

struct priv_owner {
    struct d3d11va_opts *opts;

    struct mp_hwdec_ctx hwctx;
    ID3D11Device *device;
    ID3D11Device1 *device1;
};

struct priv {
    // 1-copy path
    ID3D11DeviceContext1 *ctx;
    ID3D11Texture2D *copy_tex;

    // zero-copy path
    int num_planes;
    const struct ra_format *fmt[4];
};

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
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

    p->opts = mp_get_config_group(hw->priv, hw->global, &d3d11va_conf);

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

    static const int subfmts[] = {IMGFMT_NV12, IMGFMT_P010, 0};
    p->hwctx = (struct mp_hwdec_ctx){
        .driver_name = hw->driver->name,
        .av_device_ref = d3d11_wrap_device_ref(p->device),
        .supported_formats = subfmts,
        .hw_imgfmt = IMGFMT_D3D11,
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

    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &desc))
        return -1;

    if (o->opts->zero_copy) {
        // In the zero-copy path, we create the ra_tex objects in the map
        // operation, so we just need to store the format of each plane
        p->num_planes = desc.num_planes;
        for (int i = 0; i < desc.num_planes; i++)
            p->fmt[i] = desc.planes[i];
    } else {
        struct mp_image layout = {0};
        mp_image_set_params(&layout, &mapper->dst_params);

        DXGI_FORMAT copy_fmt;
        switch (mapper->dst_params.imgfmt) {
        case IMGFMT_NV12: copy_fmt = DXGI_FORMAT_NV12; break;
        case IMGFMT_P010: copy_fmt = DXGI_FORMAT_P010; break;
        default: return -1;
        }

        D3D11_TEXTURE2D_DESC copy_desc = {
            .Width = mapper->dst_params.w,
            .Height = mapper->dst_params.h,
            .MipLevels = 1,
            .ArraySize = 1,
            .SampleDesc.Count = 1,
            .Format = copy_fmt,
            .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        };
        hr = ID3D11Device_CreateTexture2D(o->device, &copy_desc, NULL,
                                          &p->copy_tex);
        if (FAILED(hr)) {
            MP_FATAL(mapper, "Could not create shader resource texture\n");
            return -1;
        }

        for (int i = 0; i < desc.num_planes; i++) {
            mapper->tex[i] = ra_d3d11_wrap_tex_video(mapper->ra, p->copy_tex,
                mp_image_plane_w(&layout, i), mp_image_plane_h(&layout, i), 0,
                desc.planes[i]);
            if (!mapper->tex[i]) {
                MP_FATAL(mapper, "Could not create RA texture view\n");
                return -1;
            }
        }

        // A ref to the immediate context is needed for CopySubresourceRegion
        ID3D11Device1_GetImmediateContext1(o->device1, &p->ctx);
    }

    return 0;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    ID3D11Texture2D *tex = (void *)mapper->src->planes[0];
    int subresource = (intptr_t)mapper->src->planes[1];

    if (p->copy_tex) {
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
    } else {
        D3D11_TEXTURE2D_DESC desc2d;
        ID3D11Texture2D_GetDesc(tex, &desc2d);

        for (int i = 0; i < p->num_planes; i++) {
            // The video decode texture may include padding, so the size of the
            // ra_tex needs to be determined by the actual size of the Tex2D
            bool chroma = i >= 1;
            int w = desc2d.Width / (chroma ? 2 : 1);
            int h = desc2d.Height / (chroma ? 2 : 1);

            mapper->tex[i] = ra_d3d11_wrap_tex_video(mapper->ra, tex,
                w, h, subresource, p->fmt[i]);
            if (!mapper->tex[i])
                return -1;
        }
    }

    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    if (p->copy_tex)
        return;
    for (int i = 0; i < 4; i++)
        ra_tex_free(mapper->ra, &mapper->tex[i]);
}

const struct ra_hwdec_driver ra_hwdec_d3d11va = {
    .name = "d3d11va",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_D3D11, 0},
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
