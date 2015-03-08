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

@interface MpvCocoaAdapter : NSObject<NSWindowDelegate>
- (void)setNeedsResize;
- (void)signalMouseMovement:(NSPoint)point;
- (void)putKeyEvent:(NSEvent*)event;
- (void)putKey:(int)mpkey withModifiers:(int)modifiers;
- (void)putAxis:(int)mpkey delta:(float)delta;
- (void)putCommand:(char*)cmd;
- (void)handleFilesArray:(NSArray *)files;
- (void)didChangeWindowedScreenProfile:(NSScreen *)screen;
- (void)performAsyncResize:(NSSize)size;
- (void)didChangeMousePosition;

- (BOOL)isInFullScreenMode;
- (BOOL)keyboardEnabled;
- (BOOL)mouseEnabled;
- (NSScreen *)fsScreen;
- (BOOL)fsModeAllScreens;
@property(nonatomic, assign) struct vo *vout;
@end
