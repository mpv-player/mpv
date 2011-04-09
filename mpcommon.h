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

#ifndef MPLAYER_MPCOMMON_H
#define MPLAYER_MPCOMMON_H

// both int64_t and double should be able to represent this exactly
#define MP_NOPTS_VALUE (-1LL<<63)

#define ROUND(x) ((int)((x) < 0 ? (x) - 0.5 : (x) + 0.5))

#define MP_TALLOC_ELEMS(p) (talloc_get_size(p) / sizeof((p)[0]))
#define MP_GROW_ARRAY(p, nextidx) do {          \
    if ((nextidx) == MP_TALLOC_ELEMS(p))        \
        p = talloc_realloc_size(NULL, p, talloc_get_size(p) * 2); } while (0)
#define MP_RESIZE_ARRAY(ctx, p, count) do {     \
        p = talloc_realloc_size((ctx), p, (count) * sizeof(p[0])); } while (0)

extern const char *mplayer_version;

#endif /* MPLAYER_MPCOMMON_H */
