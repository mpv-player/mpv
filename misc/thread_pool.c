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

#include <pthread.h>

#include "common/common.h"

#include "thread_pool.h"

struct work {
    void (*fn)(void *ctx);
    void *fn_ctx;
};

struct mp_thread_pool {
    pthread_t *threads;
    int num_threads;

    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // --- the following fields are protected by lock
    bool terminate;
    struct work *work;
    int num_work;
};

static void *worker_thread(void *arg)
{
    struct mp_thread_pool *pool = arg;

    pthread_mutex_lock(&pool->lock);
    while (1) {
        while (!pool->num_work && !pool->terminate)
            pthread_cond_wait(&pool->wakeup, &pool->lock);

        if (!pool->num_work && pool->terminate)
            break;

        assert(pool->num_work > 0);
        struct work work = pool->work[pool->num_work - 1];
        pool->num_work -= 1;

        pthread_mutex_unlock(&pool->lock);
        work.fn(work.fn_ctx);
        pthread_mutex_lock(&pool->lock);
    }
    assert(pool->num_work == 0);
    pthread_mutex_unlock(&pool->lock);

    return NULL;
}

static void thread_pool_dtor(void *ctx)
{
    struct mp_thread_pool *pool = ctx;

    pthread_mutex_lock(&pool->lock);
    pool->terminate = true;
    pthread_cond_broadcast(&pool->wakeup);
    pthread_mutex_unlock(&pool->lock);

    for (int n = 0; n < pool->num_threads; n++)
        pthread_join(pool->threads[n], NULL);

    assert(pool->num_work == 0);
    pthread_cond_destroy(&pool->wakeup);
    pthread_mutex_destroy(&pool->lock);
}

// Create a thread pool with the given number of worker threads. This can return
// NULL if the worker threads could not be created. The thread pool can be
// destroyed with talloc_free(pool), or indirectly with talloc_free(ta_parent).
// If there are still work items on freeing, it will block until all work items
// are done, and the threads terminate.
struct mp_thread_pool *mp_thread_pool_create(void *ta_parent, int threads)
{
    assert(threads > 0);

    struct mp_thread_pool *pool = talloc_zero(ta_parent, struct mp_thread_pool);
    talloc_set_destructor(pool, thread_pool_dtor);

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->wakeup, NULL);

    for (int n = 0; n < threads; n++) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, worker_thread, pool)) {
            talloc_free(pool);
            return NULL;
        }
        MP_TARRAY_APPEND(pool, pool->threads, pool->num_threads, thread);
    }

    return pool;
}

// Queue a function to be run on a worker thread: fn(fn_ctx)
// If no worker thread is currently available, it's appended to a list in memory
// with unbounded size. This function always returns immediately.
// Concurrent queue calls are allowed, as long as it does not overlap with
// pool destruction.
void mp_thread_pool_queue(struct mp_thread_pool *pool, void (*fn)(void *ctx),
                          void *fn_ctx)
{
    pthread_mutex_lock(&pool->lock);
    struct work work = {fn, fn_ctx};
    MP_TARRAY_INSERT_AT(pool, pool->work, pool->num_work, 0, work);
    pthread_cond_signal(&pool->wakeup);
    pthread_mutex_unlock(&pool->lock);
}
