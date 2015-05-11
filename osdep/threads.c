/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include "config.h"

#if HAVE_BSD_THREAD_NAME
#include <pthread_np.h>
#endif

#include "threads.h"
#include "timer.h"

int mpthread_mutex_init_recursive(pthread_mutex_t *mutex)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int r = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return r;
}

void mpthread_set_name(const char *name)
{
    char tname[80];
    snprintf(tname, sizeof(tname), "mpv/%s", name);
#if HAVE_GLIBC_THREAD_NAME
    if (pthread_setname_np(pthread_self(), tname) == ERANGE) {
        tname[15] = '\0'; // glibc-checked kernel limit
        pthread_setname_np(pthread_self(), tname);
    }
#elif HAVE_BSD_THREAD_NAME
    pthread_set_name_np(pthread_self(), tname);
#elif HAVE_NETBSD_THREAD_NAME
    pthread_setname_np(pthread_self(), "%s", (void *)tname);
#elif HAVE_OSX_THREAD_NAME
    pthread_setname_np(tname);
#endif
}
