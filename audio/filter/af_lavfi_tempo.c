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

#include "common/common.h"
#include "common/msg.h"

#include "audio/aframe.h"
#include "filters/f_lavfi.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/frame.h"
#include "filters/user_filters.h"
#include "options/m_option.h"
#include "ta/ta_talloc.h"

struct f_opts {
    char *filter;
};

struct priv {
    struct f_opts *opts;
    bool initialized;
    bool fall_backed;
    bool lavfi_ready;
    double speed;
    double set_speed;
    double in_pts;
    struct mp_filter *lavfi_filter;
    const struct mp_filter_info *lavfi_filter_info;
};

static bool set_speed(struct mp_filter *f) {
    struct priv *p = f->priv;

    char *arg = talloc_asprintf(NULL, "%f", p->speed);
    struct mp_filter_command cmd = {
        .type = MP_FILTER_COMMAND_TEXT,
        .target = p->opts->filter,
        .cmd = "tempo",
        .arg = arg,
    };
    bool ret = p->lavfi_filter_info->command(p->lavfi_filter, &cmd);
    talloc_free(arg);
    if (!ret) {
        MP_FATAL(f, "failed to set %s=%s for lavfi filter %s\n", cmd.cmd, cmd.arg, p->opts->filter);
        return false;
    } else {
        p->set_speed = p->speed;
        return ret;
    }
}

static bool init_lavfi_tempo(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (strcmp(p->opts->filter, "ascale") && strcmp(p->opts->filter, "atempo")) {
        MP_FATAL(f, "'%s' is not recognized in this context, use 'ascale' or 'atempo'.\n",
                p->opts->filter);
        return false;
    }

    mp_assert(!p->lavfi_filter);

    if (!mp_lavfi_is_usable(p->opts->filter, AVMEDIA_TYPE_AUDIO)) {
        MP_WARN(f, "%s filter is not available, using atempo instead.\n", p->opts->filter);
        p->opts->filter = "atempo";
        p->fall_backed = true;
    }

    p->lavfi_filter = mp_create_user_filter(f, MP_OUTPUT_CHAIN_AUDIO, p->opts->filter, NULL);
    if (!p->lavfi_filter) {
        MP_FATAL(f, "failed to create lavfi %s filter.\n", p->opts->filter);
        return false;
    }

    p->lavfi_filter_info = mp_filter_get_info(p->lavfi_filter);
    p->lavfi_ready = false;
    p->initialized = true;

    return true;
}

static void af_lavfi_tempo_reset(struct mp_filter *f)
{
    struct priv *p = f->priv;
    p->in_pts = MP_NOPTS_VALUE;
    p->lavfi_ready = false;
    p->set_speed = 1.0;
    p->lavfi_filter_info->reset(p->lavfi_filter);

}

static void af_lavfi_tempo_process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    if (!p->initialized && !init_lavfi_tempo(f))
        return;

    if (p->lavfi_ready && p->set_speed != p->speed) {
        set_speed(f);
    }

    if (mp_pin_can_transfer_data(p->lavfi_filter->pins[0], f->ppins[0])) {
        struct mp_frame frame = mp_pin_out_read(f->ppins[0]);
        if (frame.type == MP_FRAME_AUDIO) {
            struct mp_aframe *aframe = frame.data;
            p->in_pts = mp_aframe_get_pts(aframe);
        }
        if (!mp_pin_in_write(p->lavfi_filter->pins[0], frame)) {
            MP_FATAL(f, "failed to move frame to internal lavfi filter\n");
        } else {
            p->lavfi_ready = true;
        }
    }

    if (mp_pin_can_transfer_data(f->ppins[1], p->lavfi_filter->pins[1])) {
        struct mp_frame frame = mp_pin_out_read(p->lavfi_filter->pins[1]);
        if (frame.type == MP_FRAME_AUDIO) {
            struct mp_aframe *aframe = frame.data;
            mp_aframe_set_speed(aframe, p->set_speed);
            if (p->in_pts != MP_NOPTS_VALUE) {
                mp_aframe_set_pts(aframe, p->in_pts);
            }
        }
        if (!mp_pin_in_write(f->ppins[1], frame)) {
            MP_FATAL(f, "failed to move frame to internal lavfi filter\n");
        } else {
            p->lavfi_ready = true;
        }
    }

    return;
}

static bool af_lavfi_tempo_command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct priv *p = f->priv;

    switch (cmd->type) {
        case MP_FILTER_COMMAND_SET_SPEED:
            if (cmd->speed == p->speed) {
                return true;
            }

            p->speed = cmd->speed;
            return p->lavfi_ready ? set_speed(f) : true;
    }
    return false;
}

static void af_lavfi_tempo_destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;
    if (p->fall_backed) {
        // prevent ta_alloc abort since filter name is const
       p->opts->filter = NULL;
    }
    if (p->lavfi_filter_info) {
        p->lavfi_filter_info->destroy(p->lavfi_filter);
    }
}

static const struct mp_filter_info af_lavfi_tempo_filter = {
    .name = "lavfi_tempo",
    .priv_size = sizeof(struct priv),
    .process = af_lavfi_tempo_process,
    .command = af_lavfi_tempo_command,
    .reset = af_lavfi_tempo_reset,
    .destroy = af_lavfi_tempo_destroy,
};

static struct mp_filter *af_lavfi_tempo_create(struct mp_filter *parent,
                                              void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &af_lavfi_tempo_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");


    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);
    p->speed = 1.0;
    p->set_speed = 1.0;
    p->in_pts = MP_NOPTS_VALUE;

    if (!init_lavfi_tempo(f)) {
        return NULL;
    }

    return f;
}

#define OPT_BASE_STRUCT struct f_opts

const struct mp_user_filter_entry af_lavfi_tempo = {
    .desc = {
        .description = "Tempo change with lavfi ascale or atempo filters",
        .name = "lavfi-tempo",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .filter = "atempo",
        },
        .options = (const struct m_option[]) {
            {"filter", OPT_STRING(filter)},
            {0}
        },
    },
    .create = af_lavfi_tempo_create,
};
