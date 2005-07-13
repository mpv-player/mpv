#ifndef __BSWAP_H__
#define __BSWAP_H__

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#else

#include <inttypes.h>

#ifdef ARCH_X86_64
#  define LEGACY_REGS "=Q"
#else
#  define LEGACY_REGS "=q"
#endif

#if defined(ARCH_X86) || defined(ARCH_X86_64)
static inline uint16_t ByteSwap16(uint16_t x)
{
  __asm("xchgb %b0,%h0"	:
        LEGACY_REGS (x)	:
        "0" (x));
    return x;
}
#define bswap_16(x) ByteSwap16(x)

static inline uint32_t ByteSwap32(uint32_t x)
{
#if __CPU__ > 386
 __asm("bswap	%0":
      "=r" (x)     :
#else
 __asm("xchgb	%b0,%h0\n"
      "	rorl	$16,%0\n"
      "	xchgb	%b0,%h0":
      LEGACY_REGS (x)		:
#endif
      "0" (x));
  return x;
}
#define bswap_32(x) ByteSwap32(x)

static inline uint64_t ByteSwap64(uint64_t x)
{
#ifdef ARCH_X86_64
  __asm("bswap	%0":
        "=r" (x)     :
        "0" (x));
  return x;
#else
  register union { __extension__ uint64_t __ll;
          uint32_t __l[2]; } __x;
  asm("xchgl	%0,%1":
      "=r"(__x.__l[0]),"=r"(__x.__l[1]):
      "0"(bswap_32((unsigned long)x)),"1"(bswap_32((unsigned long)(x>>32))));
  return __x.__ll;
#endif
}
#define bswap_64(x) ByteSwap64(x)

#elif defined(ARCH_SH4)

static inline uint16_t ByteSwap16(uint16_t x) {
	__asm__("swap.b %0,%0":"=r"(x):"0"(x));
	return x;
}

static inline uint32_t ByteSwap32(uint32_t x) {
	__asm__(
	"swap.b %0,%0\n"
	"swap.w %0,%0\n"
	"swap.b %0,%0\n"
	:"=r"(x):"0"(x));
	return x;
}

#define bswap_16(x) ByteSwap16(x)
#define bswap_32(x) ByteSwap32(x)

static inline uint64_t ByteSwap64(uint64_t x)
{
    union { 
        uint64_t ll;
        struct {
           uint32_t l,h;
        } l;
    } r;
    r.l.l = bswap_32 (x);
    r.l.h = bswap_32 (x>>32);
    return r.ll;
}
#define bswap_64(x) ByteSwap64(x)

#else

#define bswap_16(x) (((x) & 0x00ff) << 8 | ((x) & 0xff00) >> 8)
			

// code from bits/byteswap.h (C) 1997, 1998 Free Software Foundation, Inc.
#define bswap_32(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

static inline uint64_t ByteSwap64(uint64_t x)
{
    union { 
        uint64_t ll;
        uint32_t l[2]; 
    } w, r;
    w.ll = x;
    r.l[0] = bswap_32 (w.l[1]);
    r.l[1] = bswap_32 (w.l[0]);
    return r.ll;
}
#define bswap_64(x) ByteSwap64(x)

#endif	/* !ARCH_X86 */

#endif	/* !HAVE_BYTESWAP_H */

static float inline bswap_flt(float x) {
  union {uint32_t i; float f;} u;
  u.f = x;
  u.i = bswap_32(u.i);
  return u.f;
}

static double inline bswap_dbl(double x) {
  union {uint64_t i; double d;} u;
  u.d = x;
  u.i = bswap_64(u.i);
  return u.d;
}

static long double inline bswap_ldbl(long double x) {
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
#define be2me_16(x) (x)
#define be2me_32(x) (x)
#define be2me_64(x) (x)
#define le2me_16(x) bswap_16(x)
#define le2me_32(x) bswap_32(x)
#define le2me_64(x) bswap_64(x)
#define be2me_flt(x) (x)
#define be2me_dbl(x) (x)
#define be2me_ldbl(x) (x)
#define le2me_flt(x) bswap_flt(x)
#define le2me_dbl(x) bswap_dbl(x)
#define le2me_ldbl(x) bswap_ldbl(x)
#else
#define be2me_16(x) bswap_16(x)
#define be2me_32(x) bswap_32(x)
#define be2me_64(x) bswap_64(x)
#define le2me_16(x) (x)
#define le2me_32(x) (x)
#define le2me_64(x) (x)
#define be2me_flt(x) bswap_flt(x)
#define be2me_dbl(x) bswap_dbl(x)
#define be2me_ldbl(x) bswap_ldbl(x)
#define le2me_flt(x) (x)
#define le2me_dbl(x) (x)
#define le2me_ldbl(x) (x)
#endif

#endif /* __BSWAP_H__ */
