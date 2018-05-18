/* Copyright (C) 2018 the mpv developers
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <pthread.h>

#include "common/common.h"
#include "osdep/threads.h"
#include "osdep/timer.h"

#include "thread_pool.h"

// Threads destroy themselves after this many seconds, if there's no new work
// and the thread count is above the configured minimum.
#define DESTROY_TIMEOUT 10

struct work {
    void (*fn)(void *ctx);
    void *fn_ctx;
};

struct mp_thread_pool {
    int min_threads, max_threads;

    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // --- the following fields are protected by lock

    pthread_t *threads;
    int num_threads;

    // Number of threads which have taken up work and are still processing it.
    int busy_threads;

    bool terminate;

    struct work *work;
    int num_work;
};

static void *worker_thread(void *arg)
{
    struct mp_thread_pool *pool = arg;

    mpthread_set_name("worker");

    pthread_mutex_lock(&pool->lock);

    struct timespec ts = {0};
    bool got_timeout = false;
    while (1) {
        struct work work = {0};
        if (pool->num_work > 0) {
            work = pool->work[pool->num_work - 1];
            pool->num_work -= 1;
        }

        if (!work.fn) {
            if (got_timeout || pool->terminate)
                break;

            if (pool->num_threads > pool->min_threads) {
                if (!ts.tv_sec && !ts.tv_nsec)
                    ts = mp_rel_time_to_timespec(DESTROY_TIMEOUT);
                if (pthread_cond_timedwait(&pool->wakeup, &pool->lock, &ts))
                    got_timeout = pool->num_threads > pool->min_threads;
            } else {
                pthread_cond_wait(&pool->wakeup, &pool->lock);
            }
            continue;
        }

        pool->busy_threads += 1;
        pthread_mutex_unlock(&pool->lock);

        work.fn(work.fn_ctx);

        pthread_mutex_lock(&pool->lock);
        pool->busy_threads -= 1;

        ts = (struct timespec){0};
        got_timeout = false;
    }

    // If no termination signal was given, it must mean we died because of a
    // timeout, and nobody is waiting for us. We have to remove ourselves.
    if (!pool->terminate) {
        for (int n = 0; n < pool->num_threads; n++) {
            if (pthread_equal(pool->threads[n], pthread_self())) {
                pthread_detach(pthread_self());
                MP_TARRAY_REMOVE_AT(pool->threads, pool->num_threads, n);
                pthread_mutex_unlock(&pool->lock);
                return NULL;
            }
        }
        assert(0);
    }

    pthread_mutex_unlock(&pool->lock);
    return NULL;
}

static void thread_pool_dtor(void *ctx)
{
    struct mp_thread_pool *pool = ctx;


    pthread_mutex_lock(&pool->lock);

    pool->terminate = true;
    pthread_cond_broadcast(&pool->wakeup);

    pthread_t *threads = pool->threads;
    int num_threads = pool->num_threads;

    pool->threads = NULL;
    pool->num_threads = 0;

    pthread_mutex_unlock(&pool->lock);

    for (int n = 0; n < num_threads; n++)
        pthread_join(threads[n], NULL);

    assert(pool->num_work == 0);
    assert(pool->num_threads == 0);
    pthread_cond_destroy(&pool->wakeup);
    pthread_mutex_destroy(&pool->lock);
}

static void add_thread(struct mp_thread_pool *pool)
{
    pthread_t thread;

    if (pthread_create(&thread, NULL, worker_thread, pool) == 0)
        MP_TARRAY_APPEND(pool, pool->threads, pool->num_threads, thread);
}

struct mp_thread_pool *mp_thread_pool_create(void *ta_parent, int init_threads,
                                             int min_threads, int max_threads)
{
    assert(min_threads >= 0);
    assert(init_threads <= min_threads);
    assert(max_threads > 0 && max_threads >= min_threads);

    struct mp_thread_pool *pool = talloc_zero(ta_parent, struct mp_thread_pool);
    talloc_set_destructor(pool, thread_pool_dtor);

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->wakeup, NULL);

    pool->min_threads = min_threads;
    pool->max_threads = max_threads;

    pthread_mutex_lock(&pool->lock);
    for (int n = 0; n < init_threads; n++)
        add_thread(pool);
    bool ok = pool->num_threads >= init_threads;
    pthread_mutex_unlock(&pool->lock);

    if (!ok)
        TA_FREEP(&pool);

    return pool;
}

bool mp_thread_pool_queue(struct mp_thread_pool *pool, void (*fn)(void *ctx),
                          void *fn_ctx)
{
    bool ok = true;

    assert(fn);

    pthread_mutex_lock(&pool->lock);
    struct work work = {fn, fn_ctx};

    // If there are not enough threads to process all at once, but we can
    // create a new thread, then do so. If work is queued quickly, it can
    // happen that not all available threads have picked up work yet (up to
    // num_threads - busy_threads threads), which has to be accounted for.
    if (pool->busy_threads + pool->num_work + 1 > pool->num_threads &&
        pool->num_threads < pool->max_threads)
    {
        // We ignore failures, unless there are no threads available (below).
        add_thread(pool);
    }

    if (pool->num_threads) {
        MP_TARRAY_INSERT_AT(pool, pool->work, pool->num_work, 0, work);
        pthread_cond_signal(&pool->wakeup);
    } else {
        ok = false;
    }

    pthread_mutex_unlock(&pool->lock);
    return ok;
}
