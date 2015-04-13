/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>

#include "rendezvous.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wakeup = PTHREAD_COND_INITIALIZER;

static struct waiter *waiters;

struct waiter {
    void *tag;
    struct waiter *next;
    intptr_t *value;
};

/* A barrier for 2 threads, which can exchange a value when they meet.
 * The first thread to call this function will block. As soon as two threads
 * are calling this function with the same tag value, they will unblock, and
 * on each thread the call return the value parameter of the _other_ thread.
 *
 * tag is an arbitrary value, but it must be an unique pointer. If there are
 * more than 2 threads using the same tag, things won't work. Typically, it
 * will have to point to a memory allocation or to the stack, while pointing
 * it to static data is always a bug.
 *
 * This shouldn't be used for performance critical code (uses a linked list
 * of _all_ waiters in the process, and temporarily wakes up _all_ waiters on
 * each second call).
 *
 * This is inspired by: http://9atom.org/magic/man2html/2/rendezvous */
intptr_t mp_rendezvous(void *tag, intptr_t value)
{
    struct waiter wait = { .tag = tag, .value = &value };
    pthread_mutex_lock(&lock);
    struct waiter **prev = &waiters;
    while (*prev) {
        if ((*prev)->tag == tag) {
            intptr_t tmp = *(*prev)->value;
            *(*prev)->value = value;
            value = tmp;
            (*prev)->value = NULL; // signals completion
            *prev = (*prev)->next; // unlink
            pthread_cond_broadcast(&wakeup);
            goto done;
        }
        prev = &(*prev)->next;
    }
    *prev = &wait;
    while (wait.value)
        pthread_cond_wait(&wakeup, &lock);
done:
    pthread_mutex_unlock(&lock);
    return value;
}
