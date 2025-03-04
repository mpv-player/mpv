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

#include <stdbool.h>
#include <assert.h>

#include "common/common.h"
#include "osdep/threads.h"
#include "osdep/timer.h"

#include "dispatch.h"

struct mp_dispatch_queue {
    struct mp_dispatch_item *head, *tail;
    mp_mutex lock;
    mp_cond cond;
    void (*wakeup_fn)(void *wakeup_ctx);
    void *wakeup_ctx;
    void (*onlock_fn)(void *onlock_ctx);
    void *onlock_ctx;
    // Time at which mp_dispatch_queue_process() should return.
    int64_t wait;
    // Make mp_dispatch_queue_process() exit if it's idle.
    bool interrupted;
    // The target thread is in mp_dispatch_queue_process() (and either idling,
    // locked, or running a dispatch callback).
    bool in_process;
    mp_thread_id in_process_thread_id;
    // The target thread is in mp_dispatch_queue_process(), and currently
    // something has exclusive access to it (e.g. running a dispatch callback,
    // or a different thread got it with mp_dispatch_lock()).
    bool locked;
    // A mp_dispatch_lock() call is requesting an exclusive lock.
    size_t lock_requests;
    // locked==true is due to a mp_dispatch_lock() call (for debugging).
    bool locked_explicit;
    mp_thread_id locked_explicit_thread_id;
};

struct mp_dispatch_item {
    mp_dispatch_fn fn;
    void *fn_data;
    bool asynchronous;
    bool mergeable;
    bool completed;
    struct mp_dispatch_item *next;
};

static void queue_dtor(void *p)
{
    struct mp_dispatch_queue *queue = p;
    mp_assert(!queue->head);
    mp_assert(!queue->in_process);
    mp_assert(!queue->lock_requests);
    mp_assert(!queue->locked);
    mp_cond_destroy(&queue->cond);
    mp_mutex_destroy(&queue->lock);
}

// A dispatch queue lets other threads run callbacks in a target thread.
// The target thread is the thread which calls mp_dispatch_queue_process().
// Free the dispatch queue with talloc_free(). At the time of destruction,
// the queue must be empty. The easiest way to guarantee this is to
// terminate all potential senders, then call mp_dispatch_run() with a
// function that e.g. makes the target thread exit, then mp_thread_join() the
// target thread, and finally destroy the queue. Another way is calling
// mp_dispatch_queue_process() after terminating all potential senders, and
// then destroying the queue.
struct mp_dispatch_queue *mp_dispatch_create(void *ta_parent)
{
    struct mp_dispatch_queue *queue = talloc_ptrtype(ta_parent, queue);
    *queue = (struct mp_dispatch_queue){0};
    talloc_set_destructor(queue, queue_dtor);
    mp_mutex_init(&queue->lock);
    mp_cond_init(&queue->cond);
    return queue;
}

// Set a custom function that should be called to guarantee that the target
// thread wakes up. This is intended for use with code that needs to block
// on non-pthread primitives, such as e.g. poll(). In the case of poll(),
// the wakeup_fn could for example write a byte into a "wakeup" pipe in order
// to unblock the poll(). The wakeup_fn is called from the dispatch queue
// when there are new dispatch items, and the target thread should then enter
// mp_dispatch_queue_process() as soon as possible.
// Note that this setter does not do internal synchronization, so you must set
// it before other threads see it.
void mp_dispatch_set_wakeup_fn(struct mp_dispatch_queue *queue,
                               void (*wakeup_fn)(void *wakeup_ctx),
                               void *wakeup_ctx)
{
    queue->wakeup_fn = wakeup_fn;
    queue->wakeup_ctx = wakeup_ctx;
}

// Set a function that will be called by mp_dispatch_lock() if the target thread
// is not calling mp_dispatch_queue_process() right now. This is an obscure,
// optional mechanism to make a worker thread react to external events more
// quickly. The idea is that the callback will make the worker thread to stop
// doing whatever (e.g. by setting a flag), and call mp_dispatch_queue_process()
// in order to let mp_dispatch_lock() calls continue sooner.
// Like wakeup_fn, this setter does no internal synchronization, and you must
// not access the dispatch queue itself from the callback.
void mp_dispatch_set_onlock_fn(struct mp_dispatch_queue *queue,
                               void (*onlock_fn)(void *onlock_ctx),
                               void *onlock_ctx)
{
    queue->onlock_fn = onlock_fn;
    queue->onlock_ctx = onlock_ctx;
}

