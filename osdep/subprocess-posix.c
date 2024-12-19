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

#if HAVE_CLONE
#include <sched.h>
#endif

#ifdef SIGRTMAX
#define SIGNAL_MAX SIGRTMAX
#else
#define SIGNAL_MAX 32
#endif

#define SAFE_CLOSE(fd) do { if ((fd) >= 0) close((fd)); (fd) = -1; } while (0)

// Async-signal-safe execvpe(). POSIX does not list it as async-signal-safe
// (POSIX is such a joke), so do it manually. While in theory the searching is
// apparently implementation dependent and not exposed (because POSIX is a
// joke?), the  expected rules are still relatively simple.
// Doesn't set errno correctly.
// Somewhat inspired by musl's src/process/execvp.c.
static int as_execvpe(const char *path, const char *file, char *const argv[],
                      char *const envp[])
{
    if (strchr(file, '/') || !file[0])
        return execve(file, argv, envp);

    size_t flen = strlen(file);
    while (path && path[0]) {
        size_t plen = strcspn(path, ":");
        // Ignore paths that are too long.
        char fn[PATH_MAX];
        if (plen + 1 + flen + 1 < sizeof(fn)) {
            memcpy(fn, path, plen);
            fn[plen] = '/';
            memcpy(fn + plen + 1, file, flen + 1);
            execve(fn, argv, envp);
            if (errno != EACCES && errno != ENOENT && errno != ENOTDIR)
                break;
        }
        path += plen + (path[plen] == ':' ? 1 : 0);
    }
    return -1;
}

#if !HAVE_RFORK
// In the child process, resets the signal mask to defaults. Also clears any
// signal handlers first so nothing funny happens.
static void reset_signals_child(void)
{
    struct sigaction sa = { 0 };
    sigset_t sigmask;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sigmask);

    for (int nr = 1; nr < SIGNAL_MAX; nr++)
        sigaction(nr, &sa, NULL);
    sigprocmask(SIG_SETMASK, &sigmask, NULL);
}
#endif

struct child_args {
    const char *path;
    struct mp_subprocess_opts *opts;
    int *src_fds;
    void *child_stack;
    int pipe_end;
    bool detach;
};

static pid_t spawn_process_inner(const char *path, struct mp_subprocess_opts *opts,
                                 int src_fds[], bool detach, void *stacks[]);

static int child_main(void* args)
{
    struct child_args *child_args = args;
    const char *path = child_args->path;
    struct mp_subprocess_opts *opts = child_args->opts;
    int *src_fds = child_args->src_fds;
    void *child_stack = child_args->child_stack;
    int *pipe_end = &child_args->pipe_end;
    bool detach = child_args->detach;

    if (detach) {
        setsid();
        if (!spawn_process_inner(path, opts, src_fds, false, &child_stack))
            goto child_failed;
        return 0;
    }

    // RFSPAWN has reset all signal actions in the child to default
#if !HAVE_RFORK
    reset_signals_child();
#endif

    for (int n = 0; n < opts->num_fds; n++) {
        if (src_fds[n] == opts->fds[n].fd) {
            int flags = fcntl(opts->fds[n].fd, F_GETFD);
            if (flags == -1)
                goto child_failed;
            flags &= ~(unsigned)FD_CLOEXEC;
            if (fcntl(opts->fds[n].fd, F_SETFD, flags) == -1)
                goto child_failed;
        } else if (dup2(src_fds[n], opts->fds[n].fd) < 0) {
            goto child_failed;
        }
    }

    as_execvpe(path, opts->exe, opts->args, opts->env ? opts->env : environ);

child_failed:
#if HAVE_CLONE || HAVE_RFORK
    *pipe_end = 1;
#else
    (void)write(*pipe_end, &(char){1}, 1); // shouldn't be able to fail
#endif
    return 1;
}

