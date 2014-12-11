#include <pthread.h>
#include <stdio.h>
#include <windows.h>
#include <io.h>

#include <stdint.h>
#include <waio/waio.h>

#include "common/msg.h"
#include "osdep/io.h"
#include "input.h"

static void request_cancel(struct mp_input_src *src)
{
    HANDLE terminate = src->priv;

    MP_VERBOSE(src, "Exiting...\n");
    SetEvent(terminate);
}

static void uninit(struct mp_input_src *src)
{
    HANDLE terminate = src->priv;

    CloseHandle(terminate);
    MP_VERBOSE(src, "Exited.\n");
}

static void read_pipe_thread(struct mp_input_src *src, void *param)
{
    char *filename = talloc_strdup(src, param);

    struct waio_cx_interface *waio = NULL;
    int mode = O_RDONLY;
    int fd = -1;
    bool close_fd = true;
    if (strcmp(filename, "/dev/stdin") == 0) { // for symmetry with unix
        fd = STDIN_FILENO;
        close_fd = false;
    }
    if (fd < 0)
        fd = open(filename, mode);
    if (fd < 0) {
        MP_ERR(src, "Can't open %s.\n", filename);
        goto done;
    }

    // If we're reading from stdin, unset it. All I/O on synchronous handles is
    // serialized, so stupid DLLs that call GetFileType on stdin can hang the
    // process if they do it while we're reading from it. At least, the
    // VirtualBox OpenGL ICD is affected by this, but only on Windows XP.
    // GetFileType works differently in later versions of Windows. See:
    // https://support.microsoft.com/kb/2009703
    // http://blogs.msdn.com/b/oldnewthing/archive/2011/12/02/10243553.aspx
    if ((void*)_get_osfhandle(fd) == GetStdHandle(STD_INPUT_HANDLE))
        SetStdHandle(STD_INPUT_HANDLE, NULL);

    waio = waio_alloc((void *)_get_osfhandle(fd), 0, NULL, NULL);
    if (!waio) {
        MP_ERR(src, "Can't initialize win32 file reader.\n");
        goto done;
    }

    HANDLE terminate = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!terminate)
        goto done;

    src->priv = terminate;
    src->cancel = request_cancel;
    src->uninit = uninit;
    mp_input_src_init_done(src);

    char buffer[128];
    struct waio_aiocb cb = {
        .aio_buf = buffer,
        .aio_nbytes = sizeof(buffer),
        .hsignal = terminate,
    };
    while (1) {
        if (waio_read(waio, &cb)) {
            MP_ERR(src, "Read operation failed.\n");
            break;
        }
        if (waio_suspend(waio, (const struct waio_aiocb *[]){&cb}, 1, NULL))
            break;
        ssize_t r = waio_return(waio, &cb);
        if (r <= 0)
            break; // EOF or error
        mp_input_src_feed_cmd_text(src, buffer, r);
    }

done:
    waio_free(waio);
    if (close_fd)
        close(fd);
}

void mp_input_pipe_add(struct input_ctx *ictx, const char *filename)
{
    mp_input_add_thread_src(ictx, (void *)filename, read_pipe_thread);
}
