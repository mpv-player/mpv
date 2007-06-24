#ifndef MPBSWAP_H
#define MPBSWAP_H

#include "libavutil/bswap.h"
#ifndef HAVE_SWAB
void swab(const void *from, void *to, ssize_t n);
#endif

#endif /* MPBSWAP_H */
