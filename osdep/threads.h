#ifndef MP_OSDEP_THREADS_H_
#define MP_OSDEP_THREADS_H_

#include "config.h"

#if HAVE_VDPAU
#define HAVE_MP_MUTEX_RECURSIVE 1
#else
#define HAVE_MP_MUTEX_RECURSIVE 0
#endif

enum mp_mutex_type {
    MP_MUTEX_NORMAL = 0,
#if HAVE_MP_MUTEX_RECURSIVE
    MP_MUTEX_RECURSIVE,
#endif
};

#define mp_mutex_init(mutex) \
    mp_mutex_init_type(mutex, MP_MUTEX_NORMAL)

#define mp_mutex_init_type(mutex, mtype) \
    mp_mutex_init_type_internal(mutex, mtype)

#if HAVE_WIN32_THREADS
#include "threads-win32.h"
#else
#include "threads-posix.h"
#endif

#endif
