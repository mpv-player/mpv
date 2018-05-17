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

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#ifdef __MINGW32__
#include <windows.h>
#else
#include <poll.h>
#endif

#include "common/common.h"
#include "misc/linked_list.h"
#include "osdep/atomic.h"
#include "osdep/io.h"
#include "osdep/timer.h"

#include "thread_tools.h"

uintptr_t mp_waiter_wait(struct mp_waiter *waiter)
{
    pthread_mutex_lock(&waiter->lock);
    while (!waiter->done)
        pthread_cond_wait(&waiter->wakeup, &waiter->lock);
    pthread_mutex_unlock(&waiter->lock);

    uintptr_t ret = waiter->value;

    // We document that after mp_waiter_wait() the waiter object becomes
    // invalid. (It strictly returns only after mp_waiter_wakeup() has returned,
    // and the object is "single-shot".) So destroy it here.

    // Normally, we expect that the system uses futexes, in which case the
    // following functions will do nearly nothing. This is true for Windows
    // and Linux. But some lesser OSes still might allocate kernel objects
    // when initializing mutexes, so destroy them here.
    pthread_mutex_destroy(&waiter->lock);
    pthread_cond_destroy(&waiter->wakeup);

    memset(waiter, 0xCA, sizeof(*waiter)); // for debugging

    return ret;
}

void mp_waiter_wakeup(struct mp_waiter *waiter, uintptr_t value)
{
    pthread_mutex_lock(&waiter->lock);
    assert(!waiter->done);
    waiter->done = true;
    waiter->value = value;
    pthread_cond_signal(&waiter->wakeup);
    pthread_mutex_unlock(&waiter->lock);
}

bool mp_waiter_poll(struct mp_waiter *waiter)
{
    pthread_mutex_lock(&waiter->lock);
    bool r = waiter->done;
    pthread_mutex_unlock(&waiter->lock);
    return r;
}

struct mp_cancel {
    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // Semaphore state and "mirrors".
    atomic_bool triggered;
    void (*cb)(void *ctx);
    void *cb_ctx;
    int wakeup_pipe[2];
    void *win32_event; // actually HANDLE

    // Slave list. These are automatically notified as well.
    struct {
        struct mp_cancel *head, *tail;
    } slaves;

    // For slaves. Synchronization is managed by parent.lock!
    struct mp_cancel *parent;
    struct {
        struct mp_cancel *next, *prev;
    } siblings;
};

static void cancel_destroy(void *p)
{
    struct mp_cancel *c = p;

    assert(!c->slaves.head); // API user error

    // We can access c->parent without synchronization, because:
    //  - since c is being destroyed, nobody can explicitly remove it as slave
    //    at the same time
    //  - c->parent needs to stay valid as long as the slave exists
    if (c->parent)
        mp_cancel_remove_slave(c->parent, c);

    if (c->wakeup_pipe[0] >= 0) {
        close(c->wakeup_pipe[0]);
        close(c->wakeup_pipe[1]);
    }

#ifdef __MINGW32__
    if (c->win32_event)
        CloseHandle(c->win32_event);
#endif

    pthread_mutex_destroy(&c->lock);
    pthread_cond_destroy(&c->wakeup);
}

struct mp_cancel *mp_cancel_new(void *talloc_ctx)
{
    struct mp_cancel *c = talloc_ptrtype(talloc_ctx, c);
    talloc_set_destructor(c, cancel_destroy);
    *c = (struct mp_cancel){
        .triggered = ATOMIC_VAR_INIT(false),
        .wakeup_pipe = {-1, -1},
    };
    pthread_mutex_init(&c->lock, NULL);
    pthread_cond_init(&c->wakeup, NULL);
    return c;
}

