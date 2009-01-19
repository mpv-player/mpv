/*
 * direct hardware access under Windows NT/2000/XP
 *
 * Copyright (c) 2004 Sascha Sommer <saschasommer@freenet.de>
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

#ifndef MPLAYER_DHAHELPER_H
#define MPLAYER_DHAHELPER_H

// Define the various device type values.  Note that values used by Microsoft
// Corporation are in the range 0-32767, and 32768-65535 are reserved for use
// by customers.

#define FILE_DEVICE_DHAHELPER 0x00008011

// Macro definition for defining IOCTL and FSCTL function control codes.
// Note that function codes 0-2047 are reserved for Microsoft Corporation,
// and 2048-4095 are reserved for customers.

#define DHAHELPER_IOCTL_INDEX 0x810

#define IOCTL_DHAHELPER_MAPPHYSTOLIN     CTL_CODE(FILE_DEVICE_DHAHELPER,     \
                                                  DHAHELPER_IOCTL_INDEX,     \
                                                  METHOD_BUFFERED,           \
                                                  FILE_ANY_ACCESS)

#define IOCTL_DHAHELPER_UNMAPPHYSADDR    CTL_CODE(FILE_DEVICE_DHAHELPER,     \
                                                  DHAHELPER_IOCTL_INDEX + 1, \
                                                  METHOD_BUFFERED,           \
                                                  FILE_ANY_ACCESS)

#define IOCTL_DHAHELPER_ENABLEDIRECTIO   CTL_CODE(FILE_DEVICE_DHAHELPER,     \
                                                  DHAHELPER_IOCTL_INDEX + 2, \
                                                  METHOD_BUFFERED,           \
                                                  FILE_ANY_ACCESS)

#define IOCTL_DHAHELPER_DISABLEDIRECTIO  CTL_CODE(FILE_DEVICE_DHAHELPER,     \
                                                  DHAHELPER_IOCTL_INDEX + 3, \
                                                  METHOD_BUFFERED,           \
                                                  FILE_ANY_ACCESS)


#if !defined(__MINGW32__) && !defined(__CYGWIN__)
#pragma pack(1)
typedef struct dhahelper_t {
#else
struct __attribute__((__packed__)) dhahelper_t {
#endif
  unsigned int size;
  void* base;
  void* ptr;
};

typedef struct dhahelper_t dhahelper_t;

#endif /* MPLAYER_DHAHELPER_H */
