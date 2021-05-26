/*
 * User language lookup for Apple platforms
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

#include "misc/language.h"

#include "apple_utils.h"
#include "mpv_talloc.h"

char **mp_get_user_langs(void)
{
    CFArrayRef arr = CFLocaleCopyPreferredLanguages();
    if (!arr)
        return NULL;
    CFIndex count = CFArrayGetCount(arr);
    if (!count)
        return NULL;

    char **ret = talloc_array_ptrtype(NULL, ret, count + 1);

    for (CFIndex i = 0; i < count; i++) {
        CFStringRef cfstr = CFArrayGetValueAtIndex(arr, i);
        ret[i] = talloc_steal(ret, cfstr_get_cstr(cfstr));
    }

    ret[count] = NULL;

    CFRelease(arr);
    return ret;
}
