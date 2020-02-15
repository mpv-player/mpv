/*
 * Cocoa Application Event Handling
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

#import <Cocoa/Cocoa.h>
#include "osdep/macosx_events.h"

@class RemoteCommandCenter;
struct input_ctx;

@interface EventsResponder : NSObject

+ (EventsResponder *)sharedInstance;
- (void)setInputContext:(struct input_ctx *)ctx;
- (void)setIsApplication:(BOOL)isApplication;

/// Blocks until inputContext is present.
- (void)waitForInputContext;
- (void)wakeup;
- (void)putKey:(int)keycode;
- (void)handleFilesArray:(NSArray *)files;

- (bool)queueCommand:(char *)cmd;
- (bool)processKeyEvent:(NSEvent *)event;

- (BOOL)handleMPKey:(int)key withMask:(int)mask;

@property(nonatomic, retain) RemoteCommandCenter *remoteCommandCenter;

@end
