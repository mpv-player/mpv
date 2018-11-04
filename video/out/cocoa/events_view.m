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

#include "input/input.h"
#include "input/keycodes.h"

#include "osdep/macosx_compat.h"
#include "video/out/cocoa_common.h"
#include "events_view.h"

@interface MpvEventsView()
@property(nonatomic, assign) BOOL hasMouseDown;
@property(nonatomic, retain) NSTrackingArea *tracker;
- (int)mpvButtonNumber:(NSEvent*)event;
- (void)mouseDownEvent:(NSEvent *)event;
- (void)mouseUpEvent:(NSEvent *)event;
@end

@implementation MpvEventsView
@synthesize adapter = _adapter;
@synthesize tracker = _tracker;
@synthesize hasMouseDown = _mouse_down;

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self registerForDraggedTypes:@[NSFilenamesPboardType,
                                        NSURLPboardType]];
        [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    }
    return self;
}

- (void)drawRect:(NSRect)rect
{
    [[NSColor blackColor] setFill];
    NSRectFill(rect);
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

    if (![self containsMouseLocation])
        [self.adapter putKey:MP_KEY_MOUSE_LEAVE withModifiers:0];
}

- (NSPoint)mouseLocation
{
    return [self.window mouseLocationOutsideOfEventStream];
}

- (BOOL)containsMouseLocation
{
    CGFloat topMargin = 0.0;
    CGFloat menuBarHeight = [[NSApp mainMenu] menuBarHeight];

    // menuBarHeight is 0 when menu bar is hidden in fullscreen
    // 1pt to compensate of the black line beneath the menu bar
    if ([self.adapter isInFullScreenMode] && menuBarHeight > 0) {
        NSRect tr = [NSWindow frameRectForContentRect:CGRectZero
                                            styleMask:NSWindowStyleMaskTitled];
        topMargin = tr.size.height + 1 + menuBarHeight;
    }

    NSRect vF  = [[self.window screen] frame];
    vF.size.height -= topMargin;
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

- (BOOL)acceptsFirstResponder
{
    return [self.adapter keyboardEnabled] || [self.adapter mouseEnabled];
}

- (BOOL)becomeFirstResponder
{
    return [self.adapter keyboardEnabled] || [self.adapter mouseEnabled];
}

- (BOOL)resignFirstResponder { return YES; }

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

- (NSPoint)convertPointToPixels:(NSPoint)point
{
    point = [self convertPoint:point fromView:nil];
    point = [self convertPointToBacking:point];
    // flip y since isFlipped returning YES doesn't affect the backing
    // coordinate system
    point.y = -point.y;
    return point;
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

    if (fabs([event deltaY]) >= fabs([event deltaX])) {
        delta = [event deltaY] * 0.1;
        cmd   = delta > 0 ? MP_WHEEL_UP : MP_WHEEL_DOWN;
    } else {
        delta = [event deltaX] * 0.1;
        cmd   = delta > 0 ? MP_WHEEL_RIGHT : MP_WHEEL_LEFT;
    }

    [self.adapter putWheel:cmd delta:fabs(delta)];
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
        const float deltaX = (modifiers & NSEventModifierFlagShift) ?
                             [event scrollingDeltaY] : [event scrollingDeltaX];
        const float deltaY = (modifiers & NSEventModifierFlagShift) ?
                             [event scrollingDeltaX] : [event scrollingDeltaY];
        int mpkey;

        if (fabs(deltaY) >= fabs(deltaX)) {
            mpkey = deltaY > 0 ? MP_WHEEL_UP : MP_WHEEL_DOWN;
        } else {
            mpkey = deltaX > 0 ? MP_WHEEL_LEFT : MP_WHEEL_RIGHT;
        }

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
    int mpkey = [self mpvButtonNumber:event];
    [self.adapter putKey:(mpkey | state) withModifiers:[event modifierFlags]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];
    NSArray *types = [pboard types];
    if ([types containsObject:NSFilenamesPboardType] ||
        [types containsObject:NSURLPboardType]) {
        return NSDragOperationCopy;
    } else {
        return NSDragOperationNone;
    }
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([[pboard types] containsObject:NSFilenamesPboardType]) {
        NSArray *pbitems = [pboard propertyListForType:NSFilenamesPboardType];
        [self.adapter handleFilesArray:pbitems];
        return YES;
    } else if ([[pboard types] containsObject:NSURLPboardType]) {
        NSURL *url = [NSURL URLFromPasteboard:pboard];
        [self.adapter handleFilesArray:@[[url absoluteString]]];
        return YES;
    }
    return NO;
}

- (int)mpvButtonNumber:(NSEvent*)event
{
    int buttonNumber = [event buttonNumber];
    switch (buttonNumber) {
        case 0:  return MP_MBTN_LEFT;
        case 1:  return MP_MBTN_RIGHT;
        case 2:  return MP_MBTN_MID;
        case 3:  return MP_MBTN_BACK;
        case 4:  return MP_MBTN_FORWARD;
        default: return MP_MBTN9 - 5 + buttonNumber;
    }
}
@end