static void mp_dispatch_append(struct mp_dispatch_queue *queue,
                               struct mp_dispatch_item *item)
{
    mp_mutex_lock(&queue->lock);
    if (item->mergeable) {
        for (struct mp_dispatch_item *cur = queue->head; cur; cur = cur->next) {
            if (cur->mergeable && cur->fn == item->fn &&
                cur->fn_data == item->fn_data)
            {
                talloc_free(item);
                mp_mutex_unlock(&queue->lock);
                return;
            }
        }
    }

    if (queue->tail) {
        queue->tail->next = item;
    } else {
        queue->head = item;
    }
    queue->tail = item;

    // Wake up the main thread; note that other threads might wait on this
    // condition for reasons, so broadcast the condition.
    mp_cond_broadcast(&queue->cond);
    // No wakeup callback -> assume mp_dispatch_queue_process() needs to be
    // interrupted instead.
    if (!queue->wakeup_fn)
        queue->interrupted = true;
    mp_mutex_unlock(&queue->lock);

    if (queue->wakeup_fn)
        queue->wakeup_fn(queue->wakeup_ctx);
}

// Enqueue a callback to run it on the target thread asynchronously. The target
// thread will run fn(fn_data) as soon as it enter mp_dispatch_queue_process.
// Note that mp_dispatch_enqueue() will usually return long before that happens.
// It's up to the user to signal completion of the callback. It's also up to
// the user to guarantee that the context fn_data has correct lifetime, i.e.
// lives until the callback is run, and is freed after that.
void mp_dispatch_enqueue(struct mp_dispatch_queue *queue,
                         mp_dispatch_fn fn, void *fn_data)
{
    struct mp_dispatch_item *item = talloc_ptrtype(NULL, item);
    *item = (struct mp_dispatch_item){
        .fn = fn,
        .fn_data = fn_data,
        .asynchronous = true,
    };
    mp_dispatch_append(queue, item);
}

// Like mp_dispatch_enqueue(), but the queue code will call talloc_free(fn_data)
// after the fn callback has been run. (The callback could trivially do that
// itself, but it makes it easier to implement synchronous and asynchronous
// requests with the same callback implementation.)
void mp_dispatch_enqueue_autofree(struct mp_dispatch_queue *queue,
                                  mp_dispatch_fn fn, void *fn_data)
{
    struct mp_dispatch_item *item = talloc_ptrtype(NULL, item);
    *item = (struct mp_dispatch_item){
        .fn = fn,
        .fn_data = talloc_steal(item, fn_data),
        .asynchronous = true,
    };
    mp_dispatch_append(queue, item);
}

// Like mp_dispatch_enqueue(), but
void mp_dispatch_enqueue_notify(struct mp_dispatch_queue *queue,
                                mp_dispatch_fn fn, void *fn_data)
{
    struct mp_dispatch_item *item = talloc_ptrtype(NULL, item);
    *item = (struct mp_dispatch_item){
        .fn = fn,
        .fn_data = fn_data,
        .mergeable = true,
        .asynchronous = true,
    };
    mp_dispatch_append(queue, item);
}

// Remove already queued item. Only items enqueued with the following functions
// can be canceled:
//  - mp_dispatch_enqueue()
//  - mp_dispatch_enqueue_notify()
// Items which were enqueued, and which are currently executing, can not be
// canceled anymore. This function is mostly for being called from the same
// context as mp_dispatch_queue_process(), where the "currently executing" case
// can be excluded.
void mp_dispatch_cancel_fn(struct mp_dispatch_queue *queue,
                           mp_dispatch_fn fn, void *fn_data)
{
    mp_mutex_lock(&queue->lock);
    struct mp_dispatch_item **pcur = &queue->head;
    queue->tail = NULL;
    while (*pcur) {
        struct mp_dispatch_item *cur = *pcur;
        if (cur->fn == fn && cur->fn_data == fn_data) {
            *pcur = cur->next;
            talloc_free(cur);
        } else {
            queue->tail = cur;
            pcur = &cur->next;
        }
    }
    mp_mutex_unlock(&queue->lock);
}

