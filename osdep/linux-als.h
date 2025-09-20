/*
 * Linux ambient light sensor utilities
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

#include "common/global.h"

struct mp_linux_als;

/**
 * Caller must call `mp_linux_als_state_destroy`
 * @param parent talloc parent
 * @return a newly allocated mp_linux_als_state
 */
struct mp_linux_als *mp_linux_als_create(void *parent, struct mpv_global *global);

/**
 * Get the ambient brightness by reading from sysfs.
 * @param lux output value
 * @return 0 on success, negative number on failure
 */
int mp_linux_als_get_lux(struct mp_linux_als *state, double *lux);
