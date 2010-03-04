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

#include "osdep.h"

#ifdef _WIN32
#include <windows.h>
#endif

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

        SetPriorityClass(GetCurrentProcess(), priority_presets_defs[i].prio);
    }
}
