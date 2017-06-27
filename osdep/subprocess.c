/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>

#include "config.h"

#include "common/common.h"
#include "common/msg.h"
#include "common/msg_control.h"

#include "subprocess.h"

struct subprocess_args {
    struct mp_log *log;
    char **args;
};

static void *run_subprocess(void *ptr)
{
    struct subprocess_args *p = ptr;
    pthread_detach(pthread_self());

    mp_msg_flush_status_line(p->log);

    char *err = NULL;
    if (mp_subprocess(p->args, NULL, NULL, NULL, NULL, &err) < 0)
        mp_err(p->log, "Running subprocess failed: %s\n", err);

    talloc_free(p);
    return NULL;
}

void mp_devnull(void *ctx, char *data, size_t size)
{
}

void mp_subprocess_detached(struct mp_log *log, char **args)
{
    struct subprocess_args *p = talloc_zero(NULL, struct subprocess_args);
    p->log = mp_log_new(p, log, NULL);
    int num_args = 0;
    for (int n = 0; args[n]; n++)
        MP_TARRAY_APPEND(p, p->args, num_args, talloc_strdup(p, args[n]));
    MP_TARRAY_APPEND(p, p->args, num_args, NULL);
    pthread_t thread;
    if (pthread_create(&thread, NULL, run_subprocess, p))
        talloc_free(p);
}
