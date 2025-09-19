/*
 * Generic interface for ambient light sensors (ALS)
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


#pragma once

typedef struct MPContext MPContext;

struct mp_als;

enum mp_als_status {
    MP_ALS_STATUS_OK = 0,

    // The system has no ALS
    MP_ALS_STATUS_NODEVICE,

    // The system has an ALS, but we couldn't read it.
    MP_ALS_STATUS_READFAILED,
};

/**
 * @param parent talloc parent
 * @return a newly allocated mp_linux_als_state
 */
// TODO: replace the mpctx parameter with mpv_global when we no longer need the
// vo for the als-voctrl implementation.
struct mp_als *mp_als_create(void *parent, MPContext *mpctx);

/**
 * Get the ambient brightness.
 * @param lux output value
 */
enum mp_als_status mp_als_get_lux(struct mp_als *state, double *lux);
