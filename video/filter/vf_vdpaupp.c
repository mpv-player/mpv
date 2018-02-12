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

#include <libavutil/hwcontext.h>

#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/hwdec.h"
#include "video/vdpau.h"
#include "video/vdpau_mixer.h"
#include "refqueue.h"

// Note: this filter does no actual filtering; it merely sets appropriate
//       flags on vdpau images (mp_vdpau_mixer_frame) to do the appropriate
//       processing on the final rendering process in the VO.

struct opts {
    int deint_enabled;
    int interlaced_only;
    struct mp_vdpau_mixer_opts opts;
};

struct priv {
    struct opts *opts;
    struct mp_vdpau_ctx *ctx;
    struct mp_refqueue *queue;
    struct mp_pin *in_pin;
};

static VdpVideoSurface ref_field(struct priv *p,
                                 struct mp_vdpau_mixer_frame *frame, int pos)
{
    struct mp_image *mpi = mp_image_new_ref(mp_refqueue_get_field(p->queue, pos));
    if (!mpi)
        return VDP_INVALID_HANDLE;
    talloc_steal(frame, mpi);
    return (uintptr_t)mpi->planes[3];
}

static void vf_vdpaupp_process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    mp_refqueue_execute_reinit(p->queue);

    if (!mp_refqueue_can_output(p->queue))
        return;

    struct mp_image *mpi =
        mp_vdpau_mixed_frame_create(mp_refqueue_get_field(p->queue, 0));
    if (!mpi)
        return; // OOM
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

    frame->opts = p->opts->opts;

    mpi->planes[3] = (void *)(uintptr_t)frame->current;

    mpi->params.hw_subfmt = 0; // force mixer

    mp_refqueue_write_out_pin(p->queue, mpi);
}

static void vf_vdpaupp_reset(struct mp_filter *f)
{
    struct priv *p = f->priv;
    mp_refqueue_flush(p->queue);
}

static void vf_vdpaupp_destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;
    talloc_free(p->queue);
}

static const struct mp_filter_info vf_vdpaupp_filter = {
    .name = "vdpaupp",
    .process = vf_vdpaupp_process,
    .reset = vf_vdpaupp_reset,
    .destroy = vf_vdpaupp_destroy,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_vdpaupp_create(struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &vf_vdpaupp_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);

    p->queue = mp_refqueue_alloc(f);

    AVBufferRef *ref = mp_filter_load_hwdec_device(f, AV_HWDEVICE_TYPE_VDPAU);
    if (!ref)
        goto error;
    p->ctx = mp_vdpau_get_ctx_from_av(ref);
    av_buffer_unref(&ref);
    if (!p->ctx)
        goto error;

    if (!p->opts->deint_enabled)
        p->opts->opts.deint = 0;

    if (p->opts->opts.deint >= 2) {
        mp_refqueue_set_refs(p->queue, 1, 1); // 2 past fields, 1 future field
    } else {
        mp_refqueue_set_refs(p->queue, 0, 0);
    }
    mp_refqueue_set_mode(p->queue,
        (p->opts->deint_enabled ? MP_MODE_DEINT : 0) |
        (p->opts->interlaced_only ? MP_MODE_INTERLACED_ONLY : 0) |
        (p->opts->opts.deint >= 2 ? MP_MODE_OUTPUT_FIELDS : 0));

    mp_refqueue_add_in_format(p->queue, IMGFMT_VDPAU, 0);

    return f;

error:
    talloc_free(f);
    return NULL;
}

#define OPT_BASE_STRUCT struct opts
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
    OPT_FLAG("interlaced-only", interlaced_only, 0),
    {0}
};

const struct mp_user_filter_entry vf_vdpaupp = {
    .desc = {
        .description = "vdpau postprocessing",
        .name = "vdpaupp",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .options = vf_opts_fields,
    },
    .create = vf_vdpaupp_create,
};
