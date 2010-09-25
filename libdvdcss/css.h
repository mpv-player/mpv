/*****************************************************************************
 * css.h: Structures for DVD authentication and unscrambling
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id$
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *
 * based on:
 *  - css-auth by Derek Fawcus <derek@spider.com>
 *  - DVD CSS ioctls example program by Andrew T. Veliath <andrewtv@usa.net>
 *  - DeCSSPlus by Ethan Hawke
 *  - The Divide and conquer attack by Frank A. Stevenson <frank@funcom.com>
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
 * You should have received a copy of the GNU General Public License along
 * with libdvdcss; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *****************************************************************************/

#ifndef DVDCSS_CSS_H
#define DVDCSS_CSS_H

#include "dvdcss/dvdcss.h"

#define KEY_SIZE 5

typedef uint8_t dvd_key_t[KEY_SIZE];

typedef struct dvd_title_s
{
    int                 i_startlb;
    dvd_key_t           p_key;
    struct dvd_title_s *p_next;
} dvd_title_t;

typedef struct css_s
{
    int             i_agid;      /* Current Authenication Grant ID. */
    dvd_key_t       p_bus_key;   /* Current session key. */
    dvd_key_t       p_disc_key;  /* This DVD disc's key. */
    dvd_key_t       p_title_key; /* Current title key. */
} css_t;

/*****************************************************************************
 * Prototypes in css.c
 *****************************************************************************/
void  _dvdcss_test        ( dvdcss_t );
int   _dvdcss_title       ( dvdcss_t, int );
int   _dvdcss_disckey     ( dvdcss_t );
int   _dvdcss_titlekey    ( dvdcss_t, int , dvd_key_t );
int   _dvdcss_unscramble  ( uint8_t *, uint8_t * );

#endif /* DVDCSS_CSS_H */
