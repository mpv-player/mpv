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
** $Id: fixed.h,v 1.9 2003/07/29 08:20:12 menno Exp $
**/

#ifndef __FIXED_H__
#define __FIXED_H__

#ifdef __cplusplus
extern "C" {
#endif


#define COEF_BITS 28
#define COEF_PRECISION (1 << COEF_BITS)
#define REAL_BITS 15 //7
#define REAL_PRECISION (1 << REAL_BITS)


typedef int32_t real_t;


#define REAL_CONST(A) ((real_t)(A*(REAL_PRECISION)))
#define COEF_CONST(A) ((real_t)(A*(COEF_PRECISION)))

#if defined(_WIN32) && !defined(_WIN32_WCE)

/* multiply real with real */
static INLINE MUL(real_t A, real_t B)
{
    _asm {
        mov eax,A
        imul B
#if 0
        shrd eax,edx,REAL_BITS
#else
        shr eax,REAL_BITS
        shl edx,(32-REAL_BITS)
        or eax,edx
#endif
    }
}

/* multiply coef with coef */
static INLINE MUL_C_C(real_t A, real_t B)
{
    _asm {
        mov eax,A
        imul B
#if 0
        shrd eax,edx,COEF_BITS
#else
        shr eax,COEF_BITS
        shl edx,(32-COEF_BITS)
        or eax,edx
#endif
    }
}

/* multiply real with coef */
static INLINE MUL_R_C(real_t A, real_t B)
{
    _asm {
        mov eax,A
        imul B
#if 0
        shrd eax,edx,COEF_BITS
#else
        shr eax,COEF_BITS
        shl edx,(32-COEF_BITS)
        or eax,edx
#endif
    }
}

#elif defined(__GNUC__) && defined (__arm__)

/* taken from MAD */
#define arm_mul(x, y, SCALEBITS) \
       ({      uint32_t __hi; \
               uint32_t __lo; \
               uint32_t __result; \
               asm ("smull  %0, %1, %3, %4\n\t" \
                    "movs   %0, %0, lsr %5\n\t" \
                    "adc    %2, %0, %1, lsl %6" \
                    : "=&r" (__lo), "=&r" (__hi), "=r" (__result) \
                    : "%r" (x), "r" (y), \
                      "M" (SCALEBITS), "M" (32 - (SCALEBITS)) \
                    : "cc"); \
               __result; \
       })

static INLINE real_t MUL(real_t A, real_t B)
{
       return arm_mul( A, B, REAL_BITS);
}

static INLINE real_t MUL_C_C(real_t A, real_t B)
{
       return arm_mul( A, B, COEF_BITS);
}

static INLINE real_t MUL_R_C(real_t A, real_t B)
{
       return arm_mul( A, B, COEF_BITS);
}

#else

  /* multiply real with real */
  #define MUL(A,B) (real_t)(((int64_t)(A)*(int64_t)(B)+(1 << (REAL_BITS-1))) >> REAL_BITS)
  /* multiply coef with coef */
  #define MUL_C_C(A,B) (real_t)(((int64_t)(A)*(int64_t)(B)+(1 << (COEF_BITS-1))) >> COEF_BITS)
  /* multiply real with coef */
  #define MUL_R_C(A,B) (real_t)(((int64_t)(A)*(int64_t)(B)+(1 << (COEF_BITS-1))) >> COEF_BITS)

#endif


#ifdef __cplusplus
}
#endif
#endif
