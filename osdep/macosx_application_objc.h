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

#import <Cocoa/Cocoa.h>
#include "osdep/macosx_application.h"
#import "ar/HIDRemote.h"

struct cocoa_input_queue;

@interface InputQueue : NSObject
- (void)push:(int)keycode;
- (int) pop;
@end

@interface EventsResponder : NSObject <HIDRemoteDelegate>
- (BOOL)handleMediaKey:(NSEvent *)event;
- (NSEvent *)handleKeyDown:(NSEvent *)event;
- (void)startAppleRemote;
- (void)stopAppleRemote;
- (void)startMediaKeys;
- (void)restartMediaKeys;
- (void)stopMediaKeys;
@end

@interface Application : NSApplication
- (void)initialize_menu;
- (void)registerSelector:(SEL)selector forKey:(MPMenuKey)key;
- (void)stopPlayback;

@property(nonatomic, assign) struct input_ctx *inputContext;
@property(nonatomic, assign) struct mp_fifo *keyFIFO;
@property(nonatomic, retain) InputQueue *iqueue;
@property(nonatomic, retain) EventsResponder *eventsResponder;
@property(nonatomic, retain) NSMutableDictionary *menuItems;
@property(nonatomic, retain) NSArray *files;
@property(nonatomic, retain) NSMutableArray *argumentsList;
@property(nonatomic, assign) BOOL willStopOnOpenEvent;
@end

Application *mpv_shared_app(void);
