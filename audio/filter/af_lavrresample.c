/*
 * Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
 *
 * Based on Michael Niedermayer's lavcresample.
 *
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
#include <math.h>
#include <assert.h>

#include "common/common.h"
#include "config.h"

#include "common/av_common.h"
#include "common/msg.h"
#include "filters/f_swresample.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/options.h"

struct af_resample {
    int allow_detach;
    struct mp_resample_opts opts;
    int global_normalize;
};

static void set_defaults(struct mpv_global *global, void *p)
{
    struct af_resample *s = p;

    struct mp_resample_opts *opts = &s->opts;

    struct mp_resample_opts *src_opts =
        mp_get_config_group(s, global, &resample_conf);

    s->global_normalize = src_opts->normalize;

    assert(!opts->avopts); // we don't set a default value, so it must be NULL

    *opts = *src_opts;

    opts->avopts = NULL;
    struct m_option dummy = {.type = &m_option_type_keyvalue_list};
    m_option_copy(&dummy, &opts->avopts, &src_opts->avopts);
}

#define OPT_BASE_STRUCT struct af_resample

static struct mp_filter *af_lavrresample_create(struct mp_filter *parent,
                                                void *options)
{
    struct af_resample *s = options;

    if (s->opts.normalize < 0)
        s->opts.normalize = s->global_normalize;

    struct mp_swresample *swr = mp_swresample_create(parent, &s->opts);
    if (!swr)
        abort();

    MP_WARN(swr->f, "This filter is deprecated! Use the --audio-resample- options"
            " to customize resampling, or the --af=aresample filter.\n");

    talloc_free(s);
    return swr->f;
}

const struct mp_user_filter_entry af_lavrresample = {
    .desc = {
        .description = "Sample frequency conversion using libavresample",
        .name = "lavrresample",
        .priv_size = sizeof(struct af_resample),
        .priv_defaults = &(const struct af_resample) {
            .opts = MP_RESAMPLE_OPTS_DEF,
            .allow_detach = 1,
        },
        .options = (const struct m_option[]) {
            OPT_INTRANGE("filter-size", opts.filter_size, 0, 0, 32),
            OPT_INTRANGE("phase-shift", opts.phase_shift, 0, 0, 30),
            OPT_FLAG("linear", opts.linear, 0),
            OPT_DOUBLE("cutoff", opts.cutoff, M_OPT_RANGE, .min = 0, .max = 1),
            OPT_FLAG("detach", allow_detach, 0), // does nothing
            OPT_CHOICE("normalize", opts.normalize, 0,
                    ({"no", 0}, {"yes", 1}, {"auto", -1})),
            OPT_KEYVALUELIST("o", opts.avopts, 0),
            {0}
        },
        .set_defaults = set_defaults,
    },
    .create = af_lavrresample_create,
};
