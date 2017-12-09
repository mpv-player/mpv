#pragma once

#include <poll.h>

// Behaves like poll(3) but works for device files on macOS.
// Only supports POLLIN, POLLOUT, and POLLERR.
int polldev(struct pollfd fds[], nfds_t nfds, int timeout);