static pid_t spawn_process_inner(const char *path, struct mp_subprocess_opts *opts,
                                 int src_fds[], bool detach, void *stacks[])
{
    pid_t fres = 0;
    int r;

    struct child_args child_args = {
        .path = path,
        .opts = opts,
        .src_fds = src_fds,
        .child_stack = stacks[1],
        .pipe_end = 0,
        .detach = detach,
    };

#if HAVE_RFORK
    fres = rfork_thread(RFSPAWN, stacks[0], child_main, &child_args);
#elif HAVE_CLONE
    fres = clone(child_main, stacks[0], CLONE_VM | CLONE_VFORK | SIGCHLD, &child_args);
#else
    int p[2] = {-1, -1};
    // We setup a communication pipe to signal failure. Since the child calls
    // exec() and becomes the calling process, we don't know if or when the
    // child process successfully ran exec() just from the PID.
    // Use a CLOEXEC pipe to detect whether exec() was used. Obviously it will
    // be closed if exec() succeeds, and an error is written if not.
    // There are also some things further below in the code that need CLOEXEC.
    if (mp_make_cloexec_pipe(p) < 0)
        goto done;
    // Check whether CLOEXEC is really set. Important for correct operation.
    int p_flags = fcntl(p[0], F_GETFD);
    if (p_flags == -1 || !FD_CLOEXEC || !(p_flags & FD_CLOEXEC))
        goto done; // require CLOEXEC; unknown if fallback would be worth it
    child_args.pipe_end = p[1];

    fres = fork();
#endif

    if (fres < 0) {
        fres = 0;
        goto done;
    }

#if HAVE_CLONE || HAVE_RFORK
    r = child_args.pipe_end;
#else
    if (fres == 0) {
        _exit(child_main(&child_args));
    }

    SAFE_CLOSE(p[1]);

    do {
        r = read(p[0], &(char){0}, 1);
    } while (r < 0 && errno == EINTR);
#endif

    // If exec()ing child failed, collect it immediately.
    if (detach || r != 0) {
        int child_status = 0;
        while (waitpid(fres, &child_status, 0) < 0 && errno == EINTR) {}
        if (r != 0 || !WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0)
            fres = 0;
    }

done:
#if !HAVE_CLONE && !HAVE_RFORK
    SAFE_CLOSE(p[0]);
    SAFE_CLOSE(p[1]);
#endif

    return fres;
}

// Returns 0 on any error, valid PID on success.
// This function must be async-signal-safe, as it may be called from a fork().
static pid_t spawn_process(const char *path, struct mp_subprocess_opts *opts,
                           int src_fds[])
{
    bool detach = opts->detach;
    void *stacks[2];

#if HAVE_CLONE || HAVE_RFORK
    // pre-allocate stacks so we can be async-signal-safe in spawn_process_inner()
    const size_t stack_size = 0x8000;
    void *ctx = talloc_new(NULL);
    // stack should be aligned to 16 bytes, which is guaranteed by malloc
    stacks[0] = (int8_t*)talloc_size(ctx, stack_size) + stack_size;
    if (detach) stacks[1] = (int8_t*)talloc_size(ctx, stack_size) + stack_size;
#endif

#if !HAVE_RFORK
    sigset_t sigmask, oldmask;

    sigfillset(&sigmask);
    pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask);
#endif

    pid_t fres = spawn_process_inner(path, opts, src_fds, detach, stacks);

#if !HAVE_RFORK
    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
#endif

#if HAVE_CLONE || HAVE_RFORK
    talloc_free(ctx);
#endif

    return fres;
}

void mp_subprocess2(struct mp_subprocess_opts *opts,
                    struct mp_subprocess_result *res)
{
    int status = -1;
    int comm_pipe[MP_SUBPROCESS_MAX_FDS][2];
    int src_fds[MP_SUBPROCESS_MAX_FDS];
    int devnull = -1;
    pid_t pid = 0;
    bool spawned = false;
    bool killed_by_us = false;
    int cancel_fd = -1;
    char *path = getenv("PATH");
    if (!path)
        path = ""; // failure, who cares

    *res = (struct mp_subprocess_result){0};

    for (int n = 0; n < opts->num_fds; n++)
        comm_pipe[n][0] = comm_pipe[n][1] = -1;

