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

#include <windows.h>
#include <io.h>

#include "common/msg.h"
#include "osdep/atomic.h"
#include "osdep/io.h"
#include "input.h"

struct priv {
    atomic_bool cancel_requested;
    int fd;
    bool close_fd;
    HANDLE file;
    HANDLE thread;
};

static void request_cancel(struct mp_input_src *src)
{
    struct priv *p = src->priv;

    MP_VERBOSE(src, "Exiting...\n");
    atomic_store(&p->cancel_requested, true);

    // The thread might not be peforming I/O at the exact moment when
    // CancelIoEx is called, so call it in a loop until it succeeds or the
    // thread exits
    do {
        if (CancelIoEx(p->file, NULL))
            break;
    } while (WaitForSingleObject(p->thread, 1) != WAIT_OBJECT_0);
}

static void uninit(struct mp_input_src *src)
{
    struct priv *p = src->priv;

    CloseHandle(p->thread);
    if (p->close_fd)
        close(p->fd);

    MP_VERBOSE(src, "Exited.\n");
}

static void read_pipe_thread(struct mp_input_src *src, void *param)
{
    char *filename = talloc_strdup(src, param);
    struct priv *p = talloc_zero(src, struct priv);

    p->fd = -1;
    p->close_fd = true;
    if (strcmp(filename, "/dev/stdin") == 0) { // for symmetry with unix
        p->fd = STDIN_FILENO;
        p->close_fd = false;
    }
    if (p->fd < 0)
        p->fd = open(filename, O_RDONLY);
    if (p->fd < 0) {
        MP_ERR(src, "Can't open %s.\n", filename);
        return;
    }

    p->file = (HANDLE)_get_osfhandle(p->fd);
    if (!p->file || p->file == INVALID_HANDLE_VALUE) {
        MP_ERR(src, "Can't open %s.\n", filename);
        return;
    }

    atomic_store(&p->cancel_requested, false);
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
        GetCurrentProcess(), &p->thread, SYNCHRONIZE, FALSE, 0))
        return;

    src->priv = p;
    src->cancel = request_cancel;
    src->uninit = uninit;
    mp_input_src_init_done(src);

    char buffer[4096];
    while (!atomic_load(&p->cancel_requested)) {
        DWORD r;
        if (!ReadFile(p->file, buffer, 4096, &r, NULL)) {
            if (GetLastError() != ERROR_OPERATION_ABORTED)
                MP_ERR(src, "Read operation failed.\n");
            break;
        }
        mp_input_src_feed_cmd_text(src, buffer, r);
    }
}

void mp_input_pipe_add(struct input_ctx *ictx, const char *filename)
{
    mp_input_add_thread_src(ictx, (void *)filename, read_pipe_thread);
}
