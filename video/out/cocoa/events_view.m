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

#include "input/input.h"
#include "input/keycodes.h"

#include "osdep/macosx_compat.h"
#include "video/out/cocoa_common.h"
#include "events_view.h"

@interface MpvEventsView()
@property(nonatomic, assign) BOOL clearing;
@property(nonatomic, assign) BOOL hasMouseDown;
@property(nonatomic, retain) NSTrackingArea *tracker;
- (BOOL)hasDock:(NSScreen*)screen;
- (BOOL)hasMenubar:(NSScreen*)screen;
- (int)mpvButtonNumber:(NSEvent*)event;
- (void)mouseDownEvent:(NSEvent *)event;
- (void)mouseUpEvent:(NSEvent *)event;
@end

@implementation MpvEventsView
@synthesize clearing = _clearing;
@synthesize adapter = _adapter;
@synthesize tracker = _tracker;
@synthesize hasMouseDown = _mouse_down;

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self registerForDraggedTypes:@[NSFilenamesPboardType,
                                        NSURLPboardType]];
        [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    }
    return self;
}

- (void)setFullScreen:(BOOL)willBeFullscreen
{
    if (willBeFullscreen && ![self isInFullScreenMode]) {
        NSApplicationPresentationOptions popts =
            NSApplicationPresentationDefault;

        if ([self hasMenubar:[self.adapter fsScreen]])
            // Cocoa raises an exception when autohiding the menubar but
            // not the dock. They probably got bored while programming the
            // multi screen support and took some shortcuts (tested on 10.8).
            popts |= NSApplicationPresentationAutoHideMenuBar |
                     NSApplicationPresentationAutoHideDock;

        if ([self hasDock:[self.adapter fsScreen]])
            popts |= NSApplicationPresentationAutoHideDock;

        NSDictionary *fsopts = @{
            NSFullScreenModeAllScreens : @([self.adapter fsModeAllScreens]),
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

- (void)clear
{
    if ([self isInFullScreenMode]) {
        self.clearing = YES;
        [self exitFullScreenModeWithOptions:nil];
    }
}

// mpv uses flipped coordinates, because X11 uses those. So let's just use them
// as well without having to do any coordinate conversion of mouse positions.
- (BOOL)isFlipped { return YES; }

- (void)updateTrackingAreas
{
    if (self.tracker)
        [self removeTrackingArea:self.tracker];

    if (![self.adapter mouseEnabled])
        return;

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
    return [self.window mouseLocationOutsideOfEventStream];
}

- (BOOL)containsMouseLocation
{
    NSRect vF  = [[self.window screen] visibleFrame];
    NSRect vFW = [self.window convertRectFromScreen:vF];
    NSRect vFV = [self convertRect:vFW fromView:nil];
    NSPoint pt = [self convertPoint:[self mouseLocation] fromView:nil];

    // clip bounds to current visibleFrame
    NSRect clippedBounds = CGRectIntersection([self bounds], vFV);
    return CGRectContainsPoint(clippedBounds, pt);
}

- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
    return [self.adapter mouseEnabled];
}
- (BOOL)acceptsFirstResponder {
    return [self.adapter keyboardEnabled] || [self.adapter mouseEnabled];
}

- (BOOL)becomeFirstResponder {
    return [self.adapter keyboardEnabled] || [self.adapter mouseEnabled];
}

- (BOOL)resignFirstResponder { return YES; }

- (void)keyDown:(NSEvent *)event {
    [self.adapter putKeyEvent:event];
}

- (void)keyUp:(NSEvent *)event {
    [self.adapter putKeyEvent:event];
}

- (BOOL)canHideCursor
{
    return !self.hasMouseDown && [self containsMouseLocation]
        && [[self window] isKeyWindow];
}

- (void)mouseEntered:(NSEvent *)event
{
    [super mouseEntered:event];
    if ([self.adapter mouseEnabled]) {
        [self.adapter putKey:MP_KEY_MOUSE_ENTER withModifiers:0];
    }
}

- (void)mouseExited:(NSEvent *)event
{
    if ([self.adapter mouseEnabled]) {
        [self.adapter putKey:MP_KEY_MOUSE_LEAVE withModifiers:0];
    } else {
        [super mouseExited:event];
    }
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];

    if (self.clearing)
        return;

    [self signalMousePosition];
}

- (NSPoint)convertPointToPixels:(NSPoint)point
{
    point = [self convertPoint:point fromView:nil];
    point = [self convertPointToBacking:point];
    // flip y since isFlipped returning YES doesn't affect the backing
    // coordinate system
    point.y = -point.y;
    return point;
}

- (void)signalMousePosition
{
    NSPoint p = [self convertPointToPixels:[self mouseLocation]];
    p.x = MIN(MAX(p.x, 0), self.bounds.size.width-1);
    p.y = MIN(MAX(p.y, 0), self.bounds.size.height-1);
    [self.adapter signalMouseMovement:p];
}

- (void)signalMouseMovement:(NSEvent *)event
{
    NSPoint p = [self convertPointToPixels:[event locationInWindow]];
    [self.adapter signalMouseMovement:p];
}

- (void)mouseMoved:(NSEvent *)event
{
    if ([self.adapter mouseEnabled]) {
        [self signalMouseMovement:event];
    } else {
        [super mouseMoved:event];
    }
}

- (void)mouseDragged:(NSEvent *)event
{
    if ([self.adapter mouseEnabled]) {
        [self signalMouseMovement:event];
    } else {
        [super mouseDragged:event];
    }
}

- (void)mouseDown:(NSEvent *)event
{
    if ([self.adapter mouseEnabled]) {
        [self mouseDownEvent:event];
    } else {
        [super mouseDown:event];
    }
}
- (void)mouseUp:(NSEvent *)event
{
    if ([self.adapter mouseEnabled]) {
        [self mouseUpEvent:event];
    } else {
        [super mouseUp:event];
    }
}
- (void)rightMouseDown:(NSEvent *)event
{
    if ([self.adapter mouseEnabled]) {
        [self mouseDownEvent:event];
    } else {
        [super rightMouseUp:event];
    }
}

- (void)rightMouseUp:(NSEvent *)event
{
    if ([self.adapter mouseEnabled]) {
        [self mouseUpEvent:event];
    } else {
        [super rightMouseUp:event];
    }
}

- (void)otherMouseDown:(NSEvent *)event
{
    if ([self.adapter mouseEnabled]) {
        [self mouseDownEvent:event];
    } else {
        [super otherMouseDown:event];
    }
}

- (void)otherMouseUp:(NSEvent *)event
{
     if ([self.adapter mouseEnabled]) {
        [self mouseUpEvent:event];
    } else {
        [super otherMouseUp:event];
    }
}

- (void)preciseScroll:(NSEvent *)event
{
    CGFloat delta;
    int cmd;

    if (FFABS([event deltaY]) >= FFABS([event deltaX])) {
        delta = [event deltaY] * 0.1;
        cmd   = delta > 0 ? MP_AXIS_UP : MP_AXIS_DOWN;
    } else {
        delta = [event deltaX] * 0.1;
        cmd   = delta > 0 ? MP_AXIS_RIGHT : MP_AXIS_LEFT;
    }

    [self.adapter putAxis:cmd delta:FFABS(delta)];
}

- (void)scrollWheel:(NSEvent *)event
{
    if (![self.adapter mouseEnabled]) {
        [super scrollWheel:event];
        return;
    }

    if ([event hasPreciseScrollingDeltas]) {
        [self preciseScroll:event];
    } else {
        const int modifiers = [event modifierFlags];
        const int mpkey = [event deltaY] > 0 ? MP_MOUSE_BTN3 : MP_MOUSE_BTN4;
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
    self.hasMouseDown = (state == MP_KEY_STATE_DOWN);
    int mpkey = (MP_MOUSE_BTN0 + [self mpvButtonNumber:event]);
    [self.adapter putKey:(mpkey | state) withModifiers:[event modifierFlags]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];
    NSArray *types = [pboard types];
    if ([types containsObject:NSFilenamesPboardType] ||
        [types containsObject:NSURLPboardType])
        return NSDragOperationCopy;
    else
        return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([[pboard types] containsObject:NSURLPboardType]) {
        NSURL *file_url = [NSURL URLFromPasteboard:pboard];
        [self.adapter handleFilesArray:@[[file_url absoluteString]]];
        return YES;
    } else if ([[pboard types] containsObject:NSFilenamesPboardType]) {
        NSArray *pbitems = [pboard propertyListForType:NSFilenamesPboardType];
        [self.adapter handleFilesArray:pbitems];
        return YES;
    }
    return NO;
}

- (BOOL)hasDock:(NSScreen*)screen
{
    NSRect vF = [screen visibleFrame];
    NSRect f  = [screen frame];
    return
        // The visible frame's width is smaller: dock is on left or right end
        // of this method's receiver.
        vF.size.width < f.size.width ||
        // The visible frame's veritical origin is bigger: dock is
        // on the bottom of this method's receiver.
        vF.origin.y > f.origin.y;

}

- (BOOL)hasMenubar:(NSScreen*)screen
{
    NSRect vF = [screen visibleFrame];
    NSRect f  = [screen frame];
    return f.size.height + f.origin.y > vF.size.height + vF.origin.y;
}

- (int)mpvButtonNumber:(NSEvent*)event
{
    int buttonNumber = [event buttonNumber];
    switch (buttonNumber) {
        case 1:  return 2;
        case 2:  return 1;
        default: return buttonNumber;
    }
}
@end
