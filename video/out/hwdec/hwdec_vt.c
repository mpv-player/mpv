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

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_videotoolbox.h>

#include "config.h"

#include "video/out/gpu/hwdec.h"
#include "video/out/hwdec/hwdec_vt.h"

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    hwdec_devices_remove(hw->devs, &p->hwctx);
    av_buffer_unref(&p->hwctx.av_device_ref);
}

static const vt_interop_init interop_inits[] = {
#if HAVE_VIDEOTOOLBOX_GL || HAVE_IOS_GL
    vt_gl_init,
#endif
#if HAVE_VIDEOTOOLBOX_PL
    vt_pl_init,
#endif
    NULL
};

static int init(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    for (int i = 0; interop_inits[i]; i++) {
        if (interop_inits[i](hw)) {
            break;
        }
    }

    if (!p->interop_map || !p->interop_unmap) {
        MP_VERBOSE(hw, "VT hwdec only works with OpenGL or Vulkan backends.\n");
        return -1;
    }

    p->hwctx = (struct mp_hwdec_ctx){
        .driver_name = hw->driver->name,
        .hw_imgfmt = IMGFMT_VIDEOTOOLBOX,
    };

    int ret = av_hwdevice_ctx_create(&p->hwctx.av_device_ref,
                                     AV_HWDEVICE_TYPE_VIDEOTOOLBOX, NULL, NULL, 0);
    if (ret != 0) {
        MP_VERBOSE(hw, "Failed to create hwdevice_ctx: %s\n", av_err2str(ret));
        return -1;
    }

    hwdec_devices_add(hw->devs, &p->hwctx);

    return 0;
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;

    p_owner->interop_unmap(mapper);
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    if (p_owner->interop_uninit) {
        p_owner->interop_uninit(mapper);
    }
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;

    mapper->dst_params = mapper->src_params;
    mapper->dst_params.imgfmt = mapper->src_params.hw_subfmt;
    mapper->dst_params.hw_subfmt = 0;

    if (!mapper->dst_params.imgfmt) {
        MP_ERR(mapper, "Unsupported CVPixelBuffer format.\n");
        return -1;
    }

    if (!ra_get_imgfmt_desc(mapper->ra, mapper->dst_params.imgfmt, &p->desc)) {
        MP_ERR(mapper, "Unsupported texture format.\n");
        return -1;
    }

    if (p_owner->interop_init)
        return p_owner->interop_init(mapper);

    return 0;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;

    return p_owner->interop_map(mapper);
}

const struct ra_hwdec_driver ra_hwdec_videotoolbox = {
    .name = "videotoolbox",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_VIDEOTOOLBOX, 0},
    .device_type = AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
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
