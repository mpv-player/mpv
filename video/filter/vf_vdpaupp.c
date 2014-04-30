/*
 * This file is part of mpv.
 *
 * Parts based on fragments of vo_vdpau.c: Copyright (C) 2009 Uoti Urpala
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

// Note: this filter does no actual filtering; it merely sets appropriate
//       flags on vdpau images (mp_vdpau_mixer_frame) to do the appropriate
//       processing on the final rendering process in the VO.

struct vf_priv_s {
    struct mp_vdpau_ctx *ctx;

    // This is needed to supply past/future fields and to calculate the
    // interpolated timestamp.
    struct mp_image *buffered[3];
    int num_buffered;

    int prev_pos;           // last field that was output

    int def_deintmode;
    int deint_enabled;
    struct mp_vdpau_mixer_opts opts;
};

static struct mp_image *upload(struct vf_instance *vf, struct mp_image *mpi)
{
    struct vf_priv_s *p = vf->priv;
    struct vdp_functions *vdp = &p->ctx->vdp;
    VdpStatus vdp_st;

    VdpChromaType chroma_type = (VdpChromaType)-1;
    VdpYCbCrFormat pixel_format = (VdpYCbCrFormat)-1;
    mp_vdpau_get_format(mpi->imgfmt, &chroma_type, &pixel_format);

    struct mp_image *hwmpi =
        mp_vdpau_get_video_surface(p->ctx, chroma_type, mpi->w, mpi->h);
    if (!hwmpi)
        return mpi;

    VdpVideoSurface surface = (intptr_t)hwmpi->planes[3];
    const void *destdata[3] = {mpi->planes[0], mpi->planes[2], mpi->planes[1]};
    if (mpi->imgfmt == IMGFMT_NV12)
        destdata[1] = destdata[2];
    vdp_st = vdp->video_surface_put_bits_y_cb_cr(surface,
                pixel_format, destdata, mpi->stride);
    CHECK_VDP_WARNING(vf, "Error when calling vdp_video_surface_put_bits_y_cb_cr");

    mp_image_copy_attributes(hwmpi, mpi);

    talloc_free(mpi);
    return hwmpi;
}

static void forget_frames(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    for (int n = 0; n < p->num_buffered; n++)
        talloc_free(p->buffered[n]);
    p->num_buffered = 0;
    p->prev_pos = 0;
}

#define FIELD_VALID(p, f) ((f) >= 0 && (f) < (p)->num_buffered * 2)

static VdpVideoSurface ref_frame(struct vf_priv_s *p,
                                 struct mp_vdpau_mixer_frame *frame, int pos)
{
    if (!FIELD_VALID(p, pos))
        return VDP_INVALID_HANDLE;
    struct mp_image *mpi = mp_image_new_ref(p->buffered[pos / 2]);
    talloc_steal(frame, mpi);
    return (uintptr_t)mpi->planes[3];
}

// pos==0 means last field of latest frame, 1 earlier field of latest frame,
// 2 last field of previous frame and so on
static bool output_field(struct vf_instance *vf, int pos)
{
    struct vf_priv_s *p = vf->priv;

    if (!FIELD_VALID(p, pos))
        return false;

    struct mp_image *mpi = mp_vdpau_mixed_frame_create(p->buffered[pos / 2]);
    struct mp_vdpau_mixer_frame *frame = mp_vdpau_mixed_frame_get(mpi);

    frame->field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
    if (p->opts.deint) {
        int top_field_first = 1;
        if (mpi->fields & MP_IMGFIELD_ORDERED)
            top_field_first = !!(mpi->fields & MP_IMGFIELD_TOP_FIRST);
        frame->field = top_field_first ^ (pos & 1) ?
            VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD:
            VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    }

    frame->future[0] = ref_frame(p, frame, pos - 1);
    frame->current = ref_frame(p, frame, pos);
    frame->past[0] = ref_frame(p, frame, pos + 1);
    frame->past[1] = ref_frame(p, frame, pos + 2);

    frame->opts = p->opts;

    mpi->planes[3] = (void *)(uintptr_t)frame->current;

    // Interpolate timestamps of extra fields (these always have even indexes)
    int idx = pos / 2;
    if (idx > 0 && !(pos & 1) && p->opts.deint >= 2) {
        double pts1 = p->buffered[idx - 1]->pts;
        double pts2 = p->buffered[idx]->pts;
        double diff = pts1 - pts2;
        mpi->pts = diff > 0 && diff < 0.5 ? (pts1 + pts2) / 2 : pts2;
    }

    vf_add_output_frame(vf, mpi);
    return true;
}

static int filter_ext(struct vf_instance *vf, struct mp_image *mpi)
{
    struct vf_priv_s *p = vf->priv;
    int maxbuffer = p->opts.deint ? MP_ARRAY_SIZE(p->buffered) : 2;
    bool eof = !mpi;

    if (mpi && mpi->imgfmt != IMGFMT_VDPAU) {
        mpi = upload(vf, mpi);
        if (mpi->imgfmt != IMGFMT_VDPAU) {
            talloc_free(mpi);
            return -1;
        }
    }

    if (mpi) {
        if (mpi->planes[2]) {
            MP_ERR(vf, "Can't apply vdpaupp filter multiple times.\n");
            vf_add_output_frame(vf, mpi);
            return -1;
        }

        while (p->num_buffered >= maxbuffer) {
            talloc_free(p->buffered[p->num_buffered - 1]);
            p->num_buffered--;
        }
        for (int n = p->num_buffered; n > 0; n--)
            p->buffered[n] = p->buffered[n - 1];
        p->buffered[0] = mpi;
        p->num_buffered++;
        p->prev_pos += 2;
    }

    while (1) {
        int current = p->prev_pos - 1;
        if (current < 0)
            break;
        // Wait for enough future frames being buffered.
        // (Past frames should be around if available at all.)
        if (p->opts.deint && !eof && !FIELD_VALID(p, current - 1))
            break;
        // No field-splitting deinterlace -> only output first field (odd index)
        if (p->opts.deint >= 2 || (current & 1)) {
            if (!output_field(vf, current))
                break;
        }
        p->prev_pos = current;
    }

    return 0;
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    forget_frames(vf);
    *out = *in;
    out->imgfmt = IMGFMT_VDPAU;
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
        forget_frames(vf);
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
    forget_frames(vf);
}

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;

    vf->reconfig = reconfig;
    vf->filter_ext = filter_ext;
    vf->filter = NULL;
    vf->query_format = query_format;
    vf->control = control;
    vf->uninit = uninit;

    hwdec_request_api(vf->hwdec, "vdpau");
    p->ctx = vf->hwdec ? vf->hwdec->vdpau_ctx : NULL;
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
    {0}
};

const vf_info_t vf_info_vdpaupp = {
    .description = "vdpau postprocessing",
    .name = "vdpaupp",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = vf_opts_fields,
};
