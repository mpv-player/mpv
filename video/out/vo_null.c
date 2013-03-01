/*
 * based on video_out_null.c from mpeg2dec
 *
 * Copyright (C) Aaron Holtzman - June 2000
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "config.h"
#include "core/mp_msg.h"
#include "vo.h"
#include "video/vfcap.h"
#include "video/mp_image.h"

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
}

static void flip_page(struct vo *vo)
{
}

static int query_format(struct vo *vo, uint32_t format)
{
    if (IMGFMT_IS_HWACCEL(format))
        return 0;
    return VFCAP_CSP_SUPPORTED;
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    return 0;
}

static void uninit(struct vo *vo)
{
}

static void check_events(struct vo *vo)
{
}

static int preinit(struct vo *vo, const char *arg)
{
    if (arg) {
        mp_tmsg(MSGT_VO, MSGL_WARN, "[VO_NULL] Unknown subdevice: %s.\n", arg);
        return ENOSYS;
    }
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

const struct vo_driver video_out_null = {
    .info = &(const vo_info_t) {
        "Null video output",
        "null",
        "Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
        ""
    },
    .preinit = preinit,
    .query_format = query_format,
    .config = config,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
};
