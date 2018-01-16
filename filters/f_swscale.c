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
#include "video/mp_image_pool.h"
#include "video/sws_utils.h"
#include "video/fmt-conversion.h"

#include "f_swscale.h"
#include "filter.h"
#include "filter_internal.h"

int mp_sws_find_best_out_format(int in_format, int *out_formats,
                                int num_out_formats)
{
    if (sws_isSupportedInput(imgfmt2pixfmt(in_format)) < 1)
        return 0;

    int best = 0;
    for (int n = 0; n < num_out_formats; n++) {
        int out_format = out_formats[n];

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

bool mp_sws_supports_input(int imgfmt)
{
    return sws_isSupportedInput(imgfmt2pixfmt(imgfmt));
}

static void process(struct mp_filter *f)
{
    struct mp_sws_filter *s = f->priv;

    if (!mp_pin_can_transfer_data(f->ppins[1], f->ppins[0]))
        return;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);
    if (mp_frame_is_signaling(frame)) {
        mp_pin_in_write(f->ppins[1], frame);
        return;
    }

    if (frame.type != MP_FRAME_VIDEO) {
        MP_ERR(f, "video frame expected\n");
        goto error;
    }

    struct mp_image *src = frame.data;
    int dstfmt = s->out_format ? s->out_format : src->imgfmt;

    struct mp_image *dst = mp_image_pool_get(s->pool, dstfmt, src->w, src->h);
    if (!dst)
        goto error;

    mp_image_copy_attributes(dst, src);

    // If we convert from RGB to YUV, default to limited range.
    if (mp_imgfmt_get_forced_csp(src->imgfmt) == MP_CSP_RGB &&
        mp_imgfmt_get_forced_csp(dst->imgfmt) == MP_CSP_AUTO)
    {
        dst->params.color.levels = MP_CSP_LEVELS_TV;
    }
    mp_image_params_guess_csp(&dst->params);

    bool ok = mp_sws_scale(s->sws, dst, src) >= 0;

    mp_frame_unref(&frame);
    frame = (struct mp_frame){MP_FRAME_VIDEO, dst};

    if (!ok)
        goto error;

    mp_pin_in_write(f->ppins[1], frame);
    return;

error:
    mp_frame_unref(&frame);
    mp_filter_internal_mark_failed(f);
    return;
}

static const struct mp_filter_info sws_filter = {
    .name = "swscale",
    .priv_size = sizeof(struct mp_sws_filter),
    .process = process,
};

struct mp_sws_filter *mp_sws_filter_create(struct mp_filter *parent)
{
    struct mp_filter *f = mp_filter_create(parent, &sws_filter);
    if (!f)
        return NULL;

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct mp_sws_filter *s = f->priv;
    s->f = f;
    s->sws = mp_sws_alloc(s);
    s->sws->log = f->log;
    s->pool = mp_image_pool_new(s);

    mp_sws_set_from_cmdline(s->sws, f->global);

    return s;
}
