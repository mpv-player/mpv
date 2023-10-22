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

    mp_mutex lock;
    mp_cond wakeup;

    // --- the following fields are protected by lock

    mp_thread *threads;
    int num_threads;

    // Number of threads which have taken up work and are still processing it.
    int busy_threads;

    bool terminate;

    struct work *work;
    int num_work;
};

static MP_THREAD_VOID worker_thread(void *arg)
{
    struct mp_thread_pool *pool = arg;

    mp_thread_set_name("worker");

    mp_mutex_lock(&pool->lock);

    int64_t destroy_deadline = 0;
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
                if (!destroy_deadline)
                    destroy_deadline = mp_time_ns() + MP_TIME_S_TO_NS(DESTROY_TIMEOUT);
                if (mp_cond_timedwait_until(&pool->wakeup, &pool->lock, destroy_deadline))
                    got_timeout = pool->num_threads > pool->min_threads;
            } else {
                mp_cond_wait(&pool->wakeup, &pool->lock);
            }
            continue;
        }

        pool->busy_threads += 1;
        mp_mutex_unlock(&pool->lock);

        work.fn(work.fn_ctx);

        mp_mutex_lock(&pool->lock);
        pool->busy_threads -= 1;

        destroy_deadline = 0;
        got_timeout = false;
    }

    // If no termination signal was given, it must mean we died because of a
    // timeout, and nobody is waiting for us. We have to remove ourselves.
    if (!pool->terminate) {
        for (int n = 0; n < pool->num_threads; n++) {
            if (mp_thread_id_equal(mp_thread_get_id(pool->threads[n]),
                                   mp_thread_current_id()))
            {
                mp_thread_detach(pool->threads[n]);
                MP_TARRAY_REMOVE_AT(pool->threads, pool->num_threads, n);
                mp_mutex_unlock(&pool->lock);
                MP_THREAD_RETURN();
            }
        }
        MP_ASSERT_UNREACHABLE();
    }

    mp_mutex_unlock(&pool->lock);
    MP_THREAD_RETURN();
}

static void thread_pool_dtor(void *ctx)
{
    struct mp_thread_pool *pool = ctx;


    mp_mutex_lock(&pool->lock);

    pool->terminate = true;
    mp_cond_broadcast(&pool->wakeup);

    mp_thread *threads = pool->threads;
    int num_threads = pool->num_threads;

    pool->threads = NULL;
    pool->num_threads = 0;

    mp_mutex_unlock(&pool->lock);

    for (int n = 0; n < num_threads; n++)
        mp_thread_join(threads[n]);

    assert(pool->num_work == 0);
    assert(pool->num_threads == 0);
    mp_cond_destroy(&pool->wakeup);
    mp_mutex_destroy(&pool->lock);
}

static bool add_thread(struct mp_thread_pool *pool)
{
    mp_thread thread;

    if (mp_thread_create(&thread, worker_thread, pool) != 0)
        return false;

    MP_TARRAY_APPEND(pool, pool->threads, pool->num_threads, thread);
    return true;
}

struct mp_thread_pool *mp_thread_pool_create(void *ta_parent, int init_threads,
                                             int min_threads, int max_threads)
{
    assert(min_threads >= 0);
    assert(init_threads <= min_threads);
    assert(max_threads > 0 && max_threads >= min_threads);

    struct mp_thread_pool *pool = talloc_zero(ta_parent, struct mp_thread_pool);
    talloc_set_destructor(pool, thread_pool_dtor);

    mp_mutex_init(&pool->lock);
    mp_cond_init(&pool->wakeup);

    pool->min_threads = min_threads;
    pool->max_threads = max_threads;

    mp_mutex_lock(&pool->lock);
    for (int n = 0; n < init_threads; n++)
        add_thread(pool);
    bool ok = pool->num_threads >= init_threads;
    mp_mutex_unlock(&pool->lock);

    if (!ok)
        TA_FREEP(&pool);

    return pool;
}

static bool thread_pool_add(struct mp_thread_pool *pool, void (*fn)(void *ctx),
                            void *fn_ctx, bool allow_queue)
{
    bool ok = true;

    assert(fn);

    mp_mutex_lock(&pool->lock);
    struct work work = {fn, fn_ctx};

    // If there are not enough threads to process all at once, but we can
    // create a new thread, then do so. If work is queued quickly, it can
    // happen that not all available threads have picked up work yet (up to
    // num_threads - busy_threads threads), which has to be accounted for.
    if (pool->busy_threads + pool->num_work + 1 > pool->num_threads &&
        pool->num_threads < pool->max_threads)
    {
        if (!add_thread(pool)) {
            // If we can queue it, it'll get done as long as there is 1 thread.
            ok = allow_queue && pool->num_threads > 0;
        }
    }

    if (ok) {
        MP_TARRAY_INSERT_AT(pool, pool->work, pool->num_work, 0, work);
        mp_cond_signal(&pool->wakeup);
    }

    mp_mutex_unlock(&pool->lock);
    return ok;
}

bool mp_thread_pool_queue(struct mp_thread_pool *pool, void (*fn)(void *ctx),
                          void *fn_ctx)
{
    return thread_pool_add(pool, fn, fn_ctx, true);
}

bool mp_thread_pool_run(struct mp_thread_pool *pool, void (*fn)(void *ctx),
                        void *fn_ctx)
{
    return thread_pool_add(pool, fn, fn_ctx, false);
}
