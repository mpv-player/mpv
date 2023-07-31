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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "libmpv_test_utils.h"

MP_NORETURN PRINTF_ATTRIBUTE(2, 3)
void fail(mpv_handle *ctx, const char *fmt, ...)
{
	if (fmt) {
		va_list va;
		va_start(va, fmt);
		vfprintf(stderr, fmt, va);
		va_end(va);
	}
    if (ctx)
	    mpv_destroy(ctx);
    exit(1);
}

void check_api_error(int status)
{
    if (status < 0)
        fail(NULL, "mpv API error: %s\n", mpv_error_string(status));
}

