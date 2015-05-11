#ifndef MP_OSDEP_THREADS_H_
#define MP_OSDEP_THREADS_H_

#include <pthread.h>
#include <inttypes.h>

// Helper to reduce boiler plate.
int mpthread_mutex_init_recursive(pthread_mutex_t *mutex);

// Set thread name (for debuggers).
void mpthread_set_name(const char *name);

#endif
