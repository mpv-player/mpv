/* lzoconf.h -- configuration for the LZO real-time data compression library

   This file is part of the LZO real-time data compression library.

   Copyright (C) 2000 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1999 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1998 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1997 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1996 Markus Franz Xaver Johannes Oberhumer

   The LZO library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   The LZO library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the LZO library; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Markus F.X.J. Oberhumer
   <markus.oberhumer@jk.uni-linz.ac.at>
   http://wildsau.idv.uni-linz.ac.at/mfx/lzo.html
 */


#ifndef __LZOCONF_H
#define __LZOCONF_H

#define LZO_VERSION             0x1070
#define LZO_VERSION_STRING      "1.07"
#define LZO_VERSION_DATE        "Oct 18 2000"

/* internal Autoconf configuration file - only used when building LZO */
#if defined(LZO_HAVE_CONFIG_H)
#  include <config.h>
#endif
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif


/***********************************************************************
// LZO requires a conforming <limits.h>
************************************************************************/

#if !defined(CHAR_BIT) || (CHAR_BIT != 8)
#  error "invalid CHAR_BIT"
#endif
#if !defined(UCHAR_MAX) || !defined(UINT_MAX) || !defined(ULONG_MAX)
#  error "check your compiler installation"
#endif
#if (USHRT_MAX < 1) || (UINT_MAX < 1) || (ULONG_MAX < 1)
#  error "your limits.h macros are broken"
#endif

/* workaround a cpp bug under hpux 10.20 */
#define LZO_0xffffffffL     4294967295ul


/***********************************************************************
// architecture defines
************************************************************************/

#if !defined(__LZO_WIN) && !defined(__LZO_DOS) && !defined(__LZO_OS2)
#  if defined(__WINDOWS__) || defined(_WINDOWS) || defined(_Windows)
#    define __LZO_WIN
#  elif defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
#    define __LZO_WIN
#  elif defined(__NT__) || defined(__NT_DLL__) || defined(__WINDOWS_386__)
#    define __LZO_WIN
#  elif defined(__DOS__) || defined(__MSDOS__) || defined(MSDOS)
#    define __LZO_DOS
#  elif defined(__OS2__) || defined(__OS2V2__) || defined(OS2)
#    define __LZO_OS2
#  elif defined(__palmos__)
#    define __LZO_PALMOS
#  elif defined(__TOS__) || defined(__atarist__)
#    define __LZO_TOS
#  endif
#endif

#if (UINT_MAX < LZO_0xffffffffL)
#  if defined(__LZO_WIN)
#    define __LZO_WIN16
#  elif defined(__LZO_DOS)
#    define __LZO_DOS16
#  elif defined(__LZO_PALMOS)
#    define __LZO_PALMOS16
#  elif defined(__LZO_TOS)
#    define __LZO_TOS16
#  elif defined(__C166__)
#  else
#    error "16-bit target not supported - contact me for porting hints"
#  endif
#endif

#if !defined(__LZO_i386)
#  if defined(__LZO_DOS) || defined(__LZO_WIN16)
#    define __LZO_i386
#  elif defined(__i386__) || defined(__386__) || defined(_M_IX86)
#    define __LZO_i386
#  endif
#endif

#if defined(__LZO_STRICT_16BIT)
#  if (UINT_MAX < LZO_0xffffffffL)
#    include <lzo16bit.h>
#  endif
#endif

/* memory checkers */
#if !defined(__LZO_CHECKER)
#  if defined(__BOUNDS_CHECKING_ON)
#    define __LZO_CHECKER
#  elif defined(__CHECKER__)
#    define __LZO_CHECKER
#  elif defined(__INSURE__)
#    define __LZO_CHECKER
#  elif defined(__PURIFY__)
#    define __LZO_CHECKER
#  endif
#endif


/***********************************************************************
// integral and pointer types
************************************************************************/

/* Integral types with 32 bits or more */
#if !defined(LZO_UINT32_MAX)
#  if (UINT_MAX >= LZO_0xffffffffL)
     typedef unsigned int       lzo_uint32;
     typedef int                lzo_int32;
#    define LZO_UINT32_MAX      UINT_MAX
#    define LZO_INT32_MAX       INT_MAX
#    define LZO_INT32_MIN       INT_MIN
#  elif (ULONG_MAX >= LZO_0xffffffffL)
     typedef unsigned long      lzo_uint32;
     typedef long               lzo_int32;
#    define LZO_UINT32_MAX      ULONG_MAX
#    define LZO_INT32_MAX       LONG_MAX
#    define LZO_INT32_MIN       LONG_MIN
#  else
#    error "lzo_uint32"
#  endif
#endif

/* lzo_uint is used like size_t */
#if !defined(LZO_UINT_MAX)
#  if (UINT_MAX >= LZO_0xffffffffL)
     typedef unsigned int       lzo_uint;
     typedef int                lzo_int;
