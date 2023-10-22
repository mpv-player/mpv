#ifndef MP_OSDEP_THREADS_H_
#define MP_OSDEP_THREADS_H_

#include "config.h"

enum mp_mutex_type {
    MP_MUTEX_NORMAL = 0,
    MP_MUTEX_RECURSIVE,
};

#define mp_mutex_init(mutex) \
    mp_mutex_init_type(mutex, MP_MUTEX_NORMAL)

#define mp_mutex_init_type(mutex, mtype) \
    assert(!mp_mutex_init_type_internal(mutex, mtype))

#if HAVE_WIN32_THREADS
#include "threads-win32.h"
#else
#include "threads-posix.h"
#endif

#endif
