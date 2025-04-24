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

#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>

#include "config.h"

#include "video/fmt-conversion.h"
#include "video/out/gpu/hwdec.h"

struct priv_owner {
    struct mp_hwdec_ctx hwctx;
    int *formats;
};

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;
    if (p->hwctx.driver_name)
        hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);
}

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;


    MP_VERBOSE(hw, "Using auto detect video device\n");

    int ret = av_hwdevice_ctx_create(&p->hwctx.av_device_ref,
                                     AV_HWDEVICE_TYPE_V4L2REQUEST,
                                     NULL, NULL, 0);
    if (ret != 0) {
        MP_VERBOSE(hw, "Failed to create hwdevice_ctx: %s\n", av_err2str(ret));
        return -1;
    }

    /*
     * At the moment, there is no way to discover compatible formats
     * from the hwdevice_ctx, and in fact the ffmpeg hwaccels hard-code
     * formats too, so we're not missing out on anything.
     */
    int num_formats = 0;
    MP_TARRAY_APPEND(p, p->formats, num_formats, IMGFMT_NV12);
    MP_TARRAY_APPEND(p, p->formats, num_formats, IMGFMT_420P);
    MP_TARRAY_APPEND(p, p->formats, num_formats, pixfmt2imgfmt(AV_PIX_FMT_NV16));
    MP_TARRAY_APPEND(p, p->formats, num_formats, IMGFMT_P010);
#ifdef AV_PIX_FMT_P210
    MP_TARRAY_APPEND(p, p->formats, num_formats, pixfmt2imgfmt(AV_PIX_FMT_P210));
#endif


    MP_TARRAY_APPEND(p, p->formats, num_formats, 0); // terminate it

    p->hwctx.hw_imgfmt = IMGFMT_DRMPRIME;
    p->hwctx.supported_formats = p->formats;
    p->hwctx.driver_name = hw->driver->name;
    hwdec_devices_add(hw->devs, &p->hwctx);

    return 0;
}

const struct ra_hwdec_driver ra_hwdec_v4l2request = {
    .name = "v4l2request",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_DRMPRIME, 0},
    .device_type = AV_HWDEVICE_TYPE_V4L2REQUEST,
    .init = init,
    .uninit = uninit,
};
