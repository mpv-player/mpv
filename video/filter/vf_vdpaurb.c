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

#include <assert.h>

#include "video/vdpau.h"
#include "video/vdpau_mixer.h"
#include "vf.h"

// This filter will read back decoded frames that have been decoded by vdpau
// so they can be post-processed by regular filters. As vdpau is still doing
// the decoding, a vdpau compatible vo must always be used.
//
// NB: This filter assumes the video surface will have a 420 chroma type and
// can always be read back in NV12 format. This is a safe assumption at the
// time of writing, but may not always remain true.

struct vf_priv_s {
    struct mp_vdpau_ctx *ctx;
};

static int filter_ext(struct vf_instance *vf, struct mp_image *mpi)
{
    struct vf_priv_s *p = vf->priv;

    if (!mpi) {
        return 0;
    }

    // Pass-through anything that's not been decoded by VDPAU
    if (mpi->imgfmt != IMGFMT_VDPAU) {
        vf_add_output_frame(vf, mpi);
        return 0;
    }

    if (mp_vdpau_mixed_frame_get(mpi)) {
        MP_ERR(vf, "Can't apply vdpaurb filter after vdpaupp filter.\n");
        mp_image_unrefp(&mpi);
        return -1;
    }

    struct mp_hwdec_ctx *hwctx = &p->ctx->hwctx;

    struct mp_image *out = hwctx->download_image(hwctx, mpi, vf->out_pool);
    if (!out || out->imgfmt != IMGFMT_NV12) {
        mp_image_unrefp(&mpi);
        mp_image_unrefp(&out);
        return -1;
    }

    vf_add_output_frame(vf, out);
    mp_image_unrefp(&mpi);
    return 0;
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    *out = *in;
    if (in->imgfmt == IMGFMT_VDPAU) {
        out->imgfmt = IMGFMT_NV12;
        out->hw_subfmt = 0;
    }
    return 0;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    return vf_next_query_format(vf, fmt == IMGFMT_VDPAU ? IMGFMT_NV12 : fmt);
}

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    vf->filter_ext = filter_ext;
    vf->filter = NULL;
    vf->reconfig = reconfig;
    vf->query_format = query_format;

    p->ctx = hwdec_devices_load(vf->hwdec_devs, HWDEC_VDPAU);
    if (!p->ctx)
        return 0;

    return 1;
}

const vf_info_t vf_info_vdpaurb = {
    .description = "vdpau readback",
    .name = "vdpaurb",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
};
