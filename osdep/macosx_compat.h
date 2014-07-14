/*
 *  Application Event Handling
 *
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


#ifndef MPV_MACOSX_COMPAT
#define MPV_MACOSX_COMPAT

#import <Cocoa/Cocoa.h>
#include "osdep/macosx_versions.h"

#if (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8)
@interface NSArray (SubscriptingAdditions)
- (id)objectAtIndexedSubscript:(NSUInteger)index;
@end

@interface NSMutableArray (SubscriptingAdditions)
- (void)setObject: (id)object atIndexedSubscript:(NSUInteger)index;
@end

@interface NSDictionary (SubscriptingAdditions)
- (id)objectForKeyedSubscript:(id)key;
@end

@interface NSMutableDictionary (SubscriptingAdditions)
- (void)setObject: (id)object forKeyedSubscript:(id)key;
@end

#if __has_feature(objc_bool)
    #define YES  __objc_yes
    #define NO   __objc_no
#else
    #define YES  ((BOOL)1)
    #define NO   ((BOOL)0)
#endif

#endif

#endif /* MPV_MACOSX_COMPAT */
