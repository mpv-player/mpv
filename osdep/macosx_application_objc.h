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

#import <Cocoa/Cocoa.h>
#include "osdep/macosx_application.h"
#import "osdep/macosx_menubar_objc.h"

struct mpv_event;

@interface Application : NSApplication

- (void)processEvent:(struct mpv_event *)event;
- (void)queueCommand:(char *)cmd;
- (void)stopMPV:(char *)cmd;
- (void)openFiles:(NSArray *)filenames;

@property(nonatomic, retain) MenuBar *menuBar;
@property(nonatomic, retain) NSArray *files;
@property(nonatomic, assign) size_t openCount;
@end
