/*
 * Implementation of non-cryptographic pseudo-random number
 * generator algorithm known as xoshiro.
 *
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

#include <stdint.h>

/*
 * Internal state for the PRNG, should be initialized via mp_rand_seed().
 */
typedef struct mp_rand_state {
    uint64_t v[4];
} mp_rand_state;

/*
 * Initialize the pseudo-random number generator's state with
 * the given 64-bit seed. If the seed is 0, it is randomized.
 */
mp_rand_state mp_rand_seed(uint64_t seed);

/*
 * Return the next 64-bit pseudo-random integer, and update the state
 * accordingly.
 */
uint64_t mp_rand_next(mp_rand_state *s);

/*
 * Return a double value in the range of [0.0, 1.0) with uniform
 * distribution, and update the state accordingly.
 */
double mp_rand_next_double(mp_rand_state *s);

/*
 * Return a 32 bit integer in the range [min, max) with uniform distribution.
 * Caller should ensure `min < max`.
 */
uint32_t mp_rand_in_range32(mp_rand_state *s, uint32_t min, uint32_t max);
