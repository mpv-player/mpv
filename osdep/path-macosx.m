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
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#import <Foundation/Foundation.h>
#include "options/path.h"
#include "osdep/path.h"

int mp_add_macosx_bundle_dir(struct mpv_global *global, char **dirs, int i)
{
    void *talloc_ctx = dirs;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *path = [[NSBundle mainBundle] resourcePath];
    dirs[i++] = talloc_strdup(talloc_ctx, [path UTF8String]);
    [pool release];
    return i;
}
