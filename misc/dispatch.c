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
    pthread_mutex_t lock;
    pthread_cond_t cond;
    void (*wakeup_fn)(void *wakeup_ctx);
    void *wakeup_ctx;
    // The target thread is blocked by mp_dispatch_queue_process(). Note that
    // mp_dispatch_lock() can set this from true to false to keep the thread
    // blocked (this stops if from processing other dispatch items, and from
    // other threads to return from mp_dispatch_lock(), making it an exclusive
    // lock).
    bool idling;
    // A mp_dispatch_lock() call is requesting an exclusive lock.
    bool lock_request;
    // Used to block out threads calling mp_dispatch_queue_process() while
    // they're externall locked via mp_dispatch_lock().
    // We could use a simple counter (increment it instead of adding a frame,
    // also increment it when locking), but with this we can perform some
    // minimal debug checks.
    struct lock_frame *frame;
};

struct lock_frame {
    struct lock_frame *prev;
    pthread_t thread;
    pthread_t locked_thread;
    bool locked;
};

struct mp_dispatch_item {
    mp_dispatch_fn fn;
    void *fn_data;
    bool asynchronous;
    bool completed;
    struct mp_dispatch_item *next;
};

static void queue_dtor(void *p)
{
    struct mp_dispatch_queue *queue = p;
    assert(!queue->head);
    assert(!queue->idling);
    assert(!queue->lock_request);
    assert(!queue->frame);
    pthread_cond_destroy(&queue->cond);
    pthread_mutex_destroy(&queue->lock);
}

// A dispatch queue lets other threads run callbacks in a target thread.
// The target thread is the thread which calls mp_dispatch_queue_process().
// Free the dispatch queue with talloc_free(). At the time of destruction,
// the queue must be empty. The easiest way to guarantee this is to
// terminate all potential senders, then call mp_dispatch_run() with a
// function that e.g. makes the target thread exit, then pthread_join() the
// target thread, and finally destroy the queue. Another way is calling
// mp_dispatch_queue_process() after terminating all potential senders, and
// then destroying the queue.
struct mp_dispatch_queue *mp_dispatch_create(void *ta_parent)
{
    struct mp_dispatch_queue *queue = talloc_ptrtype(ta_parent, queue);
    *queue = (struct mp_dispatch_queue){0};
    talloc_set_destructor(queue, queue_dtor);
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}

// Set a custom function that should be called to guarantee that the target
// thread wakes up. This is intended for use with code that needs to block
// on non-pthread primitives, such as e.g. select(). In the case of select(),
// the wakeup_fn could for example write a byte into a "wakeup" pipe in order
// to unblock the select(). The wakeup_fn is called from the dispatch queue
// when there are new dispatch items, and the target thread should then enter
// mp_dispatch_queue_process() as soon as possible. Note that wakeup_fn is
// called under no lock, so you might have to do synchronization yourself.
void mp_dispatch_set_wakeup_fn(struct mp_dispatch_queue *queue,
                               void (*wakeup_fn)(void *wakeup_ctx),
                               void *wakeup_ctx)
{
    queue->wakeup_fn = wakeup_fn;
    queue->wakeup_ctx = wakeup_ctx;
}

static void mp_dispatch_append(struct mp_dispatch_queue *queue,
                               struct mp_dispatch_item *item)
{
    pthread_mutex_lock(&queue->lock);
    if (queue->tail) {
        queue->tail->next = item;
    } else {
        queue->head = item;
    }
    queue->tail = item;
    // Wake up the main thread; note that other threads might wait on this
    // condition for reasons, so broadcast the condition.
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
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

    pthread_mutex_lock(&queue->lock);
    while (!item.completed)
        pthread_cond_wait(&queue->cond, &queue->lock);
    pthread_mutex_unlock(&queue->lock);
}

