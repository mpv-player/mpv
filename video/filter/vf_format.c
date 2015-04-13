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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "common/msg.h"
#include "common/common.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "options/m_option.h"

struct vf_priv_s {
    int fmt;
    int outfmt;
    int colormatrix;
    int colorlevels;
    int outputlevels;
    int primaries;
    int gamma;
    int chroma_location;
    int stereo_in;
    int stereo_out;
    int rotate;
    int dw, dh;
    double dar;
};

static bool is_compatible(int fmt1, int fmt2)
{
    struct mp_imgfmt_desc d1 = mp_imgfmt_get_desc(fmt1);
    struct mp_imgfmt_desc d2 = mp_imgfmt_get_desc(fmt2);
    if (d1.num_planes < d2.num_planes)
        return false;
    if (!(d1.flags & MP_IMGFLAG_BYTE_ALIGNED) ||
        !(d2.flags & MP_IMGFLAG_BYTE_ALIGNED))
        return false;
    for (int n = 0; n < MPMIN(d1.num_planes, d2.num_planes); n++) {
        if (d1.bytes[n] != d2.bytes[n])
            return false;
        if (d1.xs[n] != d2.xs[n] || d1.ys[n] != d2.ys[n])
            return false;
    }
    return true;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (fmt == vf->priv->fmt || !vf->priv->fmt) {
        if (vf->priv->outfmt) {
            if (!is_compatible(fmt, vf->priv->outfmt))
                return 0;
            fmt = vf->priv->outfmt;
        }
        return vf_next_query_format(vf, fmt);
    }
    return 0;
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    struct vf_priv_s *p = vf->priv;

    *out = *in;

    if (p->outfmt)
        out->imgfmt = p->outfmt;
    if (p->colormatrix)
        out->colorspace = p->colormatrix;
    if (p->colorlevels)
        out->colorlevels = p->colorlevels;
    if (p->outputlevels)
        out->outputlevels = p->outputlevels;
    if (p->primaries)
        out->primaries = p->primaries;
    if (p->gamma)
        out->gamma = p->gamma;
    if (p->chroma_location)
        out->chroma_location = p->chroma_location;
    if (p->stereo_in)
        out->stereo_in = p->stereo_in;
    if (p->stereo_out)
        out->stereo_out = p->stereo_out;
    if (p->rotate >= 0)
        out->rotate = p->rotate;
    if (p->dw > 0)
        out->d_w = p->dw;
    if (p->dh > 0)
        out->d_h = p->dh;
    if (p->dar > 0)
        vf_set_dar(&out->d_w, &out->d_h, out->w, out->h, p->dar);

    // Make sure the user-overrides are consistent (no RGB csp for YUV, etc.).
    mp_image_params_guess_csp(out);

    return 0;
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    if (vf->priv->outfmt)
        mp_image_setfmt(mpi, vf->priv->outfmt);
    return mpi;
}

static int vf_open(vf_instance_t *vf)
{
    vf->query_format = query_format;
    vf->reconfig = reconfig;
    vf->filter = filter;
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_IMAGEFORMAT("fmt", fmt, 0),
    OPT_IMAGEFORMAT("outfmt", outfmt, 0),
    OPT_CHOICE_C("colormatrix", colormatrix, 0, mp_csp_names),
    OPT_CHOICE_C("colorlevels", colorlevels, 0, mp_csp_levels_names),
    OPT_CHOICE_C("outputlevels", outputlevels, 0, mp_csp_levels_names),
    OPT_CHOICE_C("primaries", primaries, 0, mp_csp_prim_names),
    OPT_CHOICE_C("gamma", gamma, 0, mp_csp_trc_names),
    OPT_CHOICE_C("chroma-location", chroma_location, 0, mp_chroma_names),
    OPT_CHOICE_C("stereo-in", stereo_in, 0, mp_stereo3d_names),
    OPT_CHOICE_C("stereo-out", stereo_out, 0, mp_stereo3d_names),
    OPT_INTRANGE("rotate", rotate, 0, -1, 359),
    OPT_INT("dw", dw, 0),
    OPT_INT("dh", dh, 0),
    OPT_DOUBLE("dar", dar, 0),
    {0}
};

const vf_info_t vf_info_format = {
    .description = "force output format",
    .name = "format",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = vf_opts_fields,
    .priv_defaults = &(const struct vf_priv_s){
        .rotate = -1,
    },
};
