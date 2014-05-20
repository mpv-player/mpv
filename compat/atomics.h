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
#define atomic_load(p) \
    (mp_memory_barrier(), (p)->v)
#define atomic_store(p, val) \
    ((p)->v = (val), mp_memory_barrier())

#if HAVE_ATOMIC_BUILTINS
# define mp_memory_barrier() \
    __atomic_thread_fence(__ATOMIC_SEQ_CST)
# define atomic_fetch_add(a, b) \
    __atomic_add_fetch(&(a)->v, b, __ATOMIC_SEQ_CST)
#elif HAVE_SYNC_BUILTINS
# define mp_memory_barrier() \
    __sync_synchronize()
# define atomic_fetch_add(a, b) \
    (__sync_add_and_fetch(&(a)->v, b), mp_memory_barrier())
#else
# error "this should have been a configuration error, report a bug please"
#endif

#endif /* else HAVE_STDATOMIC */

#endif
