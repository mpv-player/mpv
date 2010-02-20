/*
 * implementation of '-priority' for OS/2 and Win32
 *
 * Copyright (c) 2009 by KO Myung-Hun (komh@chollian.net)
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

#ifdef __OS2__

#define INCL_DOS
#include <os2.h>

#define REALTIME_PRIORITY_CLASS     MAKESHORT(0, PRTYC_TIMECRITICAL)
#define HIGH_PRIORITY_CLASS         MAKESHORT(PRTYD_MAXIMUM, PRTYC_REGULAR)
#define ABOVE_NORMAL_PRIORITY_CLASS MAKESHORT(PRTYD_MAXIMUM / 2, PRTYC_REGULAR)
#define NORMAL_PRIORITY_CLASS       MAKESHORT(0, PRTYC_REGULAR)
#define BELOW_NORMAL_PRIORITY_CLASS MAKESHORT(PRTYD_MAXIMUM, PRTYC_IDLETIME)
#define IDLE_PRIORITY_CLASS         MAKESHORT(0, PRTYC_IDLETIME)

#else

#include <windows.h>

#endif /* __OS2__ */

#include <string.h>

#include "mp_msg.h"
#include "help_mp.h"

#include "priority.h"

char *proc_priority = NULL;

void set_priority(void)
{
    struct {
        char* name;
        int prio;
    } priority_presets_defs[] = {
        { "realtime", REALTIME_PRIORITY_CLASS},
        { "high", HIGH_PRIORITY_CLASS},
#ifdef ABOVE_NORMAL_PRIORITY_CLASS
        { "abovenormal", ABOVE_NORMAL_PRIORITY_CLASS},
#endif
        { "normal", NORMAL_PRIORITY_CLASS},
#ifdef BELOW_NORMAL_PRIORITY_CLASS
        { "belownormal", BELOW_NORMAL_PRIORITY_CLASS},
#endif
        { "idle", IDLE_PRIORITY_CLASS},
        { NULL, NORMAL_PRIORITY_CLASS} /* default */
    };

    if (proc_priority) {
        int i;

        for (i = 0; priority_presets_defs[i].name; i++) {
            if (strcasecmp(priority_presets_defs[i].name, proc_priority) == 0)
                break;
        }
        mp_msg(MSGT_CPLAYER, MSGL_STATUS, MSGTR_SettingProcessPriority,
               priority_presets_defs[i].name);

#ifdef __OS2__
        DosSetPriority(PRTYS_PROCESS,
                       HIBYTE(priority_presets_defs[i].prio),
                       LOBYTE(priority_presets_defs[i].prio),
                       0);
#else
        SetPriorityClass(GetCurrentProcess(), priority_presets_defs[i].prio);
#endif
    }
}
