/*
 * Apple-specific utility functions
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils-mac.h"

#include "mpv_talloc.h"

CFStringRef cfstr_from_cstr(const char *str)
{
    return CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
}

char *cfstr_get_cstr(const CFStringRef cfstr)
{
    if (!cfstr)
        return NULL;
    CFIndex size =
        CFStringGetMaximumSizeForEncoding(
            CFStringGetLength(cfstr), kCFStringEncodingUTF8) + 1;
    char *buffer = talloc_zero_size(NULL, size);
    CFStringGetCString(cfstr, buffer, size, kCFStringEncodingUTF8);
    return buffer;
}
