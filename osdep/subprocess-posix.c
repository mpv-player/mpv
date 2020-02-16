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

#include "osdep/posix-spawn.h"
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "osdep/subprocess.h"

#include "common/common.h"
#include "misc/thread_tools.h"
#include "osdep/io.h"
#include "stream/stream.h"

extern char **environ;

#define SAFE_CLOSE(fd) do { if ((fd) >= 0) close((fd)); (fd) = -1; } while (0)

void mp_subprocess2(struct mp_subprocess_opts *opts,
                    struct mp_subprocess_result *res)
{
    posix_spawn_file_actions_t fa;
    bool fa_destroy = false;
    int status = -1;
    int comm_pipe[MP_SUBPROCESS_MAX_FDS][2];
    int devnull = -1;
    pid_t pid = -1;
    bool spawned = false;
    bool killed_by_us = false;
    int cancel_fd = -1;

    *res = (struct mp_subprocess_result){0};

    for (int n = 0; n < opts->num_fds; n++)
        comm_pipe[n][0] = comm_pipe[n][1] = -1;

    if (opts->cancel) {
        cancel_fd = mp_cancel_get_fd(opts->cancel);
        if (cancel_fd < 0)
            goto done;
    }

    for (int n = 0; n < opts->num_fds; n++) {
        if (opts->fds[n].on_read && mp_make_cloexec_pipe(comm_pipe[n]) < 0)
            goto done;
    }

    devnull = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (devnull < 0)
        goto done;

    if (posix_spawn_file_actions_init(&fa))
        goto done;
    fa_destroy = true;

    // redirect FDs
    for (int n = 0; n < opts->num_fds; n++) {
        int src_fd = devnull;
        if (comm_pipe[n][1] >= 0)
            src_fd = comm_pipe[n][1];
        if (opts->fds[n].src_fd >= 0)
            src_fd = opts->fds[n].src_fd;
        if (posix_spawn_file_actions_adddup2(&fa, src_fd, opts->fds[n].fd))
            goto done;
    }

    char **env = opts->env ? opts->env : environ;
    if (posix_spawnp(&pid, opts->exe, &fa, NULL, opts->args, env)) {
        pid = -1;
        goto done;
    }
    spawned = true;

    for (int n = 0; n < opts->num_fds; n++)
        SAFE_CLOSE(comm_pipe[n][1]);
    SAFE_CLOSE(devnull);

    while (1) {
        struct pollfd fds[MP_SUBPROCESS_MAX_FDS + 1];
        int map_fds[MP_SUBPROCESS_MAX_FDS + 1];
        int num_fds = 0;
        for (int n = 0; n < opts->num_fds; n++) {
            if (comm_pipe[n][0] >= 0) {
                map_fds[num_fds] = n;
                fds[num_fds++] = (struct pollfd){
                    .events = POLLIN,
                    .fd = comm_pipe[n][0],
                };
            }
        }
        if (!num_fds)
            break;
        if (cancel_fd >= 0) {
            map_fds[num_fds] = -1;
            fds[num_fds++] = (struct pollfd){.events = POLLIN, .fd = cancel_fd};
        }

        if (poll(fds, num_fds, -1) < 0 && errno != EINTR)
            break;

        for (int idx = 0; idx < num_fds; idx++) {
            if (fds[idx].revents) {
                int n = map_fds[idx];
                if (n < 0) {
                    // cancel_fd
                    kill(pid, SIGKILL);
                    killed_by_us = true;
                    break;
                } else {
                    char buf[4096];
                    ssize_t r = read(comm_pipe[n][0], buf, sizeof(buf));
                    if (r < 0 && errno == EINTR)
                        continue;
                    if (r > 0 && opts->fds[n].on_read)
                        opts->fds[n].on_read(opts->fds[n].on_read_ctx, buf, r);
                    if (r <= 0)
                        SAFE_CLOSE(comm_pipe[n][0]);
                }
            }
        }
    }

    // Note: it can happen that a child process closes the pipe, but does not
    //       terminate yet. In this case, we would have to run waitpid() in
    //       a separate thread and use pthread_cancel(), or use other weird
    //       and laborious tricks in order to react to mp_cancel.
    //       So this isn't handled yet.
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}

done:
    if (fa_destroy)
        posix_spawn_file_actions_destroy(&fa);
    for (int n = 0; n < opts->num_fds; n++) {
        SAFE_CLOSE(comm_pipe[n][0]);
        SAFE_CLOSE(comm_pipe[n][1]);
    }
    SAFE_CLOSE(devnull);

    if (!spawned || (WIFEXITED(status) && WEXITSTATUS(status) == 127)) {
        res->error = MP_SUBPROCESS_EINIT;
    } else if (WIFEXITED(status)) {
        res->exit_status = WEXITSTATUS(status);
    } else if (killed_by_us) {
        res->error = MP_SUBPROCESS_EKILLED_BY_US;
    } else {
        res->error = MP_SUBPROCESS_EGENERIC;
    }
}
