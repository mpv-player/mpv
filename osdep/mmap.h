/*
 * mmap declaration header for systems with missing/nonfunctional sys/mman.h
 *
 * Copyright (c) 2008 KO Myung-Hun (komh@chollian.net)
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_MMAP_H
#define MPLAYER_MMAP_H

#include <sys/types.h>

/*
 * Protections are chosen from these bits, or-ed together
 */
#define PROT_NONE   0x00 /* no permissions */
#define PROT_READ   0x01 /* pages can be read */
#define PROT_WRITE  0x02 /* pages can be written */
#define PROT_EXEC   0x04 /* pages can be executed */

/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
#define MAP_SHARED  0x0001  /* share changes */
#define MAP_PRIVATE 0x0002  /* changes are private */
#define MAP_FIXED   0x0010  /* map addr must be exactly as requested */

/*
 * Mapping type
 */
#define MAP_ANON    0x1000  /* allocated from memory, swap space */

/* MAP_FAILED is defined in config.h */

#ifndef _MMAP_DECLARED
#define _MMAP_DECLARED
void *mmap( void *addr, size_t len, int prot, int flags, int fildes, off_t off );
#endif
int   munmap( void *addr, size_t len );
int   mprotect( void *addr, size_t len, int prot );

#endif /* MPLAYER_MMAP_H */
