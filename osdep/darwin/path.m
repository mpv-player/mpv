/*
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

#import <Foundation/Foundation.h>
#include "options/path.h"
#include "osdep/path.h"

const char *mp_get_platform_path_osx(void *talloc_ctx, const char *type)
{
    if (strcmp(type, "osxbundle") == 0 && getenv("MPVBUNDLE")) {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSString *path = [[NSBundle mainBundle] resourcePath];
        char *res = talloc_strdup(talloc_ctx, [path UTF8String]);
        [pool release];
        return res;
    }
    if (strcmp(type, "desktop") == 0 && getenv("HOME"))
        return mp_path_join(talloc_ctx, getenv("HOME"), "Desktop");
    return NULL;
}
