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

#include <stdlib.h>
#include <stdbool.h>

// both int64_t and double should be able to represent this exactly
#define MP_NOPTS_VALUE (-1LL<<63)

#define ROUND(x) ((int)((x) < 0 ? (x) - 0.5 : (x) + 0.5))

#define MP_EXPAND_ARGS(...) __VA_ARGS__

#define MP_TALLOC_ELEMS(p) (talloc_get_size(p) / sizeof((p)[0]))
#define MP_GROW_ARRAY(p, nextidx) do {          \
    if ((nextidx) == MP_TALLOC_ELEMS(p))        \
        p = talloc_realloc_size(NULL, p, talloc_get_size(p) * 2); } while (0)
#define MP_RESIZE_ARRAY(ctx, p, count) do {     \
        p = talloc_realloc_size((ctx), p, (count) * sizeof(p[0])); } while (0)


#define MP_TARRAY_GROW(ctx, p, nextidx)             \
    do {                                            \
        size_t nextidx_ = (nextidx);                \
        size_t nelems_ = MP_TALLOC_ELEMS(p);        \
        if (nextidx_ <= nelems_)                    \
            p = talloc_realloc_size((ctx), p,       \
               (nextidx_ + 1) * sizeof((p)[0]) * 2);\
    } while (0)

#define MP_TARRAY_APPEND(ctx, p, idxvar, ...)       \
    do {                                            \
        MP_TARRAY_GROW(ctx, p, idxvar);             \
        p[idxvar] = (MP_EXPAND_ARGS(__VA_ARGS__));  \
        idxvar++;                                   \
    } while (0)

#define talloc_struct(ctx, type, ...) \
    talloc_memdup(ctx, &(type) MP_EXPAND_ARGS(__VA_ARGS__), sizeof(type))

#ifdef __GNUC__

/** Use gcc attribute to check printf fns.  a1 is the 1-based index of
 * the parameter containing the format, and a2 the index of the first
 * argument. **/
#ifdef __MINGW32__
// MinGW maps "printf" to the non-standard MSVCRT functions, even if
// __USE_MINGW_ANSI_STDIO is defined and set to 1. We need to use "gnu_printf",
// which isn't necessarily available on other GCC compatible compilers.
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (gnu_printf, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (printf, a1, a2)))
#endif

#else

#define PRINTF_ATTRIBUTE(a1, a2)

#endif

extern const char *mplayer_version;

char *mp_format_time(double time, bool fractions);

#endif /* MPLAYER_MPCOMMON_H */
