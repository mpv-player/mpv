/*
 * Compilers that uses ILP32, LP64 or P64 type models
 * for both Win32 and Win64 are supported by this file.
 */

/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#ifndef __WINE_BASETSD_H
#define __WINE_BASETSD_H

#ifdef __WINE__
#include "config.h"
#endif /* defined(__WINE__) */

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

/*
 * Win32 was easy to implement under Unix since most (all?) 32-bit
 * Unices uses the same type model (ILP32) as Win32, where int, long
 * and pointer are 32-bit.
 *
 * Win64, however, will cause some problems when implemented under Unix.
 * Linux/{Alpha, Sparc64} and most (all?) other 64-bit Unices uses
 * the LP64 type model where int is 32-bit and long and pointer are
 * 64-bit. Win64 on the other hand uses the P64 (sometimes called LLP64)
 * type model where int and long are 32 bit and pointer is 64-bit.
 */

/* Type model indepent typedefs */

#ifndef __INTEL_COMPILER

#ifndef __int8
typedef char          __int8;
#endif
#ifndef __uint8
typedef unsigned char __uint8;
#endif

#ifndef __int16
typedef short          __int16;
#endif
#ifndef __uint16
typedef unsigned short __uint16;
#endif

#ifndef __int32
typedef int          __int32;
#endif
#ifndef __uint32
typedef unsigned int __uint32;
#endif

#ifndef __int64
typedef long long          __int64;
#endif
#ifndef __uint64
typedef unsigned long long __uint64;
#endif

#else

typedef unsigned __int8  __uint8;
typedef unsigned __int16 __uint16;
typedef unsigned __int32 __uint32;
typedef unsigned __int64 __uint64;

#endif /* __INTEL_COMPILER */

#if defined(_WIN64)

typedef __uint32 __ptr32;
typedef void    *__ptr64;

#else /* FIXME: defined(_WIN32) */

typedef void    *__ptr32;
typedef __uint64 __ptr64;

#endif

/* Always signed and 32 bit wide */

typedef __int32 LONG32;
//typedef __int32 INT32;

typedef LONG32 *PLONG32;
//typedef INT32  *PINT32;

/* Always unsigned and 32 bit wide */

typedef __uint32 ULONG32;
typedef __uint32 DWORD32;
typedef __uint32 UINT32;

typedef ULONG32 *PULONG32;
typedef DWORD32 *PDWORD32;
typedef UINT32  *PUINT32;

/* Always signed and 64 bit wide */

typedef __int64 LONG64;
typedef __int64 INT64;

typedef LONG64 *PLONG64;
typedef INT64  *PINT64;

/* Always unsigned and 64 bit wide */

typedef __uint64 ULONG64;
typedef __uint64 DWORD64;
typedef __uint64 UINT64;

typedef ULONG64 *PULONG64;
typedef DWORD64 *PDWORD64;
typedef UINT64  *PUINT64;

/* Win32 or Win64 dependent typedef/defines. */

#ifdef _WIN64

typedef __int64 INT_PTR, *PINT_PTR;
typedef __uint64 UINT_PTR, *PUINT_PTR;

#define MAXINT_PTR 0x7fffffffffffffff
#define MININT_PTR 0x8000000000000000
#define MAXUINT_PTR 0xffffffffffffffff

typedef __int32 HALF_PTR, *PHALF_PTR;
typedef __int32 UHALF_PTR, *PUHALF_PTR;

#define MAXHALF_PTR 0x7fffffff
#define MINHALF_PTR 0x80000000
#define MAXUHALF_PTR 0xffffffff

typedef __int64 LONG_PTR, *PLONG_PTR;
typedef __uint64 ULONG_PTR, *PULONG_PTR;
typedef __uint64 DWORD_PTR, *PDWORD_PTR;

#else /* FIXME: defined(_WIN32) */

typedef __int32 INT_PTR, *PINT_PTR;
typedef __uint32 UINT_PTR, *PUINT_PTR;

#define MAXINT_PTR 0x7fffffff
#define MININT_PTR 0x80000000
#define MAXUINT_PTR 0xffffffff

typedef __int16 HALF_PTR, *PHALF_PTR;
typedef __uint16 UHALF_PTR, *PUHALF_PTR;

#define MAXUHALF_PTR 0xffff
#define MAXHALF_PTR 0x7fff
#define MINHALF_PTR 0x8000

typedef __int32 LONG_PTR, *PLONG_PTR;
typedef __uint32 ULONG_PTR, *PULONG_PTR;
typedef __uint32 DWORD_PTR, *PDWORD_PTR;

#endif /* defined(_WIN64) || defined(_WIN32) */

typedef INT_PTR SSIZE_T, *PSSIZE_T;
typedef UINT_PTR SIZE_T, *PSIZE_T;

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* !defined(__WINE_BASETSD_H) */