#    define LZO_UINT_MAX        UINT_MAX
#    define LZO_INT_MAX         INT_MAX
#    define LZO_INT_MIN         INT_MIN
#  elif (ULONG_MAX >= LZO_0xffffffffL)
     typedef unsigned long      lzo_uint;
     typedef long               lzo_int;
#    define LZO_UINT_MAX        ULONG_MAX
#    define LZO_INT_MAX         LONG_MAX
#    define LZO_INT_MIN         LONG_MIN
#  else
#    error "lzo_uint"
#  endif
#endif


/* Memory model that allows to access memory at offsets of lzo_uint. */
#if !defined(__LZO_MMODEL)
#  if (LZO_UINT_MAX <= UINT_MAX)
#    define __LZO_MMODEL
#  elif defined(__LZO_DOS16) || defined(__LZO_WIN16)
#    define __LZO_MMODEL        __huge
#    define LZO_999_UNSUPPORTED
#  elif defined(__LZO_PALMOS16) || defined(__LZO_TOS16)
#    define __LZO_MMODEL
#  else
#    error "__LZO_MMODEL"
#  endif
#endif

/* no typedef here because of const-pointer issues */
#define lzo_byte                unsigned char __LZO_MMODEL
#define lzo_bytep               unsigned char __LZO_MMODEL *
#define lzo_charp               char __LZO_MMODEL *
#define lzo_voidp               void __LZO_MMODEL *
#define lzo_shortp              short __LZO_MMODEL *
#define lzo_ushortp             unsigned short __LZO_MMODEL *
#define lzo_uint32p             lzo_uint32 __LZO_MMODEL *
#define lzo_int32p              lzo_int32 __LZO_MMODEL *
#define lzo_uintp               lzo_uint __LZO_MMODEL *
#define lzo_intp                lzo_int __LZO_MMODEL *
#define lzo_voidpp              lzo_voidp __LZO_MMODEL *
#define lzo_bytepp              lzo_bytep __LZO_MMODEL *

typedef int lzo_bool;

#ifndef lzo_sizeof_dict_t
#  define lzo_sizeof_dict_t     sizeof(lzo_bytep)
#endif


/***********************************************************************
// function types
************************************************************************/

/* linkage */
#if !defined(__LZO_EXTERN_C)
#  ifdef __cplusplus
#    define __LZO_EXTERN_C      extern "C"
#  else
#    define __LZO_EXTERN_C      extern
#  endif
#endif

/* calling conventions */
#if !defined(__LZO_CDECL)
#  if defined(__LZO_DOS16) || defined(__LZO_WIN16)
#    define __LZO_CDECL         __far __cdecl
#  elif defined(__LZO_i386) && defined(_MSC_VER)
#    define __LZO_CDECL         __cdecl
#  elif defined(__LZO_i386) && defined(__WATCOMC__)
#    define __LZO_CDECL         __near __cdecl
#  else
#    define __LZO_CDECL
#  endif
#endif
#if !defined(__LZO_ENTRY)
#  define __LZO_ENTRY           __LZO_CDECL
#endif

/* DLL export information */
#if !defined(__LZO_EXPORT1)
#  define __LZO_EXPORT1
#endif
#if !defined(__LZO_EXPORT2)
#  define __LZO_EXPORT2
#endif

/* calling convention for C functions */
#if !defined(LZO_PUBLIC)
#  define LZO_PUBLIC(_rettype)  __LZO_EXPORT1 _rettype __LZO_EXPORT2 __LZO_ENTRY
#endif
#if !defined(LZO_EXTERN)
#  define LZO_EXTERN(_rettype)  __LZO_EXTERN_C LZO_PUBLIC(_rettype)
#endif
#if !defined(LZO_PRIVATE)
#  define LZO_PRIVATE(_rettype) static _rettype __LZO_ENTRY
#endif

/* cdecl calling convention for assembler functions */
#if !defined(LZO_PUBLIC_CDECL)
#  define LZO_PUBLIC_CDECL(_rettype) \
                __LZO_EXPORT1 _rettype __LZO_EXPORT2 __LZO_CDECL
#endif
#if !defined(LZO_EXTERN_CDECL)
#  define LZO_EXTERN_CDECL(_rettype)  __LZO_EXTERN_C LZO_PUBLIC_CDECL(_rettype)
#endif


typedef int
(__LZO_ENTRY *lzo_compress_t)   ( const lzo_byte *src, lzo_uint  src_len,
                                        lzo_byte *dst, lzo_uint *dst_len,
                                        lzo_voidp wrkmem );

typedef int
(__LZO_ENTRY *lzo_decompress_t) ( const lzo_byte *src, lzo_uint  src_len,
                                        lzo_byte *dst, lzo_uint *dst_len,
                                        lzo_voidp wrkmem );

typedef int
(__LZO_ENTRY *lzo_optimize_t)   (       lzo_byte *src, lzo_uint  src_len,
                                        lzo_byte *dst, lzo_uint *dst_len,
                                        lzo_voidp wrkmem );

