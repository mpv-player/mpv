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

#include <libavutil/hwcontext.h>

#include "hwdec.h"

static struct AVBufferRef *v4l2request_create_standalone(struct mpv_global *global,
        struct mp_log *log, struct hwcontext_create_dev_params *params)
{
    AVBufferRef* ref = NULL;
    av_hwdevice_ctx_create(&ref, AV_HWDEVICE_TYPE_V4L2REQUEST, NULL, NULL, 0);

    return ref;
}

const struct hwcontext_fns hwcontext_fns_v4l2request = {
    .av_hwdevice_type = AV_HWDEVICE_TYPE_V4L2REQUEST,
    .create_dev = v4l2request_create_standalone,
};
