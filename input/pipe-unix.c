#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <poll.h>

#include "common/msg.h"
#include "osdep/io.h"
#include "input.h"
#include "cmd_parse.h"

static void read_pipe_thread(struct mp_input_src *src, void *param)
{
    void *tmp = talloc_new(NULL);
    char *filename = talloc_strdup(tmp, param); // param deallocates after init
    int wakeup_fd = mp_input_src_get_wakeup_fd(src);
    int fd = -1;

    struct mp_log *log = src->log;

    int mode = O_RDONLY;
    // Use RDWR for FIFOs to ensure they stay open over multiple accesses.
    struct stat st;
    if (stat(filename, &st) == 0 && S_ISFIFO(st.st_mode))
        mode = O_RDWR;
    fd = open(filename, mode);
    if (fd < 0) {
        mp_err(log, "Can't open %s.\n", filename);
        goto done;
    }

    mp_input_src_init_done(src);

    while (1) {
        struct pollfd fds[2] = {
            { .fd = fd, .events = POLLIN },
            { .fd = wakeup_fd, .events = POLLIN },
        };
        poll(fds, 2, -1);
        if (!(fds[0].revents & POLLIN))
            break;
        char buffer[128];
        int r = read(fd, buffer, sizeof(buffer));
        if (r <= 0)
            break;
        mp_input_src_feed_cmd_text(src, buffer, r);
    }

done:
    close(fd);
    talloc_free(tmp);
}

void mp_input_pipe_add(struct input_ctx *ictx, const char *filename)
{
    mp_input_add_thread_src(ictx, (void *)filename, read_pipe_thread);
}
