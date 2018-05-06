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
