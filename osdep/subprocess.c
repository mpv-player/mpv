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

#include <pthread.h>

#include "config.h"

#include "common/common.h"
#include "common/msg.h"
#include "common/msg_control.h"

#include "subprocess.h"

void mp_devnull(void *ctx, char *data, size_t size)
{
}

const char *mp_subprocess_err_str(int num)
{
    // Note: these are visible to the public client API
    switch (num) {
    case MP_SUBPROCESS_OK:              return "success";
    case MP_SUBPROCESS_EKILLED_BY_US:   return "killed";
    case MP_SUBPROCESS_EINIT:           return "init";
    case MP_SUBPROCESS_EUNSUPPORTED:    return "unsupported";
    case MP_SUBPROCESS_EGENERIC:        // fall through
    default:                            return "unknown";
    }
}
