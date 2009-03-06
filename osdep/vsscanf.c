/*
 * vsscanf implementation for systems that do not have it in libc
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

#include "config.h"

#include <stdio.h>
#include <stdarg.h>

int
vsscanf(const char *str, const char *format, va_list ap)
{
    /* XXX: can this be implemented in a more portable way? */
    long p1 = va_arg(ap, long);
    long p2 = va_arg(ap, long);
    long p3 = va_arg(ap, long);
    long p4 = va_arg(ap, long);
    long p5 = va_arg(ap, long);
    return sscanf(str, format, p1, p2, p3, p4, p5);
}
