#ifndef MPV_MP_THREAD_POOL_H
#define MPV_MP_THREAD_POOL_H

struct mp_thread_pool;

// Create a thread pool with the given number of worker threads. This can return
// NULL if the worker threads could not be created. The thread pool can be
// destroyed with talloc_free(pool), or indirectly with talloc_free(ta_parent).
// If there are still work items on freeing, it will block until all work items
// are done, and the threads terminate.
// init_threads is the number of threads created in this function (and it fails
// if it could not be done). min_threads must be >=, if it's >, then the
// remaining threads will be created on demand, but never destroyed.
// If init_threads > 0, then mp_thread_pool_queue() can never fail.
// If init_threads == 0, mp_thread_pool_create() itself can never fail.
struct mp_thread_pool *mp_thread_pool_create(void *ta_parent, int init_threads,
                                             int min_threads, int max_threads);

// Queue a function to be run on a worker thread: fn(fn_ctx)
// If no worker thread is currently available, it's appended to a list in memory
// with unbounded size. This function always returns immediately.
// Concurrent queue calls are allowed, as long as it does not overlap with
// pool destruction.
// This function is explicitly thread-safe.
// Cannot fail if thread pool was created with at least 1 thread.
bool mp_thread_pool_queue(struct mp_thread_pool *pool, void (*fn)(void *ctx),
                          void *fn_ctx);

// Like mp_thread_pool_queue(), but only queue the item and succeed if a thread
// can be reserved for the item (i.e. minimal wait time instead of unbounded).
bool mp_thread_pool_run(struct mp_thread_pool *pool, void (*fn)(void *ctx),
                        void *fn_ctx);

#endif
