/*****************************************************************************
 * private.h: private DVD reading library data
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id$
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
 * The libdvdcss structure
 *****************************************************************************/
struct dvdcss_s
{
    /* File descriptor */
    char * psz_device;
    int    i_fd;
    int    i_seekpos;

    /* Decryption stuff */
    int          i_method;
    css_t        css;
    int          b_ioctls;
    int          b_scrambled;
    dvd_title_t *p_titles;
    char *	 psz_cache;

    /* Error management */
    char * psz_error;
    int    b_errors;
    int    b_debug;

#ifdef WIN32
    char * p_readv_buffer;
    int    i_readv_buf_size;
#endif

#ifndef WIN32
    int    i_raw_fd;
    int    i_read_fd;
#endif
};

/*****************************************************************************
 * libdvdcss method: used like init flags
 *****************************************************************************/
#define DVDCSS_METHOD_KEY        0
#define DVDCSS_METHOD_DISC       1
#define DVDCSS_METHOD_TITLE      2

/*****************************************************************************
 * Functions used across the library
 *****************************************************************************/
int  _dvdcss_seek  ( dvdcss_t, int );
int  _dvdcss_read  ( dvdcss_t, void *, int );

void _dvdcss_error ( dvdcss_t, char * );
void _dvdcss_debug ( dvdcss_t, char * );

