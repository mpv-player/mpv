#ifndef BSWAP_H_INCLUDED
#define BSWAP_H_INCLUDED

/*
 * Copyright (C) 2000, 2001 Billy Biggs <vektor@dumbterm.net>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <config.h>

#if defined(WORDS_BIGENDIAN)
/* All bigendian systems are fine, just ignore the swaps. */  
#define B2N_16(x) (void)(x)
#define B2N_32(x) (void)(x)
#define B2N_64(x) (void)(x)

#else 

#if defined(__linux__)
#include <byteswap.h>
#define B2N_16(x) x = bswap_16(x)
#define B2N_32(x) x = bswap_32(x)
#define B2N_64(x) x = bswap_64(x)

#elif defined(__NetBSD__)
#include <sys/endian.h>
#define B2N_16(x) BE16TOH(x)
#define B2N_32(x) BE32TOH(x)
#define B2N_64(x) BE64TOH(x)

#elif defined(__OpenBSD__)
#include <sys/endian.h>
#define B2N_16(x) x = swap16(x)
#define B2N_32(x) x = swap32(x)
#define B2N_64(x) x = swap64(x)

/* This is a slow but portable implementation, it has multiple evaluation 
 * problems so beware.
 * FreeBSD and Solaris don't have <byteswap.h> or any other such 
 * functionality! 
 */

#elif defined(__FreeBSD__) || defined(__sun) || defined(__bsdi__)
#define B2N_16(x) \
 x = ((((x) & 0xff00) >> 8) | \
      (((x) & 0x00ff) << 8))
#define B2N_32(x) \
 x = ((((x) & 0xff000000) >> 24) | \
      (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | \
      (((x) & 0x000000ff) << 24))
#define B2N_64(x) \
 x = ((((x) & 0xff00000000000000) >> 56) | \
      (((x) & 0x00ff000000000000) >> 40) | \
      (((x) & 0x0000ff0000000000) >> 24) | \
      (((x) & 0x000000ff00000000) >>  8) | \
      (((x) & 0x00000000ff000000) <<  8) | \
      (((x) & 0x0000000000ff0000) << 24) | \
      (((x) & 0x000000000000ff00) << 40) | \
      (((x) & 0x00000000000000ff) << 56))

#else

/* If there isn't a header provided with your system with this functionality
 * add the relevant || define( ) to the portable implementation above.
 */
#error "You need to add endian swap macros for you're system"

#endif

#endif /* WORDS_BIGENDIAN */

#endif /* BSWAP_H_INCLUDED */
