#ifndef MP_OSDEP_THREADS_H_
#define MP_OSDEP_THREADS_H_

#include <pthread.h>
#include <inttypes.h>

// Call pthread_cond_timedwait() with an absolute timeout using the same
// time source/unit as mp_time_us() (microseconds).
int mpthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                            int64_t abstime);

// Wait by a relative amount of time in seconds.
int mpthread_cond_timedwait_rel(pthread_cond_t *cond, pthread_mutex_t *mutex,
                                double seconds);

// Helper to reduce boiler plate.
int mpthread_mutex_init_recursive(pthread_mutex_t *mutex);

#endif
