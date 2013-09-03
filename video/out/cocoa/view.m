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

#include <libavutil/common.h>

#include "mpvcore/input/input.h"
#include "mpvcore/input/keycodes.h"

#include "osdep/macosx_compat.h"
#include "video/out/cocoa_common.h"
#import  "video/out/cocoa/additions.h"

#include "view.h"

@implementation MpvVideoView
@synthesize adapter = _adapter;
@synthesize tracker = _tracker;

- (void)setFullScreen:(BOOL)willBeFullscreen
{
    if (willBeFullscreen && ![self isInFullScreenMode]) {
        NSApplicationPresentationOptions popts =
            NSApplicationPresentationDefault;

        if ([[self.adapter fsScreen] hasMenubar])
            // Cocoa raises an exception when autohiding the menubar but
            // not the dock. They probably got bored while programming the
            // multi screen support and took some shortcuts (tested on 10.8).
            popts |= NSApplicationPresentationAutoHideMenuBar |
                     NSApplicationPresentationAutoHideDock;

        if ([[self.adapter fsScreen] hasDock])
            popts |= NSApplicationPresentationAutoHideDock;

        NSDictionary *fsopts = @{
            NSFullScreenModeAllScreens : @NO,
            NSFullScreenModeApplicationPresentationOptions : @(popts)
        };

        // The original "windowed" window will stay around since sending a
        // view fullscreen wraps it in another window. This is noticeable when
        // sending the View fullscreen to another screen. Make it go away
        // manually.
        [self.window orderOut:self];

        [self enterFullScreenMode:[self.adapter fsScreen]
                                  withOptions:fsopts];
    }

    if (!willBeFullscreen && [self isInFullScreenMode]) {
        [self exitFullScreenModeWithOptions:nil];

        // Show the "windowed" window again.
        [self.window makeKeyAndOrderFront:self];
        [self.window makeFirstResponder:self];
    }
}

// mpv uses flipped coordinates, because X11 uses those. So let's just use them
// as well without having to do any coordinate conversion of mouse positions.
- (BOOL)isFlipped { return YES; }

- (void)updateTrackingAreas
{
    if (self.tracker) [self removeTrackingArea:self.tracker];

    NSTrackingAreaOptions trackingOptions =
        NSTrackingEnabledDuringMouseDrag |
        NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
        NSTrackingActiveAlways;

    self.tracker =
        [[[NSTrackingArea alloc] initWithRect:[self bounds]
                                      options:trackingOptions
                                        owner:self
                                     userInfo:nil] autorelease];

    [self addTrackingArea:self.tracker];
}

- (NSPoint)mouseLocation
{
    NSPoint wLoc = [self.window mouseLocationOutsideOfEventStream];
    return [self convertPoint:wLoc fromView:nil];
}

- (BOOL)containsMouseLocation
{
    NSRect vF  = [[self.window screen] visibleFrame];
    NSRect vFW = [self.window convertRectFromScreen:vF];
    NSRect vFV = [self convertRect:vFW fromView:nil];

    // clip bounds to current visibleFrame
    NSRect clippedBounds = CGRectIntersection([self bounds], vFV);
    return CGRectContainsPoint(clippedBounds, [self mouseLocation]);
}

- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)becomeFirstResponder { return YES; }
- (BOOL)resignFirstResponder { return YES; }

- (NSRect)frameInPixels
{
    return [self convertRectToBacking:[self frame]];
}

- (BOOL)canHideCursor
{
    return !self->hasMouseDown && [self containsMouseLocation];
}

- (void)mouseEntered:(NSEvent *)event
{
    // do nothing!
}

- (void)mouseExited:(NSEvent *)event
{
    [self.adapter putKey:MP_KEY_MOUSE_LEAVE withModifiers:0];
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [self signalMousePosition];
}

- (void)signalMousePosition
{
    NSPoint p = [self convertPoint:[self mouseLocation] fromView:nil];
    [self.adapter signalMouseMovement:p];
}

- (void)signalMouseMovement:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    [self.adapter signalMouseMovement:p];
}

- (void)mouseMoved:(NSEvent *)event   { [self signalMouseMovement:event]; }
- (void)mouseDragged:(NSEvent *)event { [self signalMouseMovement:event]; }
- (void)mouseDown:(NSEvent *)evt      { [self mouseDownEvent:evt]; }
- (void)mouseUp:(NSEvent *)evt        { [self mouseUpEvent:evt]; }
- (void)rightMouseDown:(NSEvent *)evt { [self mouseDownEvent:evt]; }
- (void)rightMouseUp:(NSEvent *)evt   { [self mouseUpEvent:evt]; }
- (void)otherMouseDown:(NSEvent *)evt { [self mouseDownEvent:evt]; }
- (void)otherMouseUp:(NSEvent *)evt   { [self mouseUpEvent:evt]; }

- (void)scrollWheel:(NSEvent *)event
{
    CGFloat delta;
    int cmd;

    if (FFABS([event deltaY]) >= FFABS([event deltaX])) {
        delta = [event deltaY] * 0.1;
        cmd   = delta > 0 ? MP_AXIS_UP : MP_AXIS_DOWN;
        delta = FFABS(delta);
    } else {
        delta = [event deltaX] * 0.1;
        cmd   = delta > 0 ? MP_AXIS_RIGHT : MP_AXIS_LEFT;
        delta = FFABS(delta);
    }

    if ([event hasPreciseScrollingDeltas]) {
        [self.adapter putAxis:cmd delta:delta];
    } else {
        const int modifiers = [event modifierFlags];
        const int mpkey = delta > 0 ? MP_MOUSE_BTN3 : MP_MOUSE_BTN4;
        [self.adapter putKey:mpkey withModifiers:modifiers];
    }
}

- (void)mouseDownEvent:(NSEvent *)event
{
    [self putMouseEvent:event withState:MP_KEY_STATE_DOWN];

    if ([event clickCount] > 1)
        [self putMouseEvent:event withState:MP_KEY_STATE_UP];
}

- (void)mouseUpEvent:(NSEvent *)event
{
    [self putMouseEvent:event withState:MP_KEY_STATE_UP];
}

- (void)putMouseEvent:(NSEvent *)event withState:(int)state
{
    self->hasMouseDown = (state == MP_KEY_STATE_DOWN);
    int mpkey = (MP_MOUSE_BTN0 + [event mpvButtonNumber]);
    [self.adapter putKey:(mpkey | state) withModifiers:[event modifierFlags]];
}

- (void)drawRect:(NSRect)rect
{
    [self.adapter performAsyncResize:[self frameInPixels].size];
    [self.adapter setNeedsResize];
}
@end
