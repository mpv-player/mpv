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
#include <math.h>
#include <inttypes.h>
#include <stdarg.h>
#include <assert.h>

#include <libswscale/swscale.h>

#include "common/av_common.h"
#include "common/msg.h"

#include "options/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "video/fmt-conversion.h"
#include "vf.h"

struct vf_priv_s {
    struct mp_sws_context *sws;
};

static int find_best_out(vf_instance_t *vf, int in_format)
{
    int best = 0;
    for (int out_format = IMGFMT_START; out_format < IMGFMT_END; out_format++) {
        if (!vf_next_query_format(vf, out_format))
            continue;
        if (sws_isSupportedOutput(imgfmt2pixfmt(out_format)) < 1)
            continue;
        if (best) {
            int candidate = mp_imgfmt_select_best(best, out_format, in_format);
            if (candidate)
                best = candidate;
        } else {
            best = out_format;
        }
    }
    return best;
}

static int reconfig(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out)
{
    unsigned int best = find_best_out(vf, in->imgfmt);
    if (!best) {
        MP_WARN(vf, "no supported output format found\n");
        return -1;
    }

    *out = *in;
    out->imgfmt = best;

    // If we convert from RGB to YUV, default to limited range.
    if (mp_imgfmt_get_forced_csp(in->imgfmt) == MP_CSP_RGB &&
        mp_imgfmt_get_forced_csp(out->imgfmt) == MP_CSP_AUTO)
        out->color.levels = MP_CSP_LEVELS_TV;

    mp_image_params_guess_csp(out);

    mp_sws_set_from_cmdline(vf->priv->sws, vf->chain->global);
    vf->priv->sws->src = *in;
    vf->priv->sws->dst = *out;

    if (mp_sws_reinit(vf->priv->sws) < 0) {
        // error...
        MP_WARN(vf, "Couldn't init libswscale for this setup\n");
        return -1;
    }
    return 0;
}

static struct mp_image *filter(struct vf_instance *vf, struct mp_image *mpi)
{
    struct mp_image *dmpi = vf_alloc_out_image(vf);
    if (!dmpi)
        return NULL;
    mp_image_copy_attributes(dmpi, mpi);

    mp_sws_scale(vf->priv->sws, dmpi, mpi);

    talloc_free(mpi);
    return dmpi;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (IMGFMT_IS_HWACCEL(fmt) || sws_isSupportedInput(imgfmt2pixfmt(fmt)) < 1)
        return 0;
    return !!find_best_out(vf, fmt);
}

static void uninit(struct vf_instance *vf)
{
}

static int vf_open(vf_instance_t *vf)
{
    vf->reconfig = reconfig;
    vf->filter = filter;
    vf->query_format = query_format;
    vf->uninit = uninit;
    vf->priv->sws = mp_sws_alloc(vf);
    vf->priv->sws->log = vf->log;
    return 1;
}

const vf_info_t vf_info_convert = {
    .description = "image format conversion with libswscale",
    .name = "convert",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
};
