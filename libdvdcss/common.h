/*****************************************************************************
 * common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Basic types definitions
 *****************************************************************************/
#if defined( HAVE_STDINT_H )
#   include <stdint.h>
#elif defined( HAVE_INTTYPES_H )
#   include <inttypes.h>
#elif defined( SYS_CYGWIN )
#   include <sys/types.h>
    /* Cygwin only defines half of these... */
    typedef u_int8_t            uint8_t;
    typedef u_int32_t           uint32_t;
#else
    /* Fallback types (very x86-centric, sorry) */
    typedef unsigned char       uint8_t;
    typedef signed char         int8_t;
    typedef unsigned int        uint32_t;
    typedef signed int          int32_t;
#endif

#if defined( WIN32 )

#   ifndef PATH_MAX
#      define PATH_MAX MAX_PATH
#   endif

/* several type definitions */
#   if defined( __MINGW32__ )
#       define lseek _lseeki64
#       if !defined( _OFF_T_ )
typedef long long _off_t;
typedef _off_t off_t;
#           define _OFF_T_
#       else
#           define off_t long long
#       endif
#   endif

#   if defined( _MSC_VER )
#       define lseek _lseeki64
#       if !defined( _OFF_T_DEFINED )
typedef __int64 off_t;
#           define _OFF_T_DEFINED
#       else
#           define off_t __int64
#       endif
#       define stat _stati64
#   endif

#   ifndef snprintf
#       define snprintf _snprintf  /* snprintf not defined in mingw32 (bug?) */
#   endif

#endif

