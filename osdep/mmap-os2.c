/*
 * very simple implementation of mmap() for OS/2
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

#define INCL_DOS
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys/types.h>

#include "config.h"
#include "mmap.h"
#include "mmap_anon.h"

typedef struct os2_mmap_s
{
    void    *addr;
    size_t  len;
    int     flags;
    struct os2_mmap_s *prev;
    struct os2_mmap_s *next;
} os2_mmap;
static os2_mmap *m_mmap = NULL;

void *mmap( void *addr, size_t len, int prot, int flags, int fildes, off_t off )
{
    os2_mmap *new_mmap;

    ULONG fl;
    ULONG rc;

    void  *ret;

    if( prot & PROT_WRITE )
    {
        if( flags & MAP_SHARED )
            return MAP_FAILED;

        if( !( flags & MAP_PRIVATE ))
            return MAP_FAILED;
    }

    if( flags & MAP_FIXED )
    {
        ULONG cb;

        cb = len;
        rc = DosQueryMem( addr, &cb, &fl );
        if( rc || ( cb < len ))
            return MAP_FAILED;

        rc = DosSetMem( addr, len, fPERM );
        if( rc )
            return MAP_FAILED;

        ret = addr;
    }
    else
    {
        // Allocate tiled memory compatible with 16-bit selectors
        // 'fs_seg' in 'ldt_keeper.c' need this attribute
        rc = DosAllocMem( &ret, len, fALLOC );
        if( rc )
            return MAP_FAILED;
    }

    new_mmap = ( os2_mmap * )malloc( sizeof( os2_mmap ));
    new_mmap->addr  = ret;
    new_mmap->len   = len;
    new_mmap->flags = flags;
    new_mmap->prev  = m_mmap;
    new_mmap->next  = NULL;

    if( m_mmap )
        m_mmap->next = new_mmap;
    m_mmap = new_mmap;

    if( !( flags & MAP_ANON ))
    {
        int pos;

        /* Now read in the file */
        if(( pos = lseek( fildes, off, SEEK_SET )) == -1)
        {
            munmap( ret, len );

            return MAP_FAILED;
        }

        read( fildes, ret, len );
        lseek( fildes, pos, SEEK_SET );  /* Restore the file pointer */
    }

    fl = 0;

    if( prot & PROT_READ )
        fl |= PAG_READ;

    if( prot & PROT_WRITE )
        fl |= PAG_WRITE;

    if( prot & PROT_EXEC )
        fl |= PAG_EXECUTE;

    if( prot & PROT_NONE )
        fl |= PAG_GUARD;

    rc = DosSetMem( ret, len, fl );
    if( rc )
    {
        munmap( ret, len );

        return MAP_FAILED;
    }

    return ret;
}

int munmap( void *addr, size_t len )
{
    os2_mmap *mm;

    for( mm = m_mmap; mm; mm = mm->prev )
    {
        if( mm->addr == addr )
            break;
    }

    if( mm )
    {

        if( !( mm->flags & MAP_FIXED ))
            DosFreeMem( addr );

        if( mm->next )
            mm->next->prev = mm->prev;

        if( mm->prev )
            mm->prev->next = mm->next;

        if( m_mmap == mm )
            m_mmap = mm->prev;

        free( mm );

        return 0;
    }

    return -1;
}

int mprotect( void *addr, size_t len, int prot )
{
    os2_mmap *mm;

    for( mm = m_mmap; mm; mm = mm->prev )
    {
        if( mm->addr == addr )
            break;
    }

    if( mm )
    {
        ULONG fl;

        fl = 0;

        if( prot & PROT_READ )
            fl |= PAG_READ;

        if( prot & PROT_WRITE )
            fl |= PAG_WRITE;

        if( prot & PROT_EXEC )
            fl |= PAG_EXECUTE;

        if( prot & PROT_NONE )
            fl |= PAG_GUARD;

        if( DosSetMem( addr, len, fl ) == 0 )
            return 0;
    }

    return -1;
}

void *mmap_anon( void *addr, size_t len, int prot, int flags, off_t off )
{
    return mmap( addr, len, prot, flags | MAP_ANON, -1, off );
}
