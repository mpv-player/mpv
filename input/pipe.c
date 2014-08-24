#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef __MINGW32__
#include <poll.h>
#endif

#include "common/msg.h"
#include "bstr/bstr.h"
#include "osdep/io.h"
#include "input.h"
#include "cmd_parse.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct priv {
    struct mp_log *log;
    char *filename;
    struct mp_input_src *src;
    int wakeup_pipe[2];
};

static void *reader_thread(void *ctx)
{
    struct priv *p = ctx;
    pthread_detach(pthread_self());

    int mode = O_RDONLY;
#ifndef __MINGW32__
    // Use RDWR for FIFOs to ensure they stay open over multiple accesses.
    // Note that on Windows due to how the API works, using RDONLY should
    // be ok.
    struct stat st;
    if (stat(p->filename, &st) == 0 && S_ISFIFO(st.st_mode))
        mode = O_RDWR;
#endif
    int fd = -1;
    bool close_fd = true;
    if (strcmp(p->filename, "/dev/stdin") == 0) { // mainly for win32
        fd = 1;
        close_fd = false;
    }
    if (fd < 0)
        fd = open(p->filename, mode);
    if (fd < 0) {
        MP_ERR(p, "Can't open %s.\n", p->filename);
        goto done;
    }

    while (1) {
#ifndef __MINGW32__
        struct pollfd fds[2] = {
            { .fd = fd, .events = POLLIN },
            { .fd = p->wakeup_pipe[0], .events = POLLIN },
        };
        poll(fds, 2, -1);
        if (!(fds[0].revents & POLLIN))
            break;
#endif
        char buffer[128];
        int r = read(fd, buffer, sizeof(buffer));
        if (r <= 0)
            break;

        pthread_mutex_lock(&lock);
        if (!p->src) {
            pthread_mutex_unlock(&lock);
            break;
        }
        mp_input_src_feed_cmd_text(p->src, buffer, r);
        pthread_mutex_unlock(&lock);
    }

    if (close_fd)
        close(fd);

done:
    pthread_mutex_lock(&lock);
    if (p->src)
        p->src->priv = NULL;
    pthread_mutex_unlock(&lock);
    close(p->wakeup_pipe[0]);
    close(p->wakeup_pipe[1]);
    talloc_free(p);
    return NULL;
}

static void close_pipe(struct mp_input_src *src)
{
    pthread_mutex_lock(&lock);
    struct priv *p = src->priv;
    // Windows pipe have a severe problem: they can't be made non-blocking (not
    // after creation), and you can't wait on them. The only things that work
    // are cancellation (Vista+, broken in wine) or forceful thread termination.
    // So don't bother with "correct" termination, and just abandon the reader
    // thread.
    // On Unix, we interrupt it using the wakeup pipe.
    if (p) {
#ifndef __MINGW32__
        write(p->wakeup_pipe[1], &(char){0}, 1);
#endif
        p->src = NULL;
    }
    pthread_mutex_unlock(&lock);
}

void mp_input_add_pipe(struct input_ctx *ictx, const char *filename)
{
    struct mp_input_src *src = mp_input_add_src(ictx);
    if (!src)
        return;

    struct priv *p = talloc_zero(NULL, struct priv);
    src->priv = p;
    p->filename = talloc_strdup(p, filename);
    p->src = src;
    p->log = mp_log_new(p, src->log, NULL);
    mp_make_wakeup_pipe(p->wakeup_pipe);

    pthread_t thread;
    if (pthread_create(&thread, NULL, reader_thread, p)) {
        close(p->wakeup_pipe[0]);
        close(p->wakeup_pipe[1]);
        talloc_free(p);
        mp_input_src_kill(src);
    } else {
        src->close = close_pipe;
    }
}
