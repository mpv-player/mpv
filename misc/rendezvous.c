
#include "rendezvous.h"

#include "osdep/threads.h"

static mp_static_mutex lock = MP_STATIC_MUTEX_INITIALIZER;
static mp_cond wakeup = MP_STATIC_COND_INITIALIZER;

static struct waiter *waiters;

struct waiter {
    void *tag;
    struct waiter *next;
    intptr_t *value;
};

/* A barrier for 2 threads, which can exchange a value when they meet.
 * The first thread to call this function will block. As soon as two threads
 * are calling this function with the same tag value, they will unblock, and
 * on each thread the call returns the value parameter of the _other_ thread.
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
 * This is inspired by: https://man.cat-v.org/plan_9/2/rendezvous */
intptr_t mp_rendezvous(void *tag, intptr_t value)
{
    struct waiter wait = { .tag = tag, .value = &value };
    mp_mutex_lock(&lock);
    struct waiter **prev = &waiters;
    while (*prev) {
        if ((*prev)->tag == tag) {
            intptr_t tmp = *(*prev)->value;
            *(*prev)->value = value;
            value = tmp;
            (*prev)->value = NULL; // signals completion
            *prev = (*prev)->next; // unlink
            mp_cond_broadcast(&wakeup);
            goto done;
        }
        prev = &(*prev)->next;
    }
    *prev = &wait;
    while (wait.value)
        mp_cond_wait(&wakeup, &lock);
done:
    mp_mutex_unlock(&lock);
    return value;
}
