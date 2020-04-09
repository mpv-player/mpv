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

#include "audio/aframe.h"
#include "audio/format.h"
#include "filters/f_autoconvert.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_option.h"

struct f_opts {
    int in_format;
    int in_srate;
    struct m_channels in_channels;
    int out_format;
    int out_srate;
    struct m_channels out_channels;

    int fail;
};

struct priv {
    struct f_opts *opts;
    struct mp_pin *in_pin;
};

static void process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!mp_pin_can_transfer_data(f->ppins[1], p->in_pin))
        return;

    struct mp_frame frame = mp_pin_out_read(p->in_pin);

    if (p->opts->fail) {
        MP_ERR(f, "Failing on purpose.\n");
        goto error;
    }

    if (frame.type == MP_FRAME_EOF) {
        mp_pin_in_write(f->ppins[1], frame);
        return;
    }

    if (frame.type != MP_FRAME_AUDIO) {
        MP_ERR(f, "audio frame expected\n");
        goto error;
    }

    struct mp_aframe *in = frame.data;

    if (p->opts->out_channels.num_chmaps > 0) {
        if (!mp_aframe_set_chmap(in, &p->opts->out_channels.chmaps[0])) {
            MP_ERR(f, "could not force output channels\n");
            goto error;
        }
    }

    if (p->opts->out_srate)
        mp_aframe_set_rate(in, p->opts->out_srate);

    mp_pin_in_write(f->ppins[1], frame);
    return;

error:
    mp_frame_unref(&frame);
    mp_filter_internal_mark_failed(f);
}

static const struct mp_filter_info af_format_filter = {
    .name = "format",
    .priv_size = sizeof(struct priv),
    .process = process,
};

static struct mp_filter *af_format_create(struct mp_filter *parent,
                                              void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &af_format_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct mp_autoconvert *conv = mp_autoconvert_create(f);
    if (!conv)
        abort();

    if (p->opts->in_format)
        mp_autoconvert_add_afmt(conv, p->opts->in_format);
    if (p->opts->in_srate)
        mp_autoconvert_add_srate(conv, p->opts->in_srate);
    if (p->opts->in_channels.num_chmaps > 0)
        mp_autoconvert_add_chmap(conv, &p->opts->in_channels.chmaps[0]);

    mp_pin_connect(conv->f->pins[0], f->ppins[0]);
    p->in_pin = conv->f->pins[1];

    return f;
}

#define OPT_BASE_STRUCT struct f_opts

const struct mp_user_filter_entry af_format = {
    .desc = {
        .name = "format",
        .description = "Force audio format",
        .priv_size = sizeof(struct f_opts),
        .options = (const struct m_option[]) {
            {"format", OPT_AUDIOFORMAT(in_format)},
            {"srate", OPT_INT(in_srate), M_RANGE(1000, 8*48000)},
            {"channels", OPT_CHANNELS(in_channels),
                .flags = M_OPT_CHANNELS_LIMITED},
            {"out-srate", OPT_INT(out_srate), M_RANGE(1000, 8*48000)},
            {"out-channels", OPT_CHANNELS(out_channels),
                .flags = M_OPT_CHANNELS_LIMITED},
            {"fail", OPT_FLAG(fail)},
            {0}
        },
    },
    .create = af_format_create,
};
