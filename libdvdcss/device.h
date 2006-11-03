/*****************************************************************************
 * device.h: DVD device access
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id$
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Håkan Hjort <d95hjort@dtek.chalmers.se>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * iovec structure: vectored data entry
 *****************************************************************************/
#if defined( WIN32 ) && !defined( SYS_CYGWIN )
#   include <io.h>                                                 /* read() */
#else
#   include <sys/types.h>
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#if defined( WIN32 ) && !defined( SYS_CYGWIN )
struct iovec
{
    void *iov_base;     /* Pointer to data. */
    size_t iov_len;     /* Length of data.  */
};
#endif

/*****************************************************************************
 * Device reading prototypes
 *****************************************************************************/
int  _dvdcss_use_ioctls ( dvdcss_t );
void _dvdcss_check      ( dvdcss_t );
int  _dvdcss_open       ( dvdcss_t );
int  _dvdcss_close      ( dvdcss_t );

/*****************************************************************************
 * Device reading prototypes, raw-device specific
 *****************************************************************************/
#ifndef WIN32
int _dvdcss_raw_open     ( dvdcss_t, char const * );
#endif

