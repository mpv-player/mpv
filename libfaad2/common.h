/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** Initially modified for use with MPlayer by Arpad Gereöffy on 2003/08/30
** $Id$
** detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
** local_changes.diff contains the exact changes to this file.
**/

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Allow build on Cygwin*/
#if defined(__CYGWIN__)
#define __STRICT_ANSI__
#endif

#include "../config.h"

#define INLINE __inline
#if 0 //defined(_WIN32) && !defined(_WIN32_WCE)
#define ALIGN __declspec(align(16))
#else
#define ALIGN
#endif

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* COMPILE TIME DEFINITIONS */

/* use double precision */
/* #define USE_DOUBLE_PRECISION */
/* use fixed point reals */
//#define FIXED_POINT
//#define BIG_IQ_TABLE

/* Use if target platform has address generators with autoincrement */
//#define PREFER_POINTERS

#if defined(_WIN32_WCE) || defined(__arm__)
#define FIXED_POINT
#endif


#define ERROR_RESILIENCE


/* Allow decoding of MAIN profile AAC */
#define MAIN_DEC
/* Allow decoding of SSR profile AAC */
//#define SSR_DEC
/* Allow decoding of LTP profile AAC */
#define LTP_DEC
/* Allow decoding of LD profile AAC */
#define LD_DEC
/* Allow decoding of scalable profiles */
//#define SCALABLE_DEC
/* Allow decoding of Digital Radio Mondiale (DRM) */
//#define DRM
//#define DRM_PS

/* LD can't do without LTP */
#ifdef LD_DEC
#ifndef ERROR_RESILIENCE
#define ERROR_RESILIENCE
#endif
#ifndef LTP_DEC
#define LTP_DEC
#endif
#endif

#define ALLOW_SMALL_FRAMELENGTH


// Define LC_ONLY_DECODER if you want a pure AAC LC decoder (independant of SBR_DEC and PS_DEC)
//#define LC_ONLY_DECODER
#ifdef LC_ONLY_DECODER
  #undef LD_DEC
  #undef LTP_DEC
  #undef MAIN_DEC
  #undef SSR_DEC
  #undef DRM
  #undef ALLOW_SMALL_FRAMELENGTH
  #undef ERROR_RESILIENCE
#endif

#define SBR_DEC
//#define SBR_LOW_POWER
#define PS_DEC

/* FIXED POINT: No MAIN decoding */
#ifdef FIXED_POINT
# ifdef MAIN_DEC
#  undef MAIN_DEC
# endif
# ifdef SBR_DEC
#  undef SBR_DEC
# endif
#endif // FIXED_POINT

#ifdef DRM
# ifndef SCALABLE_DEC
#  define SCALABLE_DEC
# endif
#endif


#ifdef FIXED_POINT
#define DIV_R(A, B) (((int64_t)A << REAL_BITS)/B)
#define DIV_C(A, B) (((int64_t)A << COEF_BITS)/B)
#else
#define DIV_R(A, B) ((A)/(B))
#define DIV_C(A, B) ((A)/(B))
#endif

#ifndef SBR_LOW_POWER
#define qmf_t complex_t
#define QMF_RE(A) RE(A)
#define QMF_IM(A) IM(A)
#else
#define qmf_t real_t
#define QMF_RE(A) (A)
#define QMF_IM(A)
#endif


/* END COMPILE TIME DEFINITIONS */

#if defined(_WIN32) && !defined(__MINGW32__)

#include <stdlib.h>

#if 0
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8  int8_t;
#else
#include <inttypes.h>
#endif

typedef float float32_t;


#else

/* #undef HAVE_FLOAT32_T */
/* Define if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define if you have the ANSI C header files. */
#define STDC_HEADERS 1

#include <stdio.h>
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#endif
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# else
/* we need these... */
typedef unsigned long long uint64_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef long int32_t;
typedef short int16_t;
typedef char int8_t;
# endif
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef HAVE_FLOAT32_T
typedef float float32_t;
#endif

#if STDC_HEADERS
# include <string.h>
#else
# if !HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(), *strrchr();
# if !HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy((s), (d), (n))
#  define memmove(d, s, n) bcopy((s), (d), (n))
# endif
#endif

#endif

#ifdef WORDS_BIGENDIAN
#define ARCH_IS_BIG_ENDIAN
#endif

/* FIXED_POINT doesn't work with MAIN and SSR yet */
#ifdef FIXED_POINT
  #undef MAIN_DEC
  #undef SSR_DEC
#endif


#if defined(FIXED_POINT)

  #include "fixed.h"

