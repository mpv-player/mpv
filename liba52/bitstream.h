/*
 * bitstream.h
 * Copyright (C) 2000-2001 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * a52dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * a52dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* code from ffmpeg/libavcodec */
#if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC_ == 3 && __GNUC_MINOR__ > 0)
#    define always_inline __attribute__((always_inline)) inline
#else
#    define always_inline inline
#endif

#if defined(__sparc__) || defined(hpux)
/*
 * the alt bitstream reader performs unaligned memory accesses; that doesn't work
 * on sparc/hpux.  For now, disable ALT_BITSTREAM_READER.
 */
#undef	ALT_BITSTREAM_READER
#else
// alternative (faster) bitstram reader (reades upto 3 bytes over the end of the input)
#define ALT_BITSTREAM_READER

/* used to avoid missaligned exceptions on some archs (alpha, ...) */
#if defined (ARCH_X86) || defined(ARCH_ARMV4L)
#    define unaligned32(a) (*(uint32_t*)(a))
#else
#    ifdef __GNUC__
static always_inline uint32_t unaligned32(const void *v) {
    struct Unaligned {
	uint32_t i;
    } __attribute__((packed));

    return ((const struct Unaligned *) v)->i;
}
#    elif defined(__DECC)
static inline uint32_t unaligned32(const void *v) {
    return *(const __unaligned uint32_t *) v;
}
#    else
static inline uint32_t unaligned32(const void *v) {
    return *(const uint32_t *) v;
}
#    endif
#endif //!ARCH_X86

#endif
 
/* (stolen from the kernel) */
#ifdef WORDS_BIGENDIAN

#	define swab32(x) (x)

#else

#	if defined (__i386__)

#	define swab32(x) __i386_swab32(x)
	static always_inline const uint32_t __i386_swab32(uint32_t x)
	{
		__asm__("bswap %0" : "=r" (x) : "0" (x));
		return x;
	}

#	else

#	define swab32(x) __generic_swab32(x)
	static always_inline const uint32_t __generic_swab32(uint32_t x)
	{
		return ((((uint8_t*)&x)[0] << 24) | (((uint8_t*)&x)[1] << 16) |
		 (((uint8_t*)&x)[2] << 8)  | (((uint8_t*)&x)[3]));
	}
#	endif
#endif

#ifdef ALT_BITSTREAM_READER
extern uint32_t *buffer_start; 
extern int indx;
#else
extern uint32_t bits_left;
extern uint32_t current_word;
#endif

void bitstream_set_ptr (uint8_t * buf);
uint32_t bitstream_get_bh(uint32_t num_bits);
int32_t bitstream_get_bh_2(uint32_t num_bits);


static inline uint32_t 
bitstream_get(uint32_t num_bits) // note num_bits is practically a constant due to inlineing
{
#ifdef ALT_BITSTREAM_READER
    uint32_t result= swab32( unaligned32(((uint8_t *)buffer_start)+(indx>>3)) );

    result<<= (indx&0x07);
    result>>= 32 - num_bits;
    indx+= num_bits;
    
    return result;
#else
    uint32_t result;
    
    if(num_bits < bits_left) {
	result = (current_word << (32 - bits_left)) >> (32 - num_bits);
	bits_left -= num_bits;
	return result;
    }

    return bitstream_get_bh(num_bits);
#endif
}

static inline void bitstream_skip(int num_bits)
{
#ifdef ALT_BITSTREAM_READER
	indx+= num_bits;
#else
	bitstream_get(num_bits);
#endif
}

static inline int32_t 
bitstream_get_2(uint32_t num_bits)
{
#ifdef ALT_BITSTREAM_READER
    int32_t result= swab32( unaligned32(((uint8_t *)buffer_start)+(indx>>3)) );

    result<<= (indx&0x07);
    result>>= 32 - num_bits;
    indx+= num_bits;
        
    return result;
#else
    int32_t result;
	
    if(num_bits < bits_left) {
	result = (((int32_t)current_word) << (32 - bits_left)) >> (32 - num_bits);
	bits_left -= num_bits;
	return result;
    }

    return bitstream_get_bh_2(num_bits);
#endif
}
