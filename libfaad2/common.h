/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
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
** $Id$
**/

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#  include "../config.h"

#define INLINE __inline

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

#define ERROR_RESILIENCE


/* Allow decoding of MAIN profile AAC */
#define MAIN_DEC
/* Allow decoding of SSR profile AAC */
//#define SSR_DEC
/* Allow decoding of LTP profile AAC */
#define LTP_DEC
/* Allow decoding of LD profile AAC */
#define LD_DEC

/* LD can't do without LTP */
#ifdef LD_DEC
#ifndef ERROR_RESILIENCE
#define ERROR_RESILIENCE
#endif
#ifndef LTP_DEC
#define LTP_DEC
#endif
#endif


#define SBR_DEC
//#define SBR_LOW_POWER

#ifdef FIXED_POINT
#ifndef SBR_LOW_POWER
#define SBR_LOW_POWER
#endif
#endif

#ifdef FIXED_POINT
#define SBR_DIV(A, B) (((int64_t)A << REAL_BITS)/B)
#else
#define SBR_DIV(A, B) ((A)/(B))
#endif

#ifndef SBR_LOW_POWER
#define qmf_t complex_t
#define QMF_RE(A) RE(A)
#define QMF_IM(A) IM(A)
#else
#define qmf_t real_t
#define QMF_RE(A) (A)
#define QMF_IM(A) 0
#endif


/* END COMPILE TIME DEFINITIONS */

#ifndef FIXED_POINT
#define POW_TABLE_SIZE 200
#endif


#if defined(_WIN32)

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

/* Define if needed */
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

  #ifdef HAS_MATHF_H
    #include <mathf.h>
  #else
    #include <math.h>
  #endif

  #include "fixed.h"

#elif defined(USE_DOUBLE_PRECISION)

  typedef double real_t;

  #include <math.h>

  #define MUL(A,B) ((A)*(B))
  #define MUL_C_C(A,B) ((A)*(B))
  #define MUL_R_C(A,B) ((A)*(B))

  #define REAL_CONST(A) ((real_t)(A))
  #define COEF_CONST(A) ((real_t)(A))

#else /* Normal floating point operation */

  typedef float real_t;

  #define MUL(A,B) ((A)*(B))
  #define MUL_C_C(A,B) ((A)*(B))
  #define MUL_R_C(A,B) ((A)*(B))

  #define REAL_CONST(A) ((real_t)(A))
  #define COEF_CONST(A) ((real_t)(A))

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

typedef real_t complex_t[2];
#define RE(A) A[0]
#define IM(A) A[1]


/* common functions */
int32_t int_log2(int32_t val);
uint32_t random_int(void);
uint8_t get_sr_index(uint32_t samplerate);
uint32_t get_sample_rate(uint8_t sr_index);
int8_t can_decode_ot(uint8_t object_type);

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#ifndef M_PI_2 /* PI/2 */
#define M_PI_2 1.57079632679489661923
#endif


#ifdef __cplusplus
}
#endif
#endif
