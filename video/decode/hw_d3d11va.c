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
#include <libavutil/mem.h>

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
#include <libavutil/hwcontext_d3d11va.h>

static void d3d11_destroy_dev(struct mp_hwdec_ctx *ctx)
{
    av_buffer_unref(&ctx->av_device_ref);
    ID3D11Device_Release((ID3D11Device *)ctx->ctx);
    talloc_free(ctx);
}

static struct mp_hwdec_ctx *d3d11_create_dev(struct mpv_global *global,
                                             struct mp_log *plog, bool probing)
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

    struct mp_hwdec_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct mp_hwdec_ctx) {
        .type = HWDEC_D3D11VA_COPY,
        .ctx = device,
        .destroy = d3d11_destroy_dev,
        .av_device_ref = d3d11_wrap_device_ref(device),
    };

    if (!ctx->av_device_ref) {
        mp_err(plog, "Failed to allocate AVHWDeviceContext.\n");
        d3d11_destroy_dev(ctx);
        return NULL;
    }

    return ctx;
}

static struct mp_image *d3d11_update_image_attribs(struct lavc_ctx *s,
                                                   struct mp_image *img)
{
    if (img->params.hw_subfmt == IMGFMT_NV12)
        mp_image_setfmt(img, IMGFMT_D3D11NV12);

    return img;
}

const struct vd_lavc_hwdec mp_vd_lavc_d3d11va = {
    .type = HWDEC_D3D11VA,
    .image_format = IMGFMT_D3D11VA,
    .generic_hwaccel = true,
    .set_hwframes = true,
    .hwframes_refine = d3d_hwframes_refine,
    .process_image = d3d11_update_image_attribs,
};

const struct vd_lavc_hwdec mp_vd_lavc_d3d11va_copy = {
    .type = HWDEC_D3D11VA_COPY,
    .copying = true,
    .image_format = IMGFMT_D3D11VA,
    .generic_hwaccel = true,
    .create_dev = d3d11_create_dev,
    .set_hwframes = true,
    .hwframes_refine = d3d_hwframes_refine,
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};