    if (opts->cancel) {
        cancel_fd = mp_cancel_get_fd(opts->cancel);
        if (cancel_fd < 0)
            goto done;
    }

    for (int n = 0; n < opts->num_fds; n++) {
        assert(!(opts->fds[n].on_read && opts->fds[n].on_write));

        if (opts->fds[n].on_read && mp_make_cloexec_pipe(comm_pipe[n]) < 0)
            goto done;

        if (opts->fds[n].on_write || opts->fds[n].write_buf) {
            assert(opts->fds[n].on_write && opts->fds[n].write_buf);
            if (mp_make_cloexec_pipe(comm_pipe[n]) < 0)
                goto done;
            MPSWAP(int, comm_pipe[n][0], comm_pipe[n][1]);

            struct sigaction sa = {.sa_handler = SIG_IGN, .sa_flags = SA_RESTART};
            sigfillset(&sa.sa_mask);
            sigaction(SIGPIPE, &sa, NULL);
        }
    }

    devnull = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (devnull < 0)
        goto done;

    // redirect FDs
    for (int n = 0; n < opts->num_fds; n++) {
        int src_fd = devnull;
        if (comm_pipe[n][1] >= 0)
            src_fd = comm_pipe[n][1];
        if (opts->fds[n].src_fd >= 0)
            src_fd = opts->fds[n].src_fd;
        src_fds[n] = src_fd;
    }

    pid = spawn_process(path, opts, src_fds);
    if (!pid)
        goto done;
    if (opts->detach)
        pid = 0;

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
                    .events = opts->fds[n].on_read ? POLLIN : POLLOUT,
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
                    if (pid)
                        kill(pid, SIGKILL);
                    killed_by_us = true;
                    goto break_poll;
                }
                struct mp_subprocess_fd *fd = &opts->fds[n];
                if (fd->on_read) {
                    char buf[4096];
                    ssize_t r = read(comm_pipe[n][0], buf, sizeof(buf));
                    if (r < 0 && errno == EINTR)
                        continue;
                    fd->on_read(fd->on_read_ctx, buf, MPMAX(r, 0));
                    if (r <= 0)
                        SAFE_CLOSE(comm_pipe[n][0]);
                } else if (fd->on_write) {
                    if (!fd->write_buf->len) {
                        fd->on_write(fd->on_write_ctx);
                        if (!fd->write_buf->len) {
                            SAFE_CLOSE(comm_pipe[n][0]);
                            continue;
                        }
                    }
                    ssize_t r = write(comm_pipe[n][0], fd->write_buf->start,
                                      fd->write_buf->len);
                    if (r < 0 && errno == EINTR)
                        continue;
                    if (r < 0) {
                        // Let's not signal an error for now - caller can check
                        // whether all buffer was written.
                        SAFE_CLOSE(comm_pipe[n][0]);
                        continue;
                    }
                    *fd->write_buf = bstr_cut(*fd->write_buf, r);
                }
            }
        }
    }

break_poll:

    // Note: it can happen that a child process closes the pipe, but does not
    //       terminate yet. In this case, we would have to run waitpid() in
    //       a separate thread and use pthread_cancel(), or use other weird
    //       and laborious tricks in order to react to mp_cancel.
    //       So this isn't handled yet.
    if (pid)
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}

done:
    for (int n = 0; n < opts->num_fds; n++) {
        SAFE_CLOSE(comm_pipe[n][0]);
        SAFE_CLOSE(comm_pipe[n][1]);
    }
    SAFE_CLOSE(devnull);

    if (!spawned || (pid && WIFEXITED(status) && WEXITSTATUS(status) == 127)) {
        res->error = MP_SUBPROCESS_EINIT;
    } else if (pid && WIFEXITED(status)) {
        res->exit_status = WEXITSTATUS(status);
    } else if (spawned && opts->detach) {
        // ok
    } else if (killed_by_us) {
        res->error = MP_SUBPROCESS_EKILLED_BY_US;
    } else {
        res->error = MP_SUBPROCESS_EGENERIC;
    }
}
