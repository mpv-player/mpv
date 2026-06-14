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

#include <libavutil/buffer.h>

#include "common/common.h"
#include "common/msg.h"
#include "demux/stheader.h"
#include "f_decoder_wrapper.h"
#include "f_enhancement_pair.h"
#include "filter_internal.h"
#include "video/mp_image.h"

// PTS-match tolerance, in seconds.
#define PTS_MATCH_TOLERANCE 1e-6

// Number of frames hold for matching.
#define QUEUE_MAX 16

struct priv {
    struct mp_decoder_wrapper *el_dec;
    struct mp_pin *el_in; // el_dec->f->pins[0]

    // BL/EL frames decoded but not yet emitted as a pair.
    struct mp_image **bl_pending;
    int num_bl_pending;
    struct mp_image **el_pending;
    int num_el_pending;

    bool bl_eof;
    bool el_eof;
};

static int pts_cmp(double a, double b)
{
    if (a == MP_NOPTS_VALUE || b == MP_NOPTS_VALUE)
        return 0;
    if (a < b - PTS_MATCH_TOLERANCE) return -1;
    if (a > b + PTS_MATCH_TOLERANCE) return  1;
    return 0;
}

// Pull available frames from `pin` into `queue` until either no data is
// ready or the queue is full. Sets *eof if the upstream signaled EOF.
static void drain_pin(struct mp_filter *f, struct mp_pin *pin,
                      struct mp_image ***queue, int *num, bool *eof)
{
    while (!*eof && *num < QUEUE_MAX) {
        if (!mp_pin_out_request_data(pin))
            return;
        struct mp_frame fr = mp_pin_out_read(pin);
        if (fr.type == MP_FRAME_EOF) {
            *eof = true;
            return;
        }
        if (fr.type != MP_FRAME_VIDEO) {
            mp_frame_unref(&fr);
            continue;
        }
        MP_TARRAY_APPEND(f->priv, *queue, *num, fr.data);
    }
}

static void pop_head(struct mp_image ***queue, int *num)
{
    talloc_free((*queue)[0]);
    int remain = *num - 1;
    if (remain > 0)
        memmove(&(*queue)[0], &(*queue)[1], remain * sizeof((*queue)[0]));
    *num = remain;
}

static struct mp_image *take_head(struct mp_image ***queue, int *num)
{
    struct mp_image *img = (*queue)[0];
    int remain = *num - 1;
    if (remain > 0)
        memmove(&(*queue)[0], &(*queue)[1], remain * sizeof((*queue)[0]));
    *num = remain;
    return img;
}

// Downstream code expects the DV RPU and the related color/repr/HDR fields on
// the BL frame. In dual-track sources these are carried on the EL stream, so
// mirror them onto the BL here. This keeps DV bookkeeping local.
static void inherit_dovi_from_el(struct mp_image *bl, struct mp_image *el)
{
    if (bl->params.no_dovi || bl->dovi || !el->dovi)
        return;
    bl->dovi = av_buffer_ref(el->dovi);
    if (!bl->dovi)
        return;
    bl->params.repr.dovi = (void *)bl->dovi->data;
    bl->params.repr.sys = el->params.repr.sys;
    bl->params.color.primaries = el->params.color.primaries;
    bl->params.color.transfer = el->params.color.transfer;
    bl->params.color.hdr.min_luma = el->params.color.hdr.min_luma;
    bl->params.color.hdr.max_luma = el->params.color.hdr.max_luma;
    bl->params.color.hdr.max_pq_y = el->params.color.hdr.max_pq_y;
    bl->params.color.hdr.avg_pq_y = el->params.color.hdr.avg_pq_y;
}

static void pair_process(struct mp_filter *f)
{
    struct priv *p = f->priv;
    struct mp_pin *in = f->ppins[0];
    struct mp_pin *out = f->ppins[1];

    drain_pin(f, in, &p->bl_pending, &p->num_bl_pending, &p->bl_eof);
    drain_pin(f, p->el_in, &p->el_pending, &p->num_el_pending, &p->el_eof);

    while (mp_pin_in_needs_data(out)) {
        if (p->num_bl_pending == 0) {
            if (p->bl_eof) {
                while (p->num_el_pending)
                    pop_head(&p->el_pending, &p->num_el_pending);
                mp_pin_in_write(out, MP_EOF_FRAME);
            }
            return;
        }

        struct mp_image *bl = p->bl_pending[0];
        int cmp = p->num_el_pending > 0
                ? pts_cmp(p->el_pending[0]->pts, bl->pts) : 0;

        // EL older than BL: its BL partner already left or never arrived.
        if (p->num_el_pending > 0 && cmp < 0) {
            pop_head(&p->el_pending, &p->num_el_pending);
            continue;
        }

        if (p->num_el_pending > 0 && cmp == 0) {
            struct mp_image *el = take_head(&p->el_pending, &p->num_el_pending);
            take_head(&p->bl_pending, &p->num_bl_pending);
            inherit_dovi_from_el(bl, el);
            if (bl->params.no_enhancement_layer) {
                talloc_free(el);
            } else {
                bl->enhancement_layer = el;
            }
            mp_pin_in_write(out, MAKE_FRAME(MP_FRAME_VIDEO, bl));
            continue;
        }

        // No EL match for the oldest BL. Hold BL unless we have affirmative
        // evidence no EL is coming.
        bool give_up = p->el_eof ||
                       (p->num_el_pending > 0 && cmp > 0) ||
                       p->num_bl_pending >= QUEUE_MAX;
        if (!give_up)
            return;

        take_head(&p->bl_pending, &p->num_bl_pending);
        bl->enhancement_layer = NULL;
        mp_pin_in_write(out, MAKE_FRAME(MP_FRAME_VIDEO, bl));
    }
}

static void pair_reset(struct mp_filter *f)
{
    struct priv *p = f->priv;
    while (p->num_bl_pending)
        pop_head(&p->bl_pending, &p->num_bl_pending);
    while (p->num_el_pending)
        pop_head(&p->el_pending, &p->num_el_pending);
    p->bl_eof = false;
    p->el_eof = false;
}

static void pair_destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;
    while (p->num_bl_pending)
        pop_head(&p->bl_pending, &p->num_bl_pending);
    while (p->num_el_pending)
        pop_head(&p->el_pending, &p->num_el_pending);
    // el_dec->f is a child filter, freed by the framework after this returns.
}

static const struct mp_filter_info pair_filter = {
    .name = "enhancement_pair",
    .priv_size = sizeof(struct priv),
    .process = pair_process,
    .reset = pair_reset,
    .destroy = pair_destroy,
};

struct mp_filter *mp_enhancement_pair_create(struct mp_filter *parent,
                                             struct sh_stream *el_sh)
{
    if (!el_sh)
        return NULL;

    struct mp_filter *f = mp_filter_create(parent, &pair_filter);
    if (!f)
        return NULL;
    mp_filter_add_pin(f, MP_PIN_IN,  "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->el_dec = mp_decoder_wrapper_create(f, el_sh);
    if (!p->el_dec || !mp_decoder_wrapper_reinit(p->el_dec)) {
        MP_WARN(f, "Failed to set up enhancement-layer decoder; "
                "rendering base layer only.\n");
        talloc_free(f);
        return NULL;
    }
    p->el_in = p->el_dec->f->pins[0];
    mp_pin_set_manual_connection_for(p->el_in, f);

    return f;
}
