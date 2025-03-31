/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <intrin.h>

#define START_TIMER \
    uint64_t tend;  \
    uint64_t tstart = __rdtsc();

#define STOP_TIMER(id)                                                                                              \
    tend = __rdtsc();                                                                                               \
    {                                                                                                               \
        static uint64_t tsum = 0;                                                                                   \
        static int tcount = 0;                                                                                      \
        static int tskip_count = 0;                                                                                 \
        if (tcount < 2 || tend - tstart < 8 * tsum / tcount || tend - tstart < 2000)                                \
        {                                                                                                           \
            tsum += tend - tstart;                                                                                  \
            tcount++;                                                                                               \
        }                                                                                                           \
        else                                                                                                        \
            tskip_count++;                                                                                          \
        if (((tcount + tskip_count) & (tcount + tskip_count - 1)) == 0)                                             \
        {                                                                                                           \
            debugprintf(L"%I64u decicycles in %S, %d runs, %d skips", tsum * 10 / tcount, id, tcount, tskip_count); \
        }                                                                                                           \
    }
