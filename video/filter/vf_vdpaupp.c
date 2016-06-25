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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/hwdec.h"
#include "video/vdpau.h"
#include "video/vdpau_mixer.h"
#include "vf.h"
#include "refqueue.h"

// Note: this filter does no actual filtering; it merely sets appropriate
//       flags on vdpau images (mp_vdpau_mixer_frame) to do the appropriate
//       processing on the final rendering process in the VO.

struct vf_priv_s {
    struct mp_vdpau_ctx *ctx;
    struct mp_refqueue *queue;

    int def_deintmode;
    int deint_enabled;
    int interlaced_only;
    struct mp_vdpau_mixer_opts opts;
};

static int filter_ext(struct vf_instance *vf, struct mp_image *mpi)
{
    struct vf_priv_s *p = vf->priv;

    if (p->opts.deint >= 2) {
        mp_refqueue_set_refs(p->queue, 1, 1); // 2 past fields, 1 future field
    } else {
        mp_refqueue_set_refs(p->queue, 0, 0);
    }
    mp_refqueue_set_mode(p->queue,
        (p->deint_enabled ? MP_MODE_DEINT : 0) |
        (p->interlaced_only ? MP_MODE_INTERLACED_ONLY : 0) |
        (p->opts.deint >= 2 ? MP_MODE_OUTPUT_FIELDS : 0));

    if (mpi) {
        struct mp_image *new = mp_vdpau_upload_video_surface(p->ctx, mpi);
        talloc_free(mpi);
        if (!new)
            return -1;
        mpi = new;

        if (mp_vdpau_mixed_frame_get(mpi)) {
            MP_ERR(vf, "Can't apply vdpaupp filter multiple times.\n");
            vf_add_output_frame(vf, mpi);
            return -1;
        }
    }

    mp_refqueue_add_input(p->queue, mpi);
    return 0;
}

static VdpVideoSurface ref_field(struct vf_priv_s *p,
                                 struct mp_vdpau_mixer_frame *frame, int pos)
{
    struct mp_image *mpi = mp_image_new_ref(mp_refqueue_get_field(p->queue, pos));
    if (!mpi)
        return VDP_INVALID_HANDLE;
    talloc_steal(frame, mpi);
    return (uintptr_t)mpi->planes[3];
}

static int filter_out(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    if (!mp_refqueue_has_output(p->queue))
        return 0;

    struct mp_image *mpi =
        mp_vdpau_mixed_frame_create(mp_refqueue_get_field(p->queue, 0));
    if (!mpi)
        return -1; // OOM
    struct mp_vdpau_mixer_frame *frame = mp_vdpau_mixed_frame_get(mpi);

    if (!mp_refqueue_should_deint(p->queue)) {
        frame->field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
    } else if (mp_refqueue_is_top_field(p->queue)) {
        frame->field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    } else {
        frame->field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD;
    }

    frame->future[0] = ref_field(p, frame, 1);
    frame->current = ref_field(p, frame, 0);
    frame->past[0] = ref_field(p, frame, -1);
    frame->past[1] = ref_field(p, frame, -2);

    frame->opts = p->opts;

    mpi->planes[3] = (void *)(uintptr_t)frame->current;

    mp_refqueue_next_field(p->queue);

    vf_add_output_frame(vf, mpi);
    return 0;
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    struct vf_priv_s *p = vf->priv;
    mp_refqueue_flush(p->queue);
    *out = *in;
    out->imgfmt = IMGFMT_VDPAU;
    out->hw_subfmt = 0;
    return 0;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (fmt == IMGFMT_VDPAU || mp_vdpau_get_format(fmt, NULL, NULL))
        return vf_next_query_format(vf, IMGFMT_VDPAU);
    return 0;
}

static int control(vf_instance_t *vf, int request, void *data)
{
    struct vf_priv_s *p = vf->priv;

    switch (request) {
    case VFCTRL_SEEK_RESET:
        mp_refqueue_flush(p->queue);
        return CONTROL_OK;
    case VFCTRL_GET_DEINTERLACE:
        *(int *)data = !!p->deint_enabled;
        return true;
    case VFCTRL_SET_DEINTERLACE:
        p->deint_enabled = !!*(int *)data;
        p->opts.deint = p->deint_enabled ? p->def_deintmode : 0;
        return true;
    }
    return CONTROL_UNKNOWN;
}

static void uninit(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    mp_refqueue_free(p->queue);
}

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    vf->reconfig = reconfig;
    vf->filter_ext = filter_ext;
    vf->filter_out = filter_out;
    vf->query_format = query_format;
    vf->control = control;
    vf->uninit = uninit;

    p->queue = mp_refqueue_alloc();

    p->ctx = hwdec_devices_load(vf->hwdec_devs, HWDEC_VDPAU);
    if (!p->ctx)
        return 0;

    p->def_deintmode = p->opts.deint;
    if (!p->deint_enabled)
        p->opts.deint = 0;

    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_CHOICE("deint-mode", opts.deint, 0,
               ({"first-field", 1},
                {"bob", 2},
                {"temporal", 3},
                {"temporal-spatial", 4}),
               OPTDEF_INT(3)),
    OPT_FLAG("deint", deint_enabled, 0),
    OPT_FLAG("chroma-deint", opts.chroma_deint, 0, OPTDEF_INT(1)),
    OPT_FLAG("pullup", opts.pullup, 0),
    OPT_FLOATRANGE("denoise", opts.denoise, 0, 0, 1),
    OPT_FLOATRANGE("sharpen", opts.sharpen, 0, -1, 1),
    OPT_INTRANGE("hqscaling", opts.hqscaling, 0, 0, 9),
    OPT_FLAG("interlaced-only", interlaced_only, 0, OPTDEF_INT(1)),
    {0}
};

const vf_info_t vf_info_vdpaupp = {
    .description = "vdpau postprocessing",
    .name = "vdpaupp",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = vf_opts_fields,
};
