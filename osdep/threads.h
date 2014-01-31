#ifndef MP_OSDEP_THREADS_H_
#define MP_OSDEP_THREADS_H_

#include <pthread.h>

struct timespec mpthread_get_deadline(double timeout);

int mpthread_cond_timed_wait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                             double timeout);

int mpthread_mutex_init_recursive(pthread_mutex_t *mutex);

#endif
