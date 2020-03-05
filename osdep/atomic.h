/*
 * This file is part of mpv.
 * Copyright (c) 2013 Stefano Pigozzi <stefano.pigozzi@gmail.com>
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

#ifndef MP_ATOMIC_H
#define MP_ATOMIC_H

#include <inttypes.h>
#include "config.h"

#if HAVE_STDATOMIC
#include <stdatomic.h>
typedef _Atomic float mp_atomic_float;
typedef _Atomic int64_t mp_atomic_int64;
typedef _Atomic uint64_t mp_atomic_uint64;
#else

// Emulate the parts of C11 stdatomic.h needed by mpv.

typedef struct { unsigned long v;      } atomic_ulong;
typedef struct { int v;                } atomic_int;
typedef struct { unsigned int v;       } atomic_uint;
typedef struct { _Bool v;              } atomic_bool;
typedef struct { long long v;          } atomic_llong;
typedef struct { uint_least32_t v;     } atomic_uint_least32_t;
typedef struct { unsigned long long v; } atomic_ullong;

typedef struct { float v;              } mp_atomic_float;
typedef struct { int64_t v;            } mp_atomic_int64;
typedef struct { uint64_t v;           } mp_atomic_uint64;

#define ATOMIC_VAR_INIT(x) \
    {.v = (x)}

#define memory_order_relaxed 1
#define memory_order_seq_cst 2
#define memory_order_acq_rel 3

#include <pthread.h>

extern pthread_mutex_t mp_atomic_mutex;

#define atomic_load(p)                                  \
    ({ __typeof__(p) p_ = (p);                          \
       pthread_mutex_lock(&mp_atomic_mutex);            \
       __typeof__(p_->v) v_ = p_->v;                    \
       pthread_mutex_unlock(&mp_atomic_mutex);          \
       v_; })
#define atomic_store(p, val)                            \
    ({ __typeof__(val) val_ = (val);                    \
       __typeof__(p) p_ = (p);                          \
       pthread_mutex_lock(&mp_atomic_mutex);            \
       p_->v = val_;                                    \
       pthread_mutex_unlock(&mp_atomic_mutex); })
#define atomic_fetch_op(a, b, op)                       \
    ({ __typeof__(a) a_ = (a);                          \
       __typeof__(b) b_ = (b);                          \
       pthread_mutex_lock(&mp_atomic_mutex);            \
       __typeof__(a_->v) v_ = a_->v;                    \
       a_->v = v_ op b_;                                \
       pthread_mutex_unlock(&mp_atomic_mutex);          \
       v_; })
#define atomic_fetch_add(a, b) atomic_fetch_op(a, b, +)
#define atomic_fetch_and(a, b) atomic_fetch_op(a, b, &)
#define atomic_fetch_or(a, b)  atomic_fetch_op(a, b, |)
#define atomic_exchange(p, new)                         \
    ({ __typeof__(p) p_ = (p);                          \
       pthread_mutex_lock(&mp_atomic_mutex);            \
       __typeof__(p_->v) res_ = p_->v;                  \
       p_->v = (new);                                   \
       pthread_mutex_unlock(&mp_atomic_mutex);          \
       res_; })
#define atomic_compare_exchange_strong(p, old, new)     \
    ({ __typeof__(p) p_ = (p);                          \
       __typeof__(old) old_ = (old);                    \
       __typeof__(new) new_ = (new);                    \
       pthread_mutex_lock(&mp_atomic_mutex);            \
       int res_ = p_->v == *old_;                       \
       if (res_) {                                      \
           p_->v = new_;                                \
       } else {                                         \
           *old_ = p_->v;                               \
       }                                                \
       pthread_mutex_unlock(&mp_atomic_mutex);          \
       res_; })

#define atomic_load_explicit(a, b)                      \
    atomic_load(a)

#define atomic_exchange_explicit(a, b, c)               \
    atomic_exchange(a, b)

#endif /* else HAVE_STDATOMIC */

#endif
