#ifndef MPLAYER_MPBSWAP_H
#define MPLAYER_MPBSWAP_H

#include <sys/types.h>
#include "config.h"
#include "libavutil/bswap.h"

#ifndef HAVE_SWAB
void swab(const void *from, void *to, ssize_t n);
#endif

#endif /* MPLAYER_MPBSWAP_H */
