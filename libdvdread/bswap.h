#ifndef DVDREAD_BSWAP_H
#define DVDREAD_BSWAP_H

#include "libavutil/bswap.h"

#ifdef WORDS_BIGENDIAN
#define B2N_16(x)
#define B2N_32(x)
#define B2N_64(x)
#else
#define B2N_16(x) x = bswap_16(x)
#define B2N_32(x) x = bswap_32(x)
#define B2N_64(x) x = bswap_64(x)
#endif

#endif
