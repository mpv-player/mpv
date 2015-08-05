/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
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
    VdpStatus vdp_st;
    struct vf_priv_s *p = vf->priv;
    struct mp_vdpau_ctx *ctx = p->ctx;
    struct vdp_functions *vdp = &ctx->vdp;

    if (!mpi) {
        return 0;
    }

    if (mp_vdpau_mixed_frame_get(mpi)) {
        MP_ERR(vf, "Can't apply vdpaurb filter after vdpaupp filter.\n");
        mp_image_unrefp(&mpi);
        return -1;
    }

    struct mp_image *out = vf_alloc_out_image(vf);
    if (!out) {
        mp_image_unrefp(&mpi);
        return -1;
    }
    mp_image_copy_attributes(out, mpi);

    VdpVideoSurface surface = (uintptr_t)mpi->planes[3];
    assert(surface > 0);

    vdp_st = vdp->video_surface_get_bits_y_cb_cr(surface,
                                                 VDP_YCBCR_FORMAT_NV12,
                                                 (void * const *)out->planes,
                                                 out->stride);
    CHECK_VDP_WARNING(vf, "Error when calling vdp_output_surface_get_bits_y_cb_cr");

    vf_add_output_frame(vf, out);
    mp_image_unrefp(&mpi);
    return 0;
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    *out = *in;
    out->imgfmt = IMGFMT_NV12;
    return 0;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (fmt == IMGFMT_VDPAU) {
        return vf_next_query_format(vf, IMGFMT_NV12);
    }
    return 0;
}

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    vf->filter_ext = filter_ext;
    vf->filter = NULL;
    vf->reconfig = reconfig;
    vf->query_format = query_format;

    if (!vf->hwdec) {
        return 0;
    }
    hwdec_request_api(vf->hwdec, "vdpau");
    p->ctx = vf->hwdec->hwctx ? vf->hwdec->hwctx->vdpau_ctx : NULL;
    if (!p->ctx) {
        return 0;
    }

    return 1;
}

const vf_info_t vf_info_vdpaurb = {
    .description = "vdpau readback",
    .name = "vdpaurb",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
};