// Run fn(fn_data) on the target thread synchronously. This function enqueues
// the callback and waits until the target thread is done doing this.
// This is redundant to calling the function inside mp_dispatch_[un]lock(),
// but can be helpful with code that relies on TLS (such as OpenGL).
void mp_dispatch_run(struct mp_dispatch_queue *queue,
                     mp_dispatch_fn fn, void *fn_data)
{
    struct mp_dispatch_item item = {
        .fn = fn,
        .fn_data = fn_data,
    };
    mp_dispatch_append(queue, &item);

    mp_mutex_lock(&queue->lock);
    while (!item.completed)
        mp_cond_wait(&queue->cond, &queue->lock);
    mp_mutex_unlock(&queue->lock);
}

// Process any outstanding dispatch items in the queue. This also handles
// suspending or locking the this thread from another thread via
// mp_dispatch_lock().
// The timeout specifies the minimum wait time. The actual time spent in this
// function can be much higher if the suspending/locking functions are used, or
// if executing the dispatch items takes time. On the other hand, this function
// can return much earlier than the timeout due to sporadic wakeups.
// Note that this will strictly return only after:
//      - timeout has passed,
//      - all queue items were processed,
//      - the possibly acquired lock has been released
// It's possible to cancel the timeout by calling mp_dispatch_interrupt().
// Reentrant calls are not allowed. There can be only 1 thread calling
// mp_dispatch_queue_process() at a time. In addition, mp_dispatch_lock() can
// not be called from a thread that is calling mp_dispatch_queue_process() (i.e.
// no enqueued callback can call the lock/unlock functions).
void mp_dispatch_queue_process(struct mp_dispatch_queue *queue, double timeout)
{
    mp_mutex_lock(&queue->lock);
    queue->wait = timeout > 0 ? mp_time_ns_add(mp_time_ns(), timeout) : 0;
    mp_assert(!queue->in_process); // recursion not allowed
    queue->in_process = true;
    queue->in_process_thread_id = mp_thread_current_id();
    // Wake up thread which called mp_dispatch_lock().
    if (queue->lock_requests)
        mp_cond_broadcast(&queue->cond);
    while (1) {
        if (queue->lock_requests) {
            // Block due to something having called mp_dispatch_lock().
            mp_cond_wait(&queue->cond, &queue->lock);
        } else if (queue->head) {
            struct mp_dispatch_item *item = queue->head;
            queue->head = item->next;
            if (!queue->head)
                queue->tail = NULL;
            item->next = NULL;
            // Unlock, because we want to allow other threads to queue items
            // while the dispatch item is processed.
            // At the same time, we must prevent other threads from returning
            // from mp_dispatch_lock(), which is done by locked=true.
            mp_assert(!queue->locked);
            queue->locked = true;
            mp_mutex_unlock(&queue->lock);

            item->fn(item->fn_data);

            mp_mutex_lock(&queue->lock);
            mp_assert(queue->locked);
            queue->locked = false;
            // Wakeup mp_dispatch_run(), also mp_dispatch_lock().
            mp_cond_broadcast(&queue->cond);
            if (item->asynchronous) {
                talloc_free(item);
            } else {
                item->completed = true;
            }
        } else if (queue->wait > 0 && !queue->interrupted) {
            if (mp_cond_timedwait_until(&queue->cond, &queue->lock, queue->wait))
                queue->wait = 0;
        } else {
            break;
        }
    }
    mp_assert(!queue->locked);
    queue->in_process = false;
    queue->interrupted = false;
    mp_mutex_unlock(&queue->lock);
}

