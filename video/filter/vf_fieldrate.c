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

#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "refqueue.h"

struct opts {
    int field_parity;
    bool interlaced_only;
};

struct priv {
    struct opts *opts;
    struct mp_refqueue *queue;
};

static void vf_field_process(struct mp_filter *f)
{
    struct priv *p = f->priv;
    mp_refqueue_execute_reinit(p->queue);

    if (!mp_refqueue_can_output(p->queue))
        return;

    struct mp_image *in = mp_refqueue_get(p->queue, 0);
    struct mp_image *out = mp_image_new_ref(in);
    if (!out) {
        mp_refqueue_write_out_pin(p->queue, NULL);
        return;
    }
    // This filter does not deinterlace. It only emits one output per field so
    // the VO gets called at fieldrate cadence.
    out->fields &= ~(MP_IMGFIELD_TICK_FIRST | MP_IMGFIELD_TICK_SECOND);
    if (mp_refqueue_should_deint(p->queue)) {
        out->fields |= mp_refqueue_is_second_field(p->queue) ?
            MP_IMGFIELD_TICK_SECOND : MP_IMGFIELD_TICK_FIRST;
        out->fields |= MP_IMGFIELD_INTERLACED;

        if (mp_refqueue_top_field_first(p->queue)) {
            out->fields |= MP_IMGFIELD_TOP_FIRST;
        } else {
            out->fields &= ~MP_IMGFIELD_TOP_FIRST;
        }
    }
    mp_refqueue_write_out_pin(p->queue, out);
}

static void vf_field_reset(struct mp_filter *f)
{
    struct priv *p = f->priv;
    mp_refqueue_flush(p->queue);
}

static void vf_field_destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;
    mp_refqueue_flush(p->queue);
    talloc_free(p->queue);
}

static const struct mp_filter_info vf_field_filter = {
    .name = "fieldrate",
    .process = vf_field_process,
    .reset = vf_field_reset,
    .destroy = vf_field_destroy,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_field_create(struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &vf_field_filter);
    if (!f)
        return NULL;
    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    p->queue = mp_refqueue_alloc(f);

    mp_refqueue_set_refs(p->queue, 0, 0);
    mp_refqueue_set_mode(p->queue,
        MP_MODE_DEINT |
        MP_MODE_OUTPUT_FIELDS |
        (p->opts->interlaced_only ? MP_MODE_INTERLACED_ONLY : 0));
    mp_refqueue_set_parity(p->queue, p->opts->field_parity);

    return f;
}
#define OPT_BASE_STRUCT struct opts
static const m_option_t vf_opts_fields[] = {
    {"interlaced-only", OPT_BOOL(interlaced_only)},
    {"parity", OPT_CHOICE(field_parity,
        {"tff", MP_FIELD_PARITY_TFF},
        {"bff", MP_FIELD_PARITY_BFF},
        {"auto", MP_FIELD_PARITY_AUTO})},
    {0}
};

const struct mp_user_filter_entry vf_fieldrate = {
    .desc = {
        .description = "Emit one frame per field",
        .name = "fieldrate",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .options = vf_opts_fields,
    },
    .create = vf_field_create,
};
