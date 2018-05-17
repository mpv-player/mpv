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
#include "osdep/atomic.h"
#include "osdep/io.h"

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

#ifndef __MINGW32__
struct mp_cancel {
    atomic_bool triggered;
    int wakeup_pipe[2];
};

static void cancel_destroy(void *p)
{
    struct mp_cancel *c = p;
    if (c->wakeup_pipe[0] >= 0) {
        close(c->wakeup_pipe[0]);
        close(c->wakeup_pipe[1]);
    }
}

struct mp_cancel *mp_cancel_new(void *talloc_ctx)
{
    struct mp_cancel *c = talloc_ptrtype(talloc_ctx, c);
    talloc_set_destructor(c, cancel_destroy);
    *c = (struct mp_cancel){.triggered = ATOMIC_VAR_INIT(false)};
    mp_make_wakeup_pipe(c->wakeup_pipe);
    return c;
}

void mp_cancel_trigger(struct mp_cancel *c)
{
    atomic_store(&c->triggered, true);
    (void)write(c->wakeup_pipe[1], &(char){0}, 1);
}

void mp_cancel_reset(struct mp_cancel *c)
{
    atomic_store(&c->triggered, false);
    // Flush it fully.
    while (1) {
        int r = read(c->wakeup_pipe[0], &(char[256]){0}, 256);
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            break;
    }
}

bool mp_cancel_test(struct mp_cancel *c)
{
    return c ? atomic_load_explicit(&c->triggered, memory_order_relaxed) : false;
}

bool mp_cancel_wait(struct mp_cancel *c, double timeout)
{
    struct pollfd fd = { .fd = c->wakeup_pipe[0], .events = POLLIN };
    poll(&fd, 1, timeout * 1000);
    return fd.revents & POLLIN;
}

int mp_cancel_get_fd(struct mp_cancel *c)
{
    return c->wakeup_pipe[0];
}

#else

struct mp_cancel {
    atomic_bool triggered;
    HANDLE event;
};

static void cancel_destroy(void *p)
{
    struct mp_cancel *c = p;
    CloseHandle(c->event);
}

struct mp_cancel *mp_cancel_new(void *talloc_ctx)
{
    struct mp_cancel *c = talloc_ptrtype(talloc_ctx, c);
    talloc_set_destructor(c, cancel_destroy);
    *c = (struct mp_cancel){.triggered = ATOMIC_VAR_INIT(false)};
    c->event = CreateEventW(NULL, TRUE, FALSE, NULL);
    return c;
}

void mp_cancel_trigger(struct mp_cancel *c)
{
    atomic_store(&c->triggered, true);
    SetEvent(c->event);
}

void mp_cancel_reset(struct mp_cancel *c)
{
    atomic_store(&c->triggered, false);
    ResetEvent(c->event);
}

bool mp_cancel_test(struct mp_cancel *c)
{
    return c ? atomic_load_explicit(&c->triggered, memory_order_relaxed) : false;
}

bool mp_cancel_wait(struct mp_cancel *c, double timeout)
{
    return WaitForSingleObject(c->event, timeout < 0 ? INFINITE : timeout * 1000)
            == WAIT_OBJECT_0;
}

void *mp_cancel_get_event(struct mp_cancel *c)
{
    return c->event;
}

int mp_cancel_get_fd(struct mp_cancel *c)
{
    return -1;
}

#endif