// If the queue is inside of mp_dispatch_queue_process(), make it return as
// soon as all work items have been run, without waiting for the timeout. This
// does not make it return early if it's blocked by a mp_dispatch_lock().
// If the queue is _not_ inside of mp_dispatch_queue_process(), make the next
// call of it use a timeout of 0 (this is useful behavior if you need to
// wakeup the main thread from another thread in a race free way).
void mp_dispatch_interrupt(struct mp_dispatch_queue *queue)
{
    mp_mutex_lock(&queue->lock);
    queue->interrupted = true;
    mp_cond_broadcast(&queue->cond);
    mp_mutex_unlock(&queue->lock);
}

// If a mp_dispatch_queue_process() call is in progress, then adjust the maximum
// time it blocks due to its timeout argument. Otherwise does nothing. (It
// makes sense to call this in code that uses both mp_dispatch_[un]lock() and
// a normal event loop.)
// Does not work correctly with queues that have mp_dispatch_set_wakeup_fn()
// called on them, because this implies you actually do waiting via
// mp_dispatch_queue_process(), while wakeup callbacks are used when you need
// to wait in external APIs.
void mp_dispatch_adjust_timeout(struct mp_dispatch_queue *queue, int64_t until)
{
    mp_mutex_lock(&queue->lock);
    if (queue->in_process && queue->wait > until) {
        queue->wait = until;
        mp_cond_broadcast(&queue->cond);
    }
    mp_mutex_unlock(&queue->lock);
}

// Grant exclusive access to the target thread's state. While this is active,
// no other thread can return from mp_dispatch_lock() (i.e. it behaves like
// a pthread mutex), and no other thread can get dispatch items completed.
// Other threads can still queue asynchronous dispatch items without waiting,
// and the mutex behavior applies to this function and dispatch callbacks only.
// The lock is non-recursive, and dispatch callback functions can be thought of
// already holding the dispatch lock.
void mp_dispatch_lock(struct mp_dispatch_queue *queue)
{
    mp_mutex_lock(&queue->lock);
    // Must not be called recursively from dispatched callbacks.
    if (queue->in_process)
        mp_assert(!mp_thread_id_equal(queue->in_process_thread_id, mp_thread_current_id()));
    // Must not be called recursively at all.
    if (queue->locked_explicit)
        mp_assert(!mp_thread_id_equal(queue->locked_explicit_thread_id, mp_thread_current_id()));
    queue->lock_requests += 1;
    // And now wait until the target thread gets "trapped" within the
    // mp_dispatch_queue_process() call, which will mean we get exclusive
    // access to the target's thread state.
    if (queue->onlock_fn)
        queue->onlock_fn(queue->onlock_ctx);
    while (!queue->in_process) {
        mp_mutex_unlock(&queue->lock);
        if (queue->wakeup_fn)
            queue->wakeup_fn(queue->wakeup_ctx);
        mp_mutex_lock(&queue->lock);
        if (queue->in_process)
            break;
        mp_cond_wait(&queue->cond, &queue->lock);
    }
    // Wait until we can get the lock.
    while (!queue->in_process || queue->locked)
        mp_cond_wait(&queue->cond, &queue->lock);
    // "Lock".
    mp_assert(queue->lock_requests);
    mp_assert(!queue->locked);
    mp_assert(!queue->locked_explicit);
    queue->locked = true;
    queue->locked_explicit = true;
    queue->locked_explicit_thread_id = mp_thread_current_id();
    mp_mutex_unlock(&queue->lock);
}

// Undo mp_dispatch_lock().
void mp_dispatch_unlock(struct mp_dispatch_queue *queue)
{
    mp_mutex_lock(&queue->lock);
    mp_assert(queue->locked);
    // Must be called after a mp_dispatch_lock(), from the same thread.
    mp_assert(queue->locked_explicit);
    mp_assert(mp_thread_id_equal(queue->locked_explicit_thread_id, mp_thread_current_id()));
    // "Unlock".
    queue->locked = false;
    queue->locked_explicit = false;
    queue->lock_requests -= 1;
    // Wakeup mp_dispatch_queue_process(), and maybe other mp_dispatch_lock()s.
    // (Would be nice to wake up only 1 other locker if lock_requests>0.)
    mp_cond_broadcast(&queue->cond);
    mp_mutex_unlock(&queue->lock);
}
