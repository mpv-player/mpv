/*
 * poll shim that supports device files on macOS.
 *
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

#include <stdlib.h>
#include <poll.h>
#include <sys/select.h>
#include <stdio.h>

#include "osdep/polldev.h"

int polldev(struct pollfd fds[], nfds_t nfds, int timeout) {
#ifdef __APPLE__
    int maxfd = 0;
    fd_set readfds, writefds, errorfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);
    for (size_t i = 0; i < nfds; ++i) {
        struct pollfd *fd = &fds[i];
        if (fd->fd > maxfd) {
            maxfd = fd->fd;
        }
        if ((fd->events & POLLIN)) {
            FD_SET(fd->fd, &readfds);
        }
        if ((fd->events & POLLOUT)) {
            FD_SET(fd->fd, &writefds);
        }
        if ((fd->events & POLLERR)) {
            FD_SET(fd->fd, &errorfds);
        }
    }
    struct timeval _timeout = {
        .tv_sec = timeout / 1000,
        .tv_usec = (timeout % 1000) * 1000
    };
    int n = select(maxfd + 1, &readfds, &writefds, &errorfds,
        timeout != -1 ? &_timeout : NULL);
    if (n < 0) {
        return n;
    }
    for (size_t i = 0; i < nfds; ++i) {
        struct pollfd *fd = &fds[i];
        fd->revents = 0;
        if (FD_ISSET(fd->fd, &readfds)) {
            fd->revents |= POLLIN;
        }
        if (FD_ISSET(fd->fd, &writefds)) {
            fd->revents |= POLLOUT;
        }
        if (FD_ISSET(fd->fd, &errorfds)) {
            fd->revents |= POLLERR;
        }
    }
    return n;
#else
    return poll(fds, nfds, timeout);
#endif
}
