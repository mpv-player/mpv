/*
 * Ambient light sensor implementation that delegates to VOCTRL_GET_AMBIENT_LUX
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

// TODO: refactor macOS implementation to not use voctrl and replace this file
// with one for macOS.

#include "osdep/als.h"
#include "player/core.h"
#include "ta/ta_talloc.h"
#include "video/out/vo.h"

struct mp_als {
    MPContext *mpctx;
};

struct mp_als *mp_als_create(void *parent, MPContext *mpctx) {
    struct mp_als *als = talloc(parent, struct mp_als);
    *als = (struct mp_als){ .mpctx = mpctx };
    return als;
}

enum mp_als_status mp_als_get_lux(struct mp_als *state, double *lux) {
    struct vo *vo = state->mpctx->video_out;
    if (!vo || vo_control(vo, VOCTRL_GET_AMBIENT_LUX, lux) < 1)
        return MP_ALS_STATUS_NODEVICE;
    return MP_ALS_STATUS_OK;
}
