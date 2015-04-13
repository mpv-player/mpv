/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "ta_talloc.h"

char *ta_talloc_strdup_append(char *s, const char *a)
{
    ta_xstrdup_append(&s, a);
    return s;
}

char *ta_talloc_strdup_append_buffer(char *s, const char *a)
{
    ta_xstrdup_append_buffer(&s, a);
    return s;
}

char *ta_talloc_strndup_append(char *s, const char *a, size_t n)
{
    ta_xstrndup_append(&s, a, n);
    return s;
}

char *ta_talloc_strndup_append_buffer(char *s, const char *a, size_t n)
{
    ta_xstrndup_append_buffer(&s, a, n);
    return s;
}

char *ta_talloc_vasprintf_append(char *s, const char *fmt, va_list ap)
{
    ta_xvasprintf_append(&s, fmt, ap);
    return s;
}

char *ta_talloc_vasprintf_append_buffer(char *s, const char *fmt, va_list ap)
{
    ta_xvasprintf_append_buffer(&s, fmt, ap);
    return s;
}

char *ta_talloc_asprintf_append(char *s, const char *fmt, ...)
{
    char *res;
    va_list ap;
    va_start(ap, fmt);
    res = talloc_vasprintf_append(s, fmt, ap);
    va_end(ap);
    return res;
}

char *ta_talloc_asprintf_append_buffer(char *s, const char *fmt, ...)
{
    char *res;
    va_list ap;
    va_start(ap, fmt);
    res = talloc_vasprintf_append_buffer(s, fmt, ap);
    va_end(ap);
    return res;
}
