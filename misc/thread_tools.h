#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// This is basically a single-shot semaphore, intended as light-weight solution
// for just making a thread wait for another thread.
struct mp_waiter {
    // All fields are considered private. Use MP_WAITER_INITIALIZER to init.
    pthread_mutex_t lock;
    pthread_cond_t wakeup;
    bool done;
    uintptr_t value;
};

// Initialize a mp_waiter object for use with mp_waiter_*().
#define MP_WAITER_INITIALIZER { \
    .lock = PTHREAD_MUTEX_INITIALIZER, \
    .wakeup = PTHREAD_COND_INITIALIZER, \
    }

// Block until some other thread calls mp_waiter_wakeup(). The function returns
// the value argument of that wakeup call. After this, the waiter object must
// not be used anymore. Although you can reinit it with MP_WAITER_INITIALIZER
// (then you must make sure nothing calls mp_waiter_wakeup() before this).
uintptr_t mp_waiter_wait(struct mp_waiter *waiter);

// Unblock the thread waiting with mp_waiter_wait(), and make it return the
// provided value. If the other thread did not enter that call yet, it will
// return immediately once it does (mp_waiter_wakeup() always returns
// immediately). Calling this more than once is not allowed.
void mp_waiter_wakeup(struct mp_waiter *waiter, uintptr_t value);

// Query whether the waiter was woken up. If true, mp_waiter_wait() will return
// immediately. This is useful if you want to use another way to block and
// wakeup (in parallel to mp_waiter).
// You still need to call mp_waiter_wait() to free resources.
bool mp_waiter_poll(struct mp_waiter *waiter);

// Basically a binary semaphore that supports signaling the semaphore value to
// a bunch of other complicated mechanisms (such as wakeup pipes). It was made
// for aborting I/O and thus has according naming.
struct mp_cancel;

struct mp_cancel *mp_cancel_new(void *talloc_ctx);

// Request abort.
void mp_cancel_trigger(struct mp_cancel *c);

// Return whether the caller should abort.
// For convenience, c==NULL is allowed.
bool mp_cancel_test(struct mp_cancel *c);

// Wait until the even is signaled. If the timeout (in seconds) expires, return
// false. timeout==0 polls, timeout<0 waits forever.
bool mp_cancel_wait(struct mp_cancel *c, double timeout);

// Restore original state. (Allows reusing a mp_cancel.)
void mp_cancel_reset(struct mp_cancel *c);

// Add a callback to invoke when mp_cancel gets triggered. If it's already
// triggered, call it from mp_cancel_add_cb() directly. May be called multiple
// times even if the trigger state changes; not called if it resets. In all
// cases, this may be called with internal locks held (either in mp_cancel, or
// other locks held by whoever calls mp_cancel_trigger()).
// There is only one callback. Create a slave mp_cancel to get a private one.
void mp_cancel_set_cb(struct mp_cancel *c, void (*cb)(void *ctx), void *ctx);

// If parent gets triggered, automatically trigger slave. There is only 1
// parent; setting NULL clears the parent. Freeing slave also automatically
// ends the parent link, but the parent mp_cancel must remain valid until the
// slave is manually removed or destroyed. Destroying a mp_cancel that still
// has slaves is an error.
void mp_cancel_set_parent(struct mp_cancel *slave, struct mp_cancel *parent);

// win32 "Event" HANDLE that indicates the current mp_cancel state.
void *mp_cancel_get_event(struct mp_cancel *c);

// The FD becomes readable if mp_cancel_test() would return true.
// Don't actually read from it, just use it for poll().
int mp_cancel_get_fd(struct mp_cancel *c);
