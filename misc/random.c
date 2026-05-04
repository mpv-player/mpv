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

#include <libavutil/random_seed.h>

#include "common/common.h"
#include "misc/mp_assert.h"
#include "osdep/timer.h"
#include "random.h"


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

mp_rand_state mp_rand_seed(uint64_t seed)
{
    mp_rand_state ret;

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    seed = 42;
#endif

    if (seed == 0) {
        if (av_random_bytes((void *)ret.v, sizeof(ret.v)) == 0) {
            return ret;
        }
        seed = mp_raw_time_ns();
        seed ^= (uintptr_t)&mp_rand_seed; // ASLR, hopefully
        seed += (uintptr_t)&ret; // stack position
    }

    ret.v[0] = seed;
    for (int i = 1; i < 4; i++)
        ret.v[i] = splitmix64(&seed);
    return ret;
}

uint64_t mp_rand_next(mp_rand_state *s)
{
    uint64_t result, t;
    uint64_t *state = s->v;

    result = rotl_u64(state[1] * 5, 7) * 9;
    t = state[1] << 17;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];
    state[2] ^= t;
    state[3] = rotl_u64(state[3], 45);

    return result;
}

double mp_rand_next_double(mp_rand_state *s)
{
    return (mp_rand_next(s) >> 11) * 0x1.0p-53;
}

// <https://web.archive.org/web/20250321082025/
// https://www.pcg-random.org/posts/bounded-rands.html#bitmask-with-rejection-unbiased-apples-method>
uint32_t mp_rand_in_range32(mp_rand_state *s, uint32_t min, uint32_t max)
{
    mp_assert(min < max);
    uint32_t range = max - min;
    uint32_t mask = mp_round_next_power_of_2(range) - 1;
    uint32_t ret;
    do {
        ret = mp_rand_next(s) & mask;
    } while (ret >= range);
    return min + ret;
}
