#pragma once

#include <poll.h>

// Behaves like poll(3) but works for device files on macOS.
// Only supports POLLIN and POLLOUT.
int polldev(struct pollfd fds[], nfds_t nfds, int timeout);