// Process any outstanding dispatch items in the queue. This also handles
// suspending or locking the this thread from another thread via
// mp_dispatch_lock().
// The timeout specifies the minimum wait time. The actual time spent in this
// function can be much higher if the suspending/locking functions are used, or
// if executing the dispatch items takes time. On the other hand, this function
// can return much earlier than the timeout due to sporadic wakeups.
// It is also guaranteed that if at least one queue item was processed, the
// function will return as soon as possible, ignoring the timeout. This
// simplifies users, such as re-checking conditions before waiting. (It will
// still process the remaining queue items, and wait for unsuspend.)
void mp_dispatch_queue_process(struct mp_dispatch_queue *queue, double timeout)
{
    int64_t wait = timeout > 0 ? mp_add_timeout(mp_time_us(), timeout) : 0;
    struct lock_frame frame = {
        .thread = pthread_self(),
    };

    pthread_mutex_lock(&queue->lock);
    frame.prev = queue->frame;
    queue->frame = &frame;
    // Logically, the queue is idling if the target thread is blocked in
    // mp_dispatch_queue_process() doing nothing, so it's not possible to call
    // it again. (Reentrant calls via callbacks temporarily reset the field.)
    assert(!queue->idling);
    queue->idling = true;
    // Wake up thread which called mp_dispatch_lock().
    if (queue->lock_request)
        pthread_cond_broadcast(&queue->cond);
    while (1) {
        if (queue->lock_request || queue->frame != &frame || frame.locked) {
            // Block due to something having called mp_dispatch_lock(). This
            // is either a lock "acquire" (lock_request=true), or a lock in
            // progress, with the possibility the thread which called
            // mp_dispatch_lock() is now calling mp_dispatch_queue_process()
            // (the latter means we must ignore any queue state changes,
            // until it has been unlocked again).
            pthread_cond_wait(&queue->cond, &queue->lock);
            if (queue->frame == &frame && !frame.locked)
                assert(queue->idling);
        } else if (queue->head) {
            struct mp_dispatch_item *item = queue->head;
            queue->head = item->next;
            if (!queue->head)
                queue->tail = NULL;
            item->next = NULL;
            // Unlock, because we want to allow other threads to queue items
            // while the dispatch item is processed.
            // At the same time, we must prevent other threads from returning
            // from mp_dispatch_lock(), which is done by idling=false.
            queue->idling = false;
            pthread_mutex_unlock(&queue->lock);

            item->fn(item->fn_data);

            pthread_mutex_lock(&queue->lock);
            assert(!queue->idling);
            queue->idling = true;
            // Wakeup mp_dispatch_run(), also mp_dispatch_lock().
            pthread_cond_broadcast(&queue->cond);
            if (item->asynchronous) {
                talloc_free(item);
            } else {
                item->completed = true;
            }
        } else if (wait > 0) {
            struct timespec ts = mp_time_us_to_timespec(wait);
            pthread_cond_timedwait(&queue->cond, &queue->lock, &ts);
        } else {
            break;
        }
        wait = 0;
    }
    queue->idling = false;
    assert(!frame.locked);
    assert(queue->frame == &frame);
    queue->frame = frame.prev;
    pthread_mutex_unlock(&queue->lock);
}

// Grant exclusive access to the target thread's state. While this is active,
// no other thread can return from mp_dispatch_lock() (i.e. it behaves like
// a pthread mutex), and no other thread can get dispatch items completed.
// Other threads can still queue asynchronous dispatch items without waiting,
// and the mutex behavior applies to this function only.
void mp_dispatch_lock(struct mp_dispatch_queue *queue)
{
    pthread_mutex_lock(&queue->lock);
    // First grab the queue lock. Something else could be holding the lock.
    while (queue->lock_request)
        pthread_cond_wait(&queue->cond, &queue->lock);
    queue->lock_request = true;
    // And now wait until the target thread gets "trapped" within the
    // mp_dispatch_queue_process() call, which will mean we get exclusive
    // access to the target's thread state.
    while (!queue->idling) {
        pthread_mutex_unlock(&queue->lock);
        if (queue->wakeup_fn)
            queue->wakeup_fn(queue->wakeup_ctx);
        pthread_mutex_lock(&queue->lock);
        if (queue->idling)
            break;
        pthread_cond_wait(&queue->cond, &queue->lock);
    }
    assert(queue->lock_request);
    assert(queue->frame); // must be set if idling
    assert(!queue->frame->locked); // no recursive locking on the same level
    // "Lock".
    queue->frame->locked = true;
    queue->frame->locked_thread = pthread_self();
    // Reset state for recursive mp_dispatch_queue_process() calls.
    queue->lock_request = false;
    queue->idling = false;
    pthread_mutex_unlock(&queue->lock);
}

// Undo mp_dispatch_lock().
void mp_dispatch_unlock(struct mp_dispatch_queue *queue)
{
    pthread_mutex_lock(&queue->lock);
    // Must be called atfer a mp_dispatch_lock().
    assert(queue->frame);
    assert(queue->frame->locked);
    assert(pthread_equal(queue->frame->locked_thread, pthread_self()));
    // "Unlock".
    queue->frame->locked = false;
    // This must have been set to false during locking (except temporarily
    // during recursive mp_dispatch_queue_process() calls).
    assert(!queue->idling);
    queue->idling = true;
    // Wakeup mp_dispatch_queue_process(), and maybe other mp_dispatch_lock()s.
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}
