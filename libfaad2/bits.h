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

#ifndef __BITS_H__
#define __BITS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "analysis.h"
#ifdef ANALYSIS
#include <stdio.h>
#endif

#define BYTE_NUMBIT 8
#define bit2byte(a) ((a+7)/BYTE_NUMBIT)

typedef struct _bitfile
{
    /* bit input */
    uint32_t bufa;
    uint32_t bufb;
    uint32_t bits_left;
    uint32_t buffer_size; /* size of the buffer in bytes */
    uint32_t bytes_used;
    uint8_t no_more_reading;
    uint8_t error;
    uint32_t *tail;
    uint32_t *start;
    void *buffer;
} bitfile;


#if defined (_WIN32) && !defined(_WIN32_WCE) && !defined(__GNUC__)
#define BSWAP(a) __asm mov eax,a __asm bswap eax __asm mov a, eax
#elif defined(ARCH_X86) && (defined(DJGPP) || defined(__GNUC__))
#define BSWAP(a) __asm__ ( "bswapl %0\n" : "=r" (a) : "0" (a) )
#else
#define BSWAP(a) \
    ((a) = ( ((a)&0xff)<<24) | (((a)&0xff00)<<8) | (((a)>>8)&0xff00) | (((a)>>24)&0xff))
#endif

static uint32_t bitmask[] = {
    0x0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF, 0x1FF,
    0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
    0x1FFFF, 0x3FFFF, 0x7FFFF, 0xFFFFF, 0x1FFFFF, 0x3FFFFF,
    0x7FFFFF, 0xFFFFFF, 0x1FFFFFF, 0x3FFFFFF, 0x7FFFFFF,
    0xFFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF
};

void faad_initbits(bitfile *ld, void *buffer, uint32_t buffer_size);
void faad_endbits(bitfile *ld);
void faad_initbits_rev(bitfile *ld, void *buffer,
                       uint32_t bits_in_buffer);
uint8_t faad_byte_align(bitfile *ld);
uint32_t faad_get_processed_bits(bitfile *ld);
void faad_rewindbits(bitfile *ld);
uint8_t *faad_getbitbuffer(bitfile *ld, uint32_t bits
                       DEBUGDEC);

/* circumvent memory alignment errors on ARM */
static INLINE uint32_t getdword(void *mem)
{
#ifdef ARM
    uint32_t tmp;
    ((uint8_t*)&tmp)[0] = ((uint8_t*)mem)[0];
    ((uint8_t*)&tmp)[1] = ((uint8_t*)mem)[1];
    ((uint8_t*)&tmp)[2] = ((uint8_t*)mem)[2];
    ((uint8_t*)&tmp)[3] = ((uint8_t*)mem)[3];

    return tmp;
#else
    return *(uint32_t*)mem;
#endif
}

static INLINE uint32_t faad_showbits(bitfile *ld, uint32_t bits)
{
    if (bits <= ld->bits_left)
    {
        return (ld->bufa >> (ld->bits_left - bits)) & bitmask[bits];
    } else {
        bits -= ld->bits_left;
        return ((ld->bufa & bitmask[ld->bits_left]) << bits) | (ld->bufb >> (32 - bits));
    }
}

static INLINE void faad_flushbits(bitfile *ld, uint32_t bits)
{
    /* do nothing if error */
    if (ld->error != 0)
        return;

    if (bits < ld->bits_left)
    {
        ld->bits_left -= bits;
    } else {
        uint32_t tmp;

        ld->bufa = ld->bufb;
        tmp = getdword(ld->tail);
        ld->tail++;
#ifndef ARCH_IS_BIG_ENDIAN
        BSWAP(tmp);
#endif
        ld->bufb = tmp;
        ld->bits_left += (32 - bits);
        ld->bytes_used += 4;
        if (ld->bytes_used == ld->buffer_size)
            ld->no_more_reading = 1;
        if (ld->bytes_used > ld->buffer_size)
            ld->error = 1;
    }
}

/* return next n bits (right adjusted) */
static INLINE uint32_t faad_getbits(bitfile *ld, uint32_t n DEBUGDEC)
{
    uint32_t ret;

    if (ld->no_more_reading)
        return 0;

    if (n == 0)
        return 0;

    ret = faad_showbits(ld, n);
    faad_flushbits(ld, n);

#ifdef ANALYSIS
    if (print)
        fprintf(stdout, "%4d %2d bits, val: %4d, variable: %d %s\n", dbg_count++, n, ret, var, dbg);
#endif

    return ret;
}

static INLINE uint8_t faad_get1bit(bitfile *ld DEBUGDEC)
{
    uint8_t r;

    if (ld->bits_left == 0)
        return (uint8_t)faad_getbits(ld, 1 DEBUGVAR(print,var,dbg));

    ld->bits_left--;
    r = (uint8_t)((ld->bufa >> ld->bits_left) & 1);

    return r;
}

/* reversed bitreading routines */
static INLINE uint32_t faad_showbits_rev(bitfile *ld, uint32_t bits)
{
    uint8_t i;
    uint32_t B = 0;

    if (bits <= ld->bits_left)
    {
        for (i = 0; i < bits; i++)
        {
            if (ld->bufa & (1 << (i + (32 - ld->bits_left))))
                B |= (1 << (bits - i - 1));
        }
        return B;
    } else {
        for (i = 0; i < ld->bits_left; i++)
        {
            if (ld->bufa & (1 << (i + (32 - ld->bits_left))))
                B |= (1 << (bits - i - 1));
        }
        for (i = 0; i < bits - ld->bits_left; i++)
        {
            if (ld->bufb & (1 << (i + (32-ld->bits_left))))
                B |= (1 << (bits - ld->bits_left - i - 1));
        }
        return B;
    }
}

static INLINE void faad_flushbits_rev(bitfile *ld, uint32_t bits)
{
    /* do nothing if error */
    if (ld->error != 0)
        return;

    if (bits < ld->bits_left)
    {
        ld->bits_left -= bits;
    } else {
        uint32_t tmp;

        ld->bufa = ld->bufb;
        tmp = getdword(ld->start);
#ifndef ARCH_IS_BIG_ENDIAN
        BSWAP(tmp);
#endif
        ld->bufb = tmp;
        ld->start--;
        ld->bits_left += (32 - bits);

        ld->bytes_used += 4;
        if (ld->bytes_used == ld->buffer_size)
            ld->no_more_reading = 1;
        if (ld->bytes_used > ld->buffer_size)
            ld->error = 1;
    }
}

static INLINE uint32_t faad_getbits_rev(bitfile *ld, uint32_t n
                                        DEBUGDEC)
{
    uint32_t ret;

    if (ld->no_more_reading)
        return 0;

    if (n == 0)
        return 0;

    ret = faad_showbits_rev(ld, n);
    faad_flushbits_rev(ld, n);

#ifdef ANALYSIS
    if (print)
        fprintf(stdout, "%4d %2d bits, val: %4d, variable: %d %s\n", dbg_count++, n, ret, var, dbg);
#endif

    return ret;
}


#ifdef __cplusplus
}
#endif
#endif
