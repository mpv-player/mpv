#ifndef MP_OSDEP_THREADS_H_
#define MP_OSDEP_THREADS_H_

#include <pthread.h>

int mpthread_cond_timed_wait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                             double timeout);

#endif
