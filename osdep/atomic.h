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

#include <stdatomic.h>
typedef _Atomic float mp_atomic_float;
typedef _Atomic double mp_atomic_double;
typedef _Atomic int64_t mp_atomic_int64;
typedef _Atomic uint64_t mp_atomic_uint64;

#endif
