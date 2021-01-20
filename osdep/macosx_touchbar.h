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
#import "osdep/macosx_application_objc.h"

#define BASE_ID @"io.mpv.touchbar"
static NSTouchBarCustomizationIdentifier customID = BASE_ID;
static NSTouchBarItemIdentifier seekBar = BASE_ID ".seekbar";
static NSTouchBarItemIdentifier play = BASE_ID ".play";
static NSTouchBarItemIdentifier nextItem = BASE_ID ".nextItem";
static NSTouchBarItemIdentifier previousItem = BASE_ID ".previousItem";
static NSTouchBarItemIdentifier nextChapter = BASE_ID ".nextChapter";
static NSTouchBarItemIdentifier previousChapter = BASE_ID ".previousChapter";
static NSTouchBarItemIdentifier cycleAudio = BASE_ID ".cycleAudio";
static NSTouchBarItemIdentifier cycleSubtitle = BASE_ID ".cycleSubtitle";
static NSTouchBarItemIdentifier currentPosition = BASE_ID ".currentPosition";
static NSTouchBarItemIdentifier timeLeft = BASE_ID ".timeLeft";

struct mpv_event;

@interface TouchBar : NSTouchBar <NSTouchBarDelegate>

-(void)processEvent:(struct mpv_event *)event;

@property(nonatomic, retain) Application *app;
@property(nonatomic, retain) NSDictionary *touchbarItems;
@property(nonatomic, assign) double duration;
@property(nonatomic, assign) double position;
@property(nonatomic, assign) int pause;

@end
