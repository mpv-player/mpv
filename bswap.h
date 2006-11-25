#ifndef __MP_BSWAP_H__
#define __MP_BSWAP_H__

#include "libavutil/common.h"
#include "libavutil/bswap.h"

static inline float bswap_flt(float x) {
  union {uint32_t i; float f;} u;
  u.f = x;
  u.i = bswap_32(u.i);
  return u.f;
}

static inline double bswap_dbl(double x) {
  union {uint64_t i; double d;} u;
  u.d = x;
  u.i = bswap_64(u.i);
  return u.d;
}

static inline long double bswap_ldbl(long double x) {
  union {char d[10]; long double ld;} uin;
  union {char d[10]; long double ld;} uout;
  uin.ld = x;
  uout.d[0] = uin.d[9];
  uout.d[1] = uin.d[8];
  uout.d[2] = uin.d[7];
  uout.d[3] = uin.d[6];
  uout.d[4] = uin.d[5];
  uout.d[5] = uin.d[4];
  uout.d[6] = uin.d[3];
  uout.d[7] = uin.d[2];
  uout.d[8] = uin.d[1];
  uout.d[9] = uin.d[0];
  return uout.ld;
}

// be2me ... BigEndian to MachineEndian
// le2me ... LittleEndian to MachineEndian

#ifdef WORDS_BIGENDIAN
#define be2me_flt(x) (x)
#define be2me_dbl(x) (x)
#define be2me_ldbl(x) (x)
#define le2me_flt(x) bswap_flt(x)
#define le2me_dbl(x) bswap_dbl(x)
#define le2me_ldbl(x) bswap_ldbl(x)
#else
#define be2me_flt(x) bswap_flt(x)
#define be2me_dbl(x) bswap_dbl(x)
#define be2me_ldbl(x) bswap_ldbl(x)
#define le2me_flt(x) (x)
#define le2me_dbl(x) (x)
#define le2me_ldbl(x) (x)
#endif

#endif /* __MP_BSWAP_H__ */
