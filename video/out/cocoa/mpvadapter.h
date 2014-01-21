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

#import <Cocoa/Cocoa.h>
#include "video/out/vo.h"

@interface MpvCocoaAdapter : NSObject
- (void)setNeedsResize;
- (void)signalMouseMovement:(NSPoint)point;
- (void)putKey:(int)mpkey withModifiers:(int)modifiers;
- (void)putAxis:(int)mpkey delta:(float)delta;
- (void)putCommand:(char*)cmd;
- (void)performAsyncResize:(NSSize)size;
- (void)handleFilesArray:(NSArray *)files;

- (BOOL)isInFullScreenMode;
- (NSScreen *)fsScreen;
@property(nonatomic, assign) struct vo *vout;
@end
