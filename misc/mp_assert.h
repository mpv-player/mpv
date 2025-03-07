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

#pragma once

/**
 * @brief This file provides two macros, mp_assert and mp_require, used to check
 * for conditions that should never occur.
 *
 * The mp_assert macro is a convenience wrapper around the standard assert,
 * which also sinks unused variables to prevent compiler warnings. It should be
 * used for debugging purposes only to check for conditions that should never
 * occur. This macro can be disabled in release builds by defining NDEBUG and
 * should not be used for error handling.
 *
 * The mp_require macro is similar to mp_assert but more strict. It enforces
 * critical assertions even in release builds. If the condition is not met, this
 * macro will abort the program, making it suitable for error handling in cases
 * where recovery is impossible. Note that this macro does not log any messages
 * to keep as small footprint as possible in NDEBUG builds.
 */

#ifndef NDEBUG

#include <assert.h>

#define mp_assert assert
#define mp_require assert

#else

#include <stdio.h>
#include <stdlib.h>

#define mp_require(expr)                                       \
    do {                                                       \
        if (!(expr))                                           \
            abort();                                           \
    } while (0)

#define mp_assert(expr) do { (void)sizeof(expr); } while (0)

#endif
