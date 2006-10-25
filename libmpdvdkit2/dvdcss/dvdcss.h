/**
 * \file dvdcss.h
 * \author Stéphane Borel <stef@via.ecp.fr>
 * \author Samuel Hocevar <sam@zoy.org>
 * \brief The \e libdvdcss public header.
 *
 * This header contains the public types and functions that applications
 * using \e libdvdcss may use.
 */

/*
 * Copyright (C) 1998-2002 VideoLAN
 * $Id$
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
 */

#ifndef _DVDCSS_DVDCSS_H
#ifndef _DOXYGEN_SKIP_ME
#define _DVDCSS_DVDCSS_H 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Library instance handle, to be used for each library call. */
typedef struct dvdcss_s* dvdcss_t;


/** The block size of a DVD. */
#define DVDCSS_BLOCK_SIZE      2048

/** The default flag to be used by \e libdvdcss functions. */
#define DVDCSS_NOFLAGS         0

/** Flag to ask dvdcss_read() to decrypt the data it reads. */
#define DVDCSS_READ_DECRYPT    (1 << 0)

/** Flag to tell dvdcss_seek() it is seeking in MPEG data. */
#define DVDCSS_SEEK_MPEG       (1 << 0)

/** Flag to ask dvdcss_seek() to check the current title key. */
#define DVDCSS_SEEK_KEY        (1 << 1)


#if defined(LIBDVDCSS_EXPORTS)
#define LIBDVDCSS_EXPORT __declspec(dllexport) extern
#elif defined(LIBDVDCSS_IMPORTS)
#define LIBDVDCSS_EXPORT __declspec(dllimport) extern
#else
#define LIBDVDCSS_EXPORT extern
#endif

/*
 * Our version number. The variable name contains the interface version.
 */
LIBDVDCSS_EXPORT char *        dvdcss_interface_2;


/*
 * Exported prototypes.
 */
LIBDVDCSS_EXPORT dvdcss_t dvdcss_open  ( char *psz_target );
LIBDVDCSS_EXPORT int      dvdcss_close ( dvdcss_t );
LIBDVDCSS_EXPORT int      dvdcss_seek  ( dvdcss_t,
                               int i_blocks,
                               int i_flags );
LIBDVDCSS_EXPORT int      dvdcss_read  ( dvdcss_t,
                               void *p_buffer,
                               int i_blocks,
                               int i_flags );
LIBDVDCSS_EXPORT int      dvdcss_readv ( dvdcss_t,
                               void *p_iovec,
                               int i_blocks,
                               int i_flags );
LIBDVDCSS_EXPORT char *   dvdcss_error ( dvdcss_t );


/*
 * Deprecated stuff.
 */
#ifndef _DOXYGEN_SKIP_ME
#define dvdcss_title(a,b) dvdcss_seek(a,b,DVDCSS_SEEK_KEY)
#define dvdcss_handle dvdcss_t
#endif


#ifdef __cplusplus
}
#endif

#endif /* <dvdcss/dvdcss.h> */
