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

/* Include file for mplayer specific defines and includes */
#ifndef MPLAYER_AF_MP_H
#define MPLAYER_AF_MP_H

#include "config.h"
#include "mp_msg.h"
#include "cpudetect.h"

/* Set the initialization type from mplayers cpudetect */
#ifdef AF_INIT_TYPE
#undef AF_INIT_TYPE
#define AF_INIT_TYPE \
  ((gCpuCaps.has3DNow || gCpuCaps.hasSSE)?AF_INIT_FAST:AF_INIT_SLOW)
#endif 

#ifdef af_msg
#undef af_msg
#endif
#define af_msg(lev, args... ) \
  mp_msg(MSGT_AFILTER,(((lev)<0)?((lev)+3):(((lev)==0)?MSGL_INFO:((lev)+5))), ##args )

#endif /* MPLAYER_AF_MP_H */
