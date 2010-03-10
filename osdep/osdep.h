/*
 * Header in order to include OS-specific headers, macros, types and so on
 *
 * Copyright (c) 2010 by KO Myung-Hun (komh@chollian.net)
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

#ifndef MPLAYER_OSDEP_H
#define MPLAYER_OSDEP_H

#ifdef __OS2__
#define INCL_DOS
#define INCL_DOSDEVIOCTL
#include <os2.h>

#include <process.h>    /* getpid() */

#define REALTIME_PRIORITY_CLASS     MAKESHORT(0, PRTYC_TIMECRITICAL)
#define HIGH_PRIORITY_CLASS         MAKESHORT(PRTYD_MAXIMUM, PRTYC_REGULAR)
#define ABOVE_NORMAL_PRIORITY_CLASS MAKESHORT(PRTYD_MAXIMUM / 2, PRTYC_REGULAR)
#define NORMAL_PRIORITY_CLASS       MAKESHORT(0, PRTYC_REGULAR)
#define BELOW_NORMAL_PRIORITY_CLASS MAKESHORT(PRTYD_MAXIMUM, PRTYC_IDLETIME)
#define IDLE_PRIORITY_CLASS         MAKESHORT(0, PRTYC_IDLETIME)

#define SetPriorityClass(pid, prio) \
            DosSetPriority(PRTYS_PROCESS, \
                           HIBYTE(prio), \
                           LOBYTE(prio), \
                           pid)

#define GetCurrentProcess() getpid()
#endif /* __OS2__ */

#endif /* MPLAYER_OSDEP_H */

