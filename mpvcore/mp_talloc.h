/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPV_TALLOC_H
#define MPV_TALLOC_H

#include "talloc.h"
#include "compat/compiler.h"

#define MP_TALLOC_ELEMS(p) (talloc_get_size(p) / sizeof((p)[0]))

#define MP_RESIZE_ARRAY(ctx, p, count) do {         \
        (p) = talloc_realloc_size((ctx), p, (count) * sizeof((p)[0])); } while (0)

#define MP_TARRAY_GROW(ctx, p, nextidx)             \
    do {                                            \
        size_t nextidx_ = (nextidx);                \
        if (nextidx_ >= MP_TALLOC_ELEMS(p))         \
            MP_RESIZE_ARRAY(ctx, p, (nextidx_ + 1) * 2);\
    } while (0)

#define MP_GROW_ARRAY(p, nextidx) MP_TARRAY_GROW(NULL, p, nextidx)

#define MP_TARRAY_APPEND(ctx, p, idxvar, ...)       \
    do {                                            \
        MP_TARRAY_GROW(ctx, p, idxvar);             \
        (p)[(idxvar)] = (MP_EXPAND_ARGS(__VA_ARGS__));\
        (idxvar)++;                                 \
    } while (0)

// Doesn't actually free any memory, or do any other talloc calls.
#define MP_TARRAY_REMOVE_AT(p, idxvar, at)          \
    do {                                            \
        size_t at_ = (at);                          \
        assert(at_ <= (idxvar));                    \
        memmove((p) + at_, (p) + at_ + 1,           \
                ((idxvar) - at_ - 1) * sizeof((p)[0])); \
        (idxvar)--;                                 \
    } while (0)

#define talloc_struct(ctx, type, ...) \
    talloc_memdup(ctx, &(type) MP_EXPAND_ARGS(__VA_ARGS__), sizeof(type))

#endif
