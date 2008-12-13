/*
 * CPP macros to deal with different symbol mangling across binary formats.
 *
 * Copyright (C) 2002 Felix Buenemann <atmosfear at users.sourceforge.net>
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

#ifndef MPLAYER_MANGLE_H
#define MPLAYER_MANGLE_H

#if (__GNUC__ * 100 + __GNUC_MINOR__ >= 300)
#define attribute_used __attribute__((used))
#else
#define attribute_used
#endif

/* Feel free to add more to the list, eg. a.out IMO */
#if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__OS2__) || \
   (defined(__OpenBSD__) && !defined(__ELF__)) || defined(__APPLE__)
#define MANGLE(a) "_" #a
#else
#define MANGLE(a) #a
#endif

#endif /* MPLAYER_MANGLE_H */
