#pragma once

#include <poll.h>
#include <stdint.h>

// Behaves like poll(3) but works for device files on macOS.
// Only supports POLLIN and POLLOUT.
int polldev(struct pollfd fds[], nfds_t nfds, int timeout);

// Generic polling wrapper. It will try and use higher resolution
// polling (ppoll) if available.
int mp_poll(struct pollfd *fds, int nfds, int64_t timeout_ns);
