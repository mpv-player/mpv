/*
 * This file is part of mpv.
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
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
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MP_ATOMICS_H
#define MP_ATOMICS_H

#include <inttypes.h>
#include "config.h"

#define HAVE_ATOMICS 1

#if HAVE_STDATOMIC
#include <stdatomic.h>
#else

// Emulate the parts of C11 stdatomic.h needed by mpv.
// Still relies on gcc/clang atomic builtins.

typedef struct { volatile unsigned long v;  } atomic_ulong;
typedef struct { volatile int v;            } atomic_int;
typedef struct { volatile _Bool v;          } atomic_bool;
typedef struct { volatile long long v;      } atomic_llong;
typedef struct { volatile uint_least32_t v; } atomic_uint_least32_t;
typedef struct { volatile unsigned long long v; } atomic_ullong;

#define ATOMIC_VAR_INIT(x) \
    {.v = (x)}

#if HAVE_ATOMIC_BUILTINS

#define atomic_load(p) \
    __atomic_load_n(&(p)->v, __ATOMIC_SEQ_CST)
#define atomic_store(p, val) \
    __atomic_store_n(&(p)->v, val, __ATOMIC_SEQ_CST)
#define atomic_fetch_add(a, b) \
    __atomic_fetch_add(&(a)->v, b, __ATOMIC_SEQ_CST)

#elif HAVE_SYNC_BUILTINS

#define atomic_load(p) \
    __sync_fetch_and_add(&(p)->v, 0)
#define atomic_store(p, val) \
    (__sync_synchronize(), (p)->v = (val), __sync_synchronize())
#define atomic_fetch_add(a, b) \
    __sync_fetch_and_add(&(a)->v, b)

#else

// This is extremely wrong. The build system actually disables code that has
// a serious dependency on working atomics, so this is barely ok.
#define atomic_load(p) ((p)->v)
#define atomic_store(p, val) ((p)->v = (val))
#define atomic_fetch_add(a, b) (((a)->v += (b)) - (b))

#undef HAVE_ATOMICS
#define HAVE_ATOMICS 0

#endif /* no atomics */

#endif /* else HAVE_STDATOMIC */

#endif