typedef int
(__LZO_ENTRY *lzo_compress_dict_t)(const lzo_byte *src, lzo_uint  src_len,
                                        lzo_byte *dst, lzo_uint *dst_len,
                                        lzo_voidp wrkmem,
                                  const lzo_byte *dict, lzo_uint dict_len );

typedef int
(__LZO_ENTRY *lzo_decompress_dict_t)(const lzo_byte *src, lzo_uint  src_len,
                                        lzo_byte *dst, lzo_uint *dst_len,
                                        lzo_voidp wrkmem,
                                  const lzo_byte *dict, lzo_uint dict_len );


/* a progress indicator callback function */
typedef void (__LZO_ENTRY *lzo_progress_callback_t) (lzo_uint, lzo_uint);


/***********************************************************************
// error codes and prototypes
************************************************************************/

/* Error codes for the compression/decompression functions. Negative
 * values are errors, positive values will be used for special but
 * normal events.
 */
#define LZO_E_OK                    0
#define LZO_E_ERROR                 (-1)
#define LZO_E_OUT_OF_MEMORY         (-2)    /* not used right now */
#define LZO_E_NOT_COMPRESSIBLE      (-3)    /* not used right now */
#define LZO_E_INPUT_OVERRUN         (-4)
#define LZO_E_OUTPUT_OVERRUN        (-5)
#define LZO_E_LOOKBEHIND_OVERRUN    (-6)
#define LZO_E_EOF_NOT_FOUND         (-7)
#define LZO_E_INPUT_NOT_CONSUMED    (-8)


/* lzo_init() should be the first function you call.
 * Check the return code !
 *
 * lzo_init() is a macro to allow checking that the library and the
 * compiler's view of various types are consistent.
 */
#define lzo_init() __lzo_init2(LZO_VERSION,(int)sizeof(short),(int)sizeof(int),\
    (int)sizeof(long),(int)sizeof(lzo_uint32),(int)sizeof(lzo_uint),\
    (int)lzo_sizeof_dict_t,(int)sizeof(char *),(int)sizeof(lzo_voidp),\
    (int)sizeof(lzo_compress_t))
LZO_EXTERN(int) __lzo_init2(unsigned,int,int,int,int,int,int,int,int,int);

/* version functions (useful for shared libraries) */
LZO_EXTERN(unsigned) lzo_version(void);
LZO_EXTERN(const char *) lzo_version_string(void);
LZO_EXTERN(const char *) lzo_version_date(void);
LZO_EXTERN(const lzo_charp) _lzo_version_string(void);
LZO_EXTERN(const lzo_charp) _lzo_version_date(void);

/* string functions */
LZO_EXTERN(int)
lzo_memcmp(const lzo_voidp _s1, const lzo_voidp _s2, lzo_uint _len);
LZO_EXTERN(lzo_voidp)
lzo_memcpy(lzo_voidp _dest, const lzo_voidp _src, lzo_uint _len);
LZO_EXTERN(lzo_voidp)
lzo_memmove(lzo_voidp _dest, const lzo_voidp _src, lzo_uint _len);
LZO_EXTERN(lzo_voidp)
lzo_memset(lzo_voidp _s, int _c, lzo_uint _len);

/* checksum functions */
LZO_EXTERN(lzo_uint32)
lzo_adler32(lzo_uint32 _adler, const lzo_byte *_buf, lzo_uint _len);
LZO_EXTERN(lzo_uint32)
lzo_crc32(lzo_uint32 _c, const lzo_byte *_buf, lzo_uint _len);

/* memory allocation functions */
LZO_EXTERN(lzo_bytep) lzo_alloc(lzo_uint _nelems, lzo_uint _size);
LZO_EXTERN(lzo_bytep) lzo_malloc(lzo_uint _size);
LZO_EXTERN(void) lzo_free(lzo_voidp _ptr);

typedef lzo_bytep (__LZO_ENTRY *lzo_alloc_hook_t) (lzo_uint, lzo_uint);
typedef void (__LZO_ENTRY *lzo_free_hook_t) (lzo_voidp);

extern lzo_alloc_hook_t lzo_alloc_hook;
extern lzo_free_hook_t lzo_free_hook;

/* misc. */
LZO_EXTERN(lzo_bool) lzo_assert(int _expr);
LZO_EXTERN(int) _lzo_config_check(void);
typedef union { lzo_bytep p; lzo_uint u; } __lzo_pu_u;
typedef union { lzo_bytep p; lzo_uint32 u32; } __lzo_pu32_u;

/* align a char pointer on a boundary that is a multiple of `size' */
LZO_EXTERN(unsigned) __lzo_align_gap(const lzo_voidp _ptr, lzo_uint _size);
#define LZO_PTR_ALIGN_UP(_ptr,_size) \
    ((_ptr) + (lzo_uint) __lzo_align_gap((const lzo_voidp)(_ptr),(lzo_uint)(_size)))

/* deprecated - only for backward compatibility */
#define LZO_ALIGN(_ptr,_size) LZO_PTR_ALIGN_UP(_ptr,_size)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* already included */

