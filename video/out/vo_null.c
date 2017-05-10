/*
 * based on video_out_null.c from mpeg2dec
 *
 * Copyright (C) Aaron Holtzman - June 2000
 *
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

#include <stdlib.h>
#include "common/msg.h"
#include "vo.h"
#include "video/mp_image.h"
#include "osdep/timer.h"
#include "options/m_option.h"

struct priv {
    int64_t last_vsync;

    double cfg_fps;
};

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    talloc_free(mpi);
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->cfg_fps) {
        int64_t ft = 1e6 / p->cfg_fps;
        int64_t prev_vsync = mp_time_us() / ft;
        int64_t target_time = (prev_vsync + 1) * ft;
        for (;;) {
            int64_t now = mp_time_us();
            if (now >= target_time)
                break;
            mp_sleep_us(target_time - now);
        }
    }
}

static int query_format(struct vo *vo, int format)
{
    return 1;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    return 0;
}

static void uninit(struct vo *vo)
{
}

static int preinit(struct vo *vo)
{
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
    case VOCTRL_GET_DISPLAY_FPS:
        if (!p->cfg_fps)
            break;
        *(double *)data = p->cfg_fps;
        return VO_TRUE;
    }
    return VO_NOTIMPL;
}

#define OPT_BASE_STRUCT struct priv
const struct vo_driver video_out_null = {
    .description = "Null video output",
    .name = "null",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_DOUBLE("fps", cfg_fps, M_OPT_RANGE, .min = 0, .max = 10000),
        {0},
    },
    .options_prefix = "vo-null",
};
