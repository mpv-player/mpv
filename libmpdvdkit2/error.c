/*****************************************************************************
 * error.c: error management functions
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id$
 *
 * Author: Samuel Hocevar <sam@zoy.org>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_SYS_PARAM_H
#   include <sys/param.h>
#endif

#ifdef HAVE_LIMITS_H
#   include <limits.h>
#endif

#include "dvdcss/dvdcss.h"

#include "common.h"
#include "css.h"
#include "libdvdcss.h"

/*****************************************************************************
 * Error messages
 *****************************************************************************/
void _print_error( dvdcss_t dvdcss, char *psz_string )
{
    if( dvdcss->b_errors )
    {
        fprintf( stderr, "libdvdcss error: %s\n", psz_string );
    }

    dvdcss->psz_error = psz_string;
}

/*****************************************************************************
 * Debug messages
 *****************************************************************************/
#if 0
void _print_debug( dvdcss_t dvdcss, char *psz_string )
{
    if( dvdcss->b_debug )
    {
        fprintf( stderr, "libdvdcss debug: %s\n", psz_string );
    }
}
#endif

