#ifndef MP_OSDEP_THREADS_H_
#define MP_OSDEP_THREADS_H_

#include <pthread.h>
#include <inttypes.h>

// Helper to reduce boiler plate.
int mpthread_mutex_init_recursive(pthread_mutex_t *mutex);

// Set thread name (for debuggers).
void mpthread_set_name(const char *name);

int mp_ptwrap_check(const char *file, int line, int res);
int mp_ptwrap_mutex_init(const char *file, int line, pthread_mutex_t *m,
                         const pthread_mutexattr_t *attr);

#ifdef MP_PTHREAD_DEBUG

// pthread debugging wrappers. Technically, this is undefined behavior, because
// you are not supposed to define any symbols that clash with reserved names.
// Other than that, they should be fine.

// Note: mpv normally never checks pthread error return values of certain
//       functions that  should never fail. It does so because these cases would
//       be undefined behavior anyway (such as double-frees etc.). However,
//       since there are no good pthread debugging tools, these wrappers are
//       provided for the sake of debugging. They crash on unexpected errors.
//
//       Technically, pthread_cond/mutex_init() can fail with ENOMEM. We don't
//       really respect this for normal/recursive mutex types, as due to the
//       existence of static initializers, no sane implementation could actually
//       require allocating memory.

#define MP_PTWRAP(fn, ...) \
    mp_ptwrap_check(__FILE__, __LINE__, (fn)(__VA_ARGS__))

// ISO C defines that all standard functions can be macros, except undef'ing
// them is allowed and must make the "real" definitions available. (Whatever.)
#undef pthread_cond_init
#undef pthread_cond_destroy
#undef pthread_cond_broadcast
#undef pthread_cond_signal
#undef pthread_cond_wait
#undef pthread_cond_timedwait
#undef pthread_detach
#undef pthread_join
#undef pthread_mutex_destroy
#undef pthread_mutex_lock
#undef pthread_mutex_unlock

#define pthread_cond_init(...)      MP_PTWRAP(pthread_cond_init, __VA_ARGS__)
#define pthread_cond_destroy(...)   MP_PTWRAP(pthread_cond_destroy, __VA_ARGS__)
#define pthread_cond_broadcast(...) MP_PTWRAP(pthread_cond_broadcast, __VA_ARGS__)
#define pthread_cond_signal(...)    MP_PTWRAP(pthread_cond_signal, __VA_ARGS__)
#define pthread_cond_wait(...)      MP_PTWRAP(pthread_cond_wait, __VA_ARGS__)
#define pthread_cond_timedwait(...) MP_PTWRAP(pthread_cond_timedwait, __VA_ARGS__)
#define pthread_detach(...)         MP_PTWRAP(pthread_detach, __VA_ARGS__)
#define pthread_join(...)           MP_PTWRAP(pthread_join, __VA_ARGS__)
#define pthread_mutex_destroy(...)  MP_PTWRAP(pthread_mutex_destroy, __VA_ARGS__)
#define pthread_mutex_lock(...)     MP_PTWRAP(pthread_mutex_lock, __VA_ARGS__)
#define pthread_mutex_unlock(...)   MP_PTWRAP(pthread_mutex_unlock, __VA_ARGS__)

#define pthread_mutex_init(...) \
    mp_ptwrap_mutex_init(__FILE__, __LINE__, __VA_ARGS__)

#endif

#endif