static void trigger_locked(struct mp_cancel *c)
{
    atomic_store(&c->triggered, true);

    pthread_cond_broadcast(&c->wakeup); // condition bound to c->triggered

    if (c->cb)
        c->cb(c->cb_ctx);

    for (struct mp_cancel *sub = c->slaves.head; sub; sub = sub->siblings.next)
        mp_cancel_trigger(sub);

    if (c->wakeup_pipe[1] >= 0)
        (void)write(c->wakeup_pipe[1], &(char){0}, 1);

#ifdef __MINGW32__
    if (c->win32_event)
        SetEvent(c->win32_event);
#endif
}

void mp_cancel_trigger(struct mp_cancel *c)
{
    pthread_mutex_lock(&c->lock);
    trigger_locked(c);
    pthread_mutex_unlock(&c->lock);
}

void mp_cancel_reset(struct mp_cancel *c)
{
    pthread_mutex_lock(&c->lock);

    atomic_store(&c->triggered, false);

    if (c->wakeup_pipe[0] >= 0) {
        // Flush it fully.
        while (1) {
            int r = read(c->wakeup_pipe[0], &(char[256]){0}, 256);
            if (r <= 0 && !(r < 0 && errno == EINTR))
                break;
        }
    }

#ifdef __MINGW32__
    if (c->win32_event)
        ResetEvent(c->win32_event);
#endif

    pthread_mutex_unlock(&c->lock);
}

bool mp_cancel_test(struct mp_cancel *c)
{
    return c ? atomic_load_explicit(&c->triggered, memory_order_relaxed) : false;
}

bool mp_cancel_wait(struct mp_cancel *c, double timeout)
{
    struct timespec ts = mp_rel_time_to_timespec(timeout);
    pthread_mutex_lock(&c->lock);
    while (!mp_cancel_test(c)) {
        if (pthread_cond_timedwait(&c->wakeup, &c->lock, &ts))
            break;
    }
    pthread_mutex_unlock(&c->lock);

    return mp_cancel_test(c);
}

// If a new notification mechanism was added, and the mp_cancel state was
// already triggered, make sure the newly added mechanism is also triggered.
static void retrigger_locked(struct mp_cancel *c)
{
    if (mp_cancel_test(c))
        trigger_locked(c);
}

void mp_cancel_set_cb(struct mp_cancel *c, void (*cb)(void *ctx), void *ctx)
{
    pthread_mutex_lock(&c->lock);
    c->cb = cb;
    c->cb_ctx = ctx;
    retrigger_locked(c);
    pthread_mutex_unlock(&c->lock);
}

void mp_cancel_add_slave(struct mp_cancel *c, struct mp_cancel *slave)
{
    pthread_mutex_lock(&c->lock);
    assert(!slave->parent);
    slave->parent = c;
    LL_APPEND(siblings, &c->slaves, slave);
    retrigger_locked(c);
    pthread_mutex_unlock(&c->lock);
}

void mp_cancel_remove_slave(struct mp_cancel *c, struct mp_cancel *slave)
{
    pthread_mutex_lock(&c->lock);
    if (slave->parent) {
        assert(slave->parent == c);
        slave->parent = NULL;
        LL_REMOVE(siblings, &c->slaves, slave);
    }
    pthread_mutex_unlock(&c->lock);
}

int mp_cancel_get_fd(struct mp_cancel *c)
{
    pthread_mutex_lock(&c->lock);
    if (c->wakeup_pipe[0] < 0) {
        mp_make_wakeup_pipe(c->wakeup_pipe);
        retrigger_locked(c);
    }
    pthread_mutex_unlock(&c->lock);


    return c->wakeup_pipe[0];
}

#ifdef __MINGW32__
void *mp_cancel_get_event(struct mp_cancel *c)
{
    pthread_mutex_lock(&c->lock);
    if (!c->win32_event) {
        c->win32_event = CreateEventW(NULL, TRUE, FALSE, NULL);
        retrigger_locked(c);
    }
    pthread_mutex_unlock(&c->lock);

    return c->win32_event;
}
#endif
