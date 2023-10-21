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

#include <stdint.h>

#include "osdep/threads.h"
#include "random.h"

static uint64_t state[4];
static mp_static_mutex state_mutex = MP_STATIC_MUTEX_INITIALIZER;

static inline uint64_t rotl_u64(const uint64_t x, const int k)
{
    return (x << k) | (x >> (64 - k));
}

static inline uint64_t splitmix64(uint64_t *const x)
{
    uint64_t z = (*x += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

void mp_rand_seed(uint64_t seed)
{
    mp_mutex_lock(&state_mutex);
    state[0] = seed;
    for (int i = 1; i < 4; i++)
        state[i] = splitmix64(&seed);
    mp_mutex_unlock(&state_mutex);
}

uint64_t mp_rand_next(void)
{
    uint64_t result, t;

    mp_mutex_lock(&state_mutex);

    result = rotl_u64(state[1] * 5, 7) * 9;
    t = state[1] << 17;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];
    state[2] ^= t;
    state[3] = rotl_u64(state[3], 45);

    mp_mutex_unlock(&state_mutex);

    return result;
}

double mp_rand_next_double(void)
{
    return (mp_rand_next() >> 11) * 0x1.0p-53;
}
