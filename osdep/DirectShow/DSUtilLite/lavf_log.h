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

#pragma once

#define LOG_BUF_LEN 2048
inline void lavf_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    static int print_prefix = 1;
    static int count;
    static char line[LOG_BUF_LEN] = {0}, prev[LOG_BUF_LEN] = {0};

    if (level > AV_LOG_VERBOSE)
        return;

    av_log_format_line(ptr, level, fmt, vl, line, sizeof(line), &print_prefix);

    if (print_prefix && !strcmp(line, prev))
    {
        count++;
        return;
    }
    if (count > 0)
    {
        DbgLog((LOG_CUSTOM1, level, L"    Last message repeated %d times", count));
        count = 0;
    }
    size_t len = strnlen_s(line, LOG_BUF_LEN);
    if (len > 0 && line[len - 1] == '\n')
    {
        line[len - 1] = 0;
    }

    DbgLog((LOG_CUSTOM1, level, L"%S", line));
    strncpy_s(prev, line, _TRUNCATE);
}