#elif defined(USE_DOUBLE_PRECISION)

  typedef double real_t;

  #include <math.h>

  #define MUL_R(A,B) ((A)*(B))
  #define MUL_C(A,B) ((A)*(B))
  #define MUL_F(A,B) ((A)*(B))

  /* Complex multiplication */
  static INLINE void ComplexMult(real_t *y1, real_t *y2,
      real_t x1, real_t x2, real_t c1, real_t c2)
  {
      *y1 = MUL_F(x1, c1) + MUL_F(x2, c2);
      *y2 = MUL_F(x2, c1) - MUL_F(x1, c2);
  }

  #define REAL_CONST(A) ((real_t)(A))
  #define COEF_CONST(A) ((real_t)(A))
  #define Q2_CONST(A) ((real_t)(A))
  #define FRAC_CONST(A) ((real_t)(A)) /* pure fractional part */

#else /* Normal floating point operation */

  typedef float real_t;

  #define MUL_R(A,B) ((A)*(B))
  #define MUL_C(A,B) ((A)*(B))
  #define MUL_F(A,B) ((A)*(B))

  #define REAL_CONST(A) ((real_t)(A))
  #define COEF_CONST(A) ((real_t)(A))
  #define Q2_CONST(A) ((real_t)(A))
  #define FRAC_CONST(A) ((real_t)(A)) /* pure fractional part */

  /* Complex multiplication */
  static INLINE void ComplexMult(real_t *y1, real_t *y2,
      real_t x1, real_t x2, real_t c1, real_t c2)
  {
      *y1 = MUL_F(x1, c1) + MUL_F(x2, c2);
      *y2 = MUL_F(x2, c1) - MUL_F(x1, c2);
  }


  #if defined(_WIN32) && !defined(__MINGW32__) && !defined(HAVE_LRINTF)
    #define HAS_LRINTF
    static INLINE int lrintf(float f)
    {
        int i;
        __asm
        {
            fld   f
            fistp i
        }
        return i;
    }
  #elif (defined(__i386__) && defined(__GNUC__)) && !defined(HAVE_LRINTF)
    #define HAS_LRINTF
    // from http://www.stereopsis.com/FPU.html
    static INLINE int lrintf(float f)
    {
        int i;
        __asm__ __volatile__ (
            "flds %1        \n\t"
            "fistpl %0      \n\t"
            : "=m" (i)
            : "m" (f));
        return i;
    }
  #endif


  #ifdef __ICL /* only Intel C compiler has fmath ??? */

    #include <mathf.h>

    #define sin sinf
    #define cos cosf
    #define log logf
    #define floor floorf
    #define ceil ceilf
    #define sqrt sqrtf

  #else

#include <math.h>

#ifdef HAVE_LRINTF
#  define HAS_LRINTF
#  define _ISOC9X_SOURCE 1
#  define _ISOC99_SOURCE 1
#  define __USE_ISOC9X   1
#  define __USE_ISOC99   1
#endif

#ifdef HAVE_SINF
#  define sin sinf
#error
#endif
#ifdef HAVE_COSF
#  define cos cosf
#endif
#ifdef HAVE_LOGF
#  define log logf
#endif
#ifdef HAVE_EXPF
#  define exp expf
#endif
#ifdef HAVE_FLOORF
#  define floor floorf
#endif
#ifdef HAVE_CEILF
#  define ceil ceilf
#endif
#ifdef HAVE_SQRTF
#  define sqrt sqrtf
#endif

  #endif

#endif

#ifndef HAS_LRINTF
/* standard cast */
#define lrintf(f) ((int32_t)(f))
#endif

typedef real_t complex_t[2];
#define RE(A) A[0]
#define IM(A) A[1]


/* common functions */
uint8_t cpu_has_sse(void);
uint32_t random_int(void);
uint32_t ones32(uint32_t x);
uint32_t floor_log2(uint32_t x);
uint32_t wl_min_lzc(uint32_t x);
#ifdef FIXED_POINT
#define LOG2_MIN_INF REAL_CONST(-10000)
int32_t log2_int(uint32_t val);
int32_t log2_fix(uint32_t val);
int32_t pow2_int(real_t val);
real_t pow2_fix(real_t val);
#endif
uint8_t get_sr_index(const uint32_t samplerate);
uint8_t max_pred_sfb(const uint8_t sr_index);
uint8_t max_tns_sfb(const uint8_t sr_index, const uint8_t object_type,
                    const uint8_t is_short);
uint32_t get_sample_rate(const uint8_t sr_index);
int8_t can_decode_ot(const uint8_t object_type);

void *faad_malloc(size_t size);
void faad_free(void *b);

//#define PROFILE
#ifdef PROFILE
static int64_t faad_get_ts()
{
    __asm
    {
        rdtsc
    }
}
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2 /* PI/2 */
#define M_PI_2 1.57079632679489661923
#endif


#ifdef __cplusplus
}
#endif
#endif
