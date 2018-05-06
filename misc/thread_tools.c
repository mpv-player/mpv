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
