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

#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>
#include <assert.h>

#include "common/common.h"
#include "common/msg.h"
#include "misc/random.h"
#include "timer.h"

static uint64_t raw_time_offset;
static pthread_once_t timer_init_once = PTHREAD_ONCE_INIT;

static void do_timer_init(void)
{
    mp_raw_time_init();
    mp_rand_seed(mp_raw_time_us());
    raw_time_offset = mp_raw_time_us();
    // Arbitrary additional offset to avoid confusing relative/absolute times.
    // Also,we rule that the timer never returns 0 (so default-initialized
    // time values will be always in the past).
    raw_time_offset -= MP_START_TIME;
}

void mp_time_init(void)
{
    pthread_once(&timer_init_once, do_timer_init);
}

int64_t mp_time_us(void)
{
    int64_t r = mp_raw_time_us() - raw_time_offset;
    if (r < MP_START_TIME)
        r = MP_START_TIME;
    return r;
}

double mp_time_sec(void)
{
    return mp_time_us() / (double)(1000 * 1000);
}

int64_t mp_add_timeout(int64_t time_us, double timeout_sec)
{
    assert(time_us > 0); // mp_time_us() returns strictly positive values
    double t = MPCLAMP(timeout_sec * (1000 * 1000), -0x1p63, 0x1p63);
    int64_t ti = t == 0x1p63 ? INT64_MAX : (int64_t)t;
    if (ti > INT64_MAX - time_us)
        return INT64_MAX;
    if (ti <= -time_us)
        return 1;
    return time_us + ti;
}

static int get_realtime(struct timespec *out_ts)
{
#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    return clock_gettime(CLOCK_REALTIME, out_ts);
#else
    // OSX
    struct timeval tv;
    gettimeofday(&tv, NULL);
    out_ts->tv_sec = tv.tv_sec;
    out_ts->tv_nsec = tv.tv_usec * 1000UL;
    return 0;
#endif
}

struct timespec mp_time_us_to_realtime(int64_t time_us)
{
    struct timespec ts = {0};
    if (get_realtime(&ts) != 0)
        return ts;

    int64_t time_ns = MPMIN(INT64_MAX / 1000, time_us) * 1000;
    int64_t time_now = mp_time_us() * 1000;

    // clamp to 1000 days in the future
    int64_t time_rel = MPMIN(time_now - time_ns,
                             1000 * 24 * 60 * 60 * INT64_C(1000000000));
    ts.tv_sec += time_rel / INT64_C(1000000000);
    ts.tv_nsec += time_rel % INT64_C(1000000000);

    if (ts.tv_nsec >= INT64_C(1000000000)) {
        ts.tv_sec++;
        ts.tv_nsec -= INT64_C(1000000000);
    }

    return ts;
}

struct timespec mp_rel_time_to_timespec(double timeout_sec)
{
    return mp_time_us_to_realtime(mp_add_timeout(mp_time_us(), timeout_sec));
}

#if 0
#include <stdio.h>
#include "threads.h"

#define TEST_SLEEP 1

int main(void) {
    int c = 2000000;
    int64_t j, r, t = 0;
    pthread_mutex_t mtx;
    pthread_mutex_init(&mtx, NULL);
    pthread_cond_t cnd;
    pthread_cond_init(&cnd, NULL);

    mp_time_init();

    for (int i = 0; i < c; i++) {
        const int delay = rand() / (RAND_MAX / 1e5);
        r = mp_time_us();
#if TEST_SLEEP
        mp_sleep_us(delay);
#else
        struct timespec ts = mp_time_us_to_realtime(r + delay);
        pthread_cond_timedwait(&cnd, &mtx, &ts);
#endif
        j = (mp_time_us() - r) - delay;
        printf("sleep time: t=%"PRId64" sleep=%8i err=%5i\n", r, delay, (int)j);
        t += j;
    }
    fprintf(stderr, "average error:\t%i\n", (int)(t / c));

    return 0;
}
#endif
