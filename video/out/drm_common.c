/*
 * This file is part of mpv.
 *
 * by rr- <rr-@sakuya.pl>
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

#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/vt.h>
#include <unistd.h>

#include "drm_common.h"

#include "common/common.h"
#include "common/msg.h"
#include "osdep/io.h"

#define EVT_RELEASE 1
#define EVT_ACQUIRE 2
#define EVT_INTERRUPT 255
#define HANDLER_ACQUIRE 0
#define HANDLER_RELEASE 1
#define RELEASE_SIGNAL SIGUSR1
#define ACQUIRE_SIGNAL SIGUSR2

static int vt_switcher_pipe[2];

static void vt_switcher_sighandler(int sig)
{
    unsigned char event = sig == RELEASE_SIGNAL ? EVT_RELEASE : EVT_ACQUIRE;
    write(vt_switcher_pipe[1], &event, sizeof(event));
}

static bool has_signal_installed(int signo)
{
    struct sigaction act = { 0 };
    sigaction(signo, 0, &act);
    return act.sa_handler != 0;
}

static int install_signal(int signo, void (*handler)(int))
{
    struct sigaction act = { 0 };
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    return sigaction(signo, &act, NULL);
}

int vt_switcher_init(struct vt_switcher *s, struct mp_log *log)
{
    s->log = log;
    s->tty_fd = -1;
    vt_switcher_pipe[0] = -1;
    vt_switcher_pipe[1] = -1;

    if (mp_make_cloexec_pipe(vt_switcher_pipe)) {
        MP_ERR(s, "Creating pipe failed: %s\n", mp_strerror(errno));
        return -1;
    }

    s->tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (s->tty_fd < 0) {
        MP_ERR(s, "Can't open TTY for VT control: %s\n", mp_strerror(errno));
        return -1;
    }

    if (has_signal_installed(RELEASE_SIGNAL)) {
        MP_ERR(s, "Can't handle VT release - signal already used\n");
        return -1;
    }
    if (has_signal_installed(ACQUIRE_SIGNAL)) {
        MP_ERR(s, "Can't handle VT acquire - signal already used\n");
        return -1;
    }

    if (install_signal(RELEASE_SIGNAL, vt_switcher_sighandler)) {
        MP_ERR(s, "Failed to install release signal: %s\n", mp_strerror(errno));
        return -1;
    }
    if (install_signal(ACQUIRE_SIGNAL, vt_switcher_sighandler)) {
        MP_ERR(s, "Failed to install acquire signal: %s\n", mp_strerror(errno));
        return -1;
    }

    struct vt_mode vt_mode;
    if (ioctl(s->tty_fd, VT_GETMODE, &vt_mode) < 0) {
        MP_ERR(s, "VT_GETMODE failed: %s\n", mp_strerror(errno));
        return -1;
    }

    vt_mode.mode = VT_PROCESS;
    vt_mode.relsig = RELEASE_SIGNAL;
    vt_mode.acqsig = ACQUIRE_SIGNAL;
    if (ioctl(s->tty_fd, VT_SETMODE, &vt_mode) < 0) {
        MP_ERR(s, "VT_SETMODE failed: %s\n", mp_strerror(errno));
        return -1;
    }

    return 0;
}

void vt_switcher_acquire(struct vt_switcher *s,
                         void (*handler)(void*), void *user_data)
{
    s->handlers[HANDLER_ACQUIRE] = handler;
    s->handler_data[HANDLER_ACQUIRE] = user_data;
}

void vt_switcher_release(struct vt_switcher *s,
                         void (*handler)(void*), void *user_data)
{
    s->handlers[HANDLER_RELEASE] = handler;
    s->handler_data[HANDLER_RELEASE] = user_data;
}

void vt_switcher_interrupt_poll(struct vt_switcher *s)
{
    unsigned char event = EVT_INTERRUPT;
    write(vt_switcher_pipe[1], &event, sizeof(event));
}

void vt_switcher_destroy(struct vt_switcher *s)
{
    install_signal(RELEASE_SIGNAL, SIG_DFL);
    install_signal(ACQUIRE_SIGNAL, SIG_DFL);
    close(s->tty_fd);
    close(vt_switcher_pipe[0]);
    close(vt_switcher_pipe[1]);
}

void vt_switcher_poll(struct vt_switcher *s, int timeout_ms)
{
    struct pollfd fds[1] = {
        { .events = POLLIN, .fd = vt_switcher_pipe[0] },
    };
    poll(fds, 1, timeout_ms);
    if (!fds[0].revents)
        return;

    unsigned char event;
    if (read(fds[0].fd, &event, sizeof(event)) != sizeof(event))
        return;

    switch (event) {
    case EVT_RELEASE:
        s->handlers[HANDLER_RELEASE](s->handler_data[HANDLER_RELEASE]);

        if (ioctl(s->tty_fd, VT_RELDISP, 1) < 0) {
            MP_ERR(s, "Failed to release virtual terminal\n");
        }
        break;

    case EVT_ACQUIRE:
        s->handlers[HANDLER_ACQUIRE](s->handler_data[HANDLER_ACQUIRE]);

        if (ioctl(s->tty_fd, VT_RELDISP, VT_ACKACQ) < 0) {
            MP_ERR(s, "Failed to acquire virtual terminal\n");
        }
        break;

    case EVT_INTERRUPT:
        break;
    }
}
