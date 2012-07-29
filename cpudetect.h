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

#ifndef MPLAYER_CPUDETECT_H
#define MPLAYER_CPUDETECT_H

#include <stdbool.h>
#include "config.h"

#define CPUTYPE_I386    3
#define CPUTYPE_I486    4
#define CPUTYPE_I586    5
#define CPUTYPE_I686    6

#include "ffmpeg_files/x86_cpu.h"

typedef struct cpucaps_s {
    int cpuType;
    int cpuStepping;
    bool isX86;
    bool hasMMX;
    bool hasMMX2;
    bool has3DNow;
    bool has3DNowExt;
    bool hasSSE;
    bool hasSSE2;
    bool hasSSE3;
    bool hasSSSE3;
} CpuCaps;

extern CpuCaps gCpuCaps;

void do_cpuid(unsigned int ax, unsigned int *p);

void GetCpuCaps(CpuCaps *caps);

/* returned value is malloc()'ed so free() it after use */
char *GetCpuFriendlyName(unsigned int regs[], unsigned int regs2[]);

#endif /* MPLAYER_CPUDETECT_H */
