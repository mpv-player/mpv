/*
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

 /**
 * \file mmap_anon.c
 * \brief Provide a compatible anonymous space mapping function
 */
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "mmap_anon.h"

#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

/*
 * mmap() anonymous space, depending on the system's mmap() style. On systems
 * that use the /dev/zero mapping idiom, zerofd will be set to the file descriptor
 * of the opened /dev/zero.
 */

 /**
 * \brief mmap() anonymous space, depending on the system's mmap() style. On systems
 * that use the /dev/zero mapping idiom, zerofd will be set to the file descriptor
 * of the opened /dev/zero.
 *
 * \param addr address to map at.
 * \param len number of bytes from addr to be mapped.
 * \param prot protections (region accessibility).
 * \param flags specifies the type of the mapped object.
 * \param offset start mapping at byte offset.
 * \param zerofd
 * \return a pointer to the mapped region upon successful completion, -1 otherwise.
 */
void *mmap_anon(void *addr, size_t len, int prot, int flags, off_t offset)
{
    void *result;

     /* From loader/ext.c:
      * "Linux EINVAL's on us if we don't pass MAP_PRIVATE to an anon mmap"
      * Therefore we preserve the same behavior on all platforms, ie. no
      * shared mappings of anon space (if the concepts are supported). */
#if defined(MAP_SHARED) && defined(MAP_PRIVATE)
     flags = (flags & ~MAP_SHARED) | MAP_PRIVATE;
#endif /* defined(MAP_SHARED) && defined(MAP_PRIVATE) */

#ifdef MAP_ANONYMOUS
    /* BSD-style anonymous mapping */
    result = mmap(addr, len, prot, flags | MAP_ANONYMOUS, -1, offset);
#else
    /* SysV-style anonymous mapping */
    int fd;
    fd = open("/dev/zero", O_RDWR);
    if(fd < 0){
        perror( "Cannot open /dev/zero for READ+WRITE. Check permissions! error: ");
        return NULL;
    }

    result = mmap(addr, len, prot, flags, fd, offset);
    close(fd);
#endif /* MAP_ANONYMOUS */

    return result;
}
