/*
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

#include "talloc.h"
#include "mpcommon.h"

char *mp_format_time(double time, bool fractions)
{
    if (time < 0)
        return talloc_strdup(NULL, "unknown");
    int h, m, s = time;
    h = s / 3600;
    s -= h * 3600;
    m = s / 60;
    s -= m * 60;
    char *res = talloc_asprintf(NULL, "%02d:%02d:%02d", h, m, s);
    if (fractions)
        res = talloc_asprintf_append(res, ".%03d",
                                     (int)((time - (int)time) * 1000));
    return res;
}
