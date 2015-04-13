#ifndef MP_ENDIAN_H_
#define MP_ENDIAN_H_

#include <sys/types.h>

#if !defined(BYTE_ORDER)

#if defined(__BYTE_ORDER)
#define BYTE_ORDER      __BYTE_ORDER
#define LITTLE_ENDIAN   __LITTLE_ENDIAN
#define BIG_ENDIAN      __BIG_ENDIAN
#elif defined(__DARWIN_BYTE_ORDER)
#define BYTE_ORDER      __DARWIN_BYTE_ORDER
#define LITTLE_ENDIAN   __DARWIN_LITTLE_ENDIAN
#define BIG_ENDIAN      __DARWIN_BIG_ENDIAN
#else
#include <libavutil/bswap.h>
#if AV_HAVE_BIGENDIAN
#define BYTE_ORDER      1234
#define LITTLE_ENDIAN   4321
#define BIG_ENDIAN      1234
#else
#define BYTE_ORDER      1234
#define LITTLE_ENDIAN   1234
#define BIG_ENDIAN      4321
#endif
#endif

#endif /* !defined(BYTE_ORDER) */

#if BYTE_ORDER == BIG_ENDIAN
#define MP_SELECT_LE_BE(LE, BE) BE
#else
#define MP_SELECT_LE_BE(LE, BE) LE
#endif

#endif
