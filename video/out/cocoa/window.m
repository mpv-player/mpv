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

#include <libavutil/common.h>

#include "input/keycodes.h"

#include "osdep/macosx_events.h"
#include "osdep/macosx_compat.h"
#include "video/out/cocoa_common.h"

#include "window.h"

@interface MpvVideoWindow()
@property(nonatomic, retain) NSScreen *targetScreen;
@property(nonatomic, retain) NSScreen *previousScreen;
@property(nonatomic, retain) NSScreen *currentScreen;
@property(nonatomic, retain) NSScreen *unfScreen;

- (NSRect)frameRect:(NSRect)frameRect forCenteredContentSize:(NSSize)newSize;
- (void)setCenteredContentSize:(NSSize)newSize;
@end

@implementation MpvVideoWindow {
    NSSize _queued_video_size;
    NSRect _unfs_content_frame;
    int _is_animating;
}

@synthesize adapter = _adapter;
@synthesize targetScreen = _target_screen;
@synthesize previousScreen = _previous_screen;
@synthesize currentScreen = _current_screen;
@synthesize unfScreen = _unf_screen;

- (id)initWithContentRect:(NSRect)content_rect
                styleMask:(NSWindowStyleMask)style_mask
                  backing:(NSBackingStoreType)buffering_type
                    defer:(BOOL)flag
                   screen:(NSScreen *)screen
{
    if (self = [super initWithContentRect:content_rect
                                styleMask:style_mask
                                  backing:buffering_type
                                    defer:flag
                                   screen:screen]) {
        [self setBackgroundColor:[NSColor whiteColor]];
        [self setMinSize:NSMakeSize(50,50)];
        [self setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary];

        self.targetScreen = screen;
        self.currentScreen = screen;
        self.unfScreen = screen;
        _is_animating = 0;
        _unfs_content_frame = [self convertRectToScreen:[[self contentView] frame]];
    }
    return self;
}

- (void)setStyleMask:(NSWindowStyleMask)style
{
    NSResponder *nR = [self firstResponder];
    [super setStyleMask:style];
    [self makeFirstResponder:nR];
}

- (void)toggleFullScreen:(id)sender
{
    if (_is_animating)
        return;

    _is_animating = 1;

    self.targetScreen = [self.adapter getTargetScreen];
    if(![self targetScreen] && ![self previousScreen]) {
        self.targetScreen = [self screen];
    } else if (![self targetScreen]) {
        self.targetScreen = self.previousScreen;
        self.previousScreen = nil;
    } else {
        self.previousScreen = [self screen];
    }

    if (![self.adapter isInFullScreenMode]) {
        _unfs_content_frame = [self convertRectToScreen:[[self contentView] frame]];
        self.unfScreen = [self screen];
    }

    //move window to target screen when going to fullscreen
    if (![self.adapter isInFullScreenMode] && ![[self targetScreen] isEqual:[self screen]]) {
        NSRect frame = [self calculateWindowPositionForScreen:[self targetScreen]
                                                withoutBounds:NO];
        [self setFrame:frame display:YES];
    }

    if ([self.adapter wantsNativeFullscreen])
        [super toggleFullScreen:sender];

    if (![self.adapter isInFullScreenMode]) {
        [self setToFullScreen];
    } else {
        [self setToWindow];
    }
}

- (void)setToFullScreen
{
    [self setStyleMask:([self styleMask] | NSWindowStyleMaskFullScreen)];
    NSRect frame = [[self targetScreen] frame];

    if ([self.adapter wantsNativeFullscreen]) {
        [self setFrame:frame display:YES];
    } else {
        [NSApp setPresentationOptions:NSApplicationPresentationAutoHideMenuBar|
                                      NSApplicationPresentationAutoHideDock];
        [self setFrame:frame display:YES];
        _is_animating = 0;
        [self.adapter windowDidEnterFullScreen];
    }
}

- (void)setToWindow
{
    [self setStyleMask:([self styleMask] & ~NSWindowStyleMaskFullScreen)];
    NSRect frame = [self calculateWindowPositionForScreen:[self targetScreen]
                    withoutBounds:[[self targetScreen] isEqual:[self screen]]];

    if ([self.adapter wantsNativeFullscreen]) {
        [self setFrame:frame display:YES];
        [self setContentAspectRatio:_unfs_content_frame.size];
        [self setCenteredContentSize:_unfs_content_frame.size];
    } else {
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];
        [self setFrame:frame display:YES];
        [self setContentAspectRatio:_unfs_content_frame.size];
        [self setCenteredContentSize:_unfs_content_frame.size];
        _is_animating = 0;
        [self.adapter windowDidExitFullScreen];
    }
}

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window
{
    return [NSArray arrayWithObject:window];
}

- (NSArray*)customWindowsToExitFullScreenForWindow:(NSWindow*)window
{
    return [NSArray arrayWithObject:window];
}

// we still need to keep those around or it will use the standard animation
- (void)window:(NSWindow *)window startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration {}

- (void)window:(NSWindow *)window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration {}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    _is_animating = 0;
    [self.adapter windowDidEnterFullScreen];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    _is_animating = 0;
    [self.adapter windowDidExitFullScreen];
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    [self.adapter windowWillEnterFullScreen:notification];
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    [self.adapter windowWillExitFullScreen:notification];
}

- (void)windowDidFailToEnterFullScreen:(NSWindow *)window
{
    _is_animating = 0;
    [self setToWindow];
    [self.adapter windowDidFailToEnterFullScreen:window];
}

- (void)windowDidFailToExitFullScreen:(NSWindow *)window
{
    _is_animating = 0;
    [self setToFullScreen];
    [self.adapter windowDidFailToExitFullScreen:window];
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
    // XXX: we maybe only need expose for this
    [self.adapter setNeedsResize];
}

- (void)windowDidChangeScreen:(NSNotification *)notification
{
    [self.adapter windowDidChangeScreen:notification];

    if (!_is_animating && ![[self currentScreen] isEqual:[self screen]]) {
        self.previousScreen = [self screen];
    }
    if (![[self currentScreen] isEqual:[self screen]]) {
        [self.adapter windowDidChangePhysicalScreen];
    }

    self.currentScreen = [self screen];
}

- (void)windowDidChangeScreenProfile:(NSNotification *)notification
{
    [self.adapter didChangeWindowedScreenProfile:notification];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    [self.adapter windowDidResignKey:notification];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    [self.adapter windowDidBecomeKey:notification];
}

- (void)windowWillMove:(NSNotification *)notification
{
    [self.adapter windowWillMove:notification];
}

- (BOOL)canBecomeMainWindow { return YES; }
- (BOOL)canBecomeKeyWindow { return YES; }

- (BOOL)windowShouldClose:(id)sender
{
    cocoa_put_key(MP_KEY_CLOSE_WIN);
    // We have to wait for MPlayer to handle this,
    // otherwise we are in trouble if the
    // MP_KEY_CLOSE_WIN handler is disabled
    return NO;
}

- (void)normalSize { [self mulSize:1.0f]; }

- (void)halfSize { [self mulSize:0.5f];}

- (void)doubleSize { [self mulSize:2.0f];}

- (void)mulSize:(float)multiplier
{
    char cmd[50];
    snprintf(cmd, sizeof(cmd), "set window-scale %f", multiplier);
    [self.adapter putCommand:cmd];
}

- (void)updateBorder:(int)border
{
    int borderStyle = NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|
                 NSWindowStyleMaskMiniaturizable;
    if (border) {
        int window_mask = [self styleMask] & ~NSWindowStyleMaskBorderless;
        window_mask |= borderStyle;
        [self setStyleMask:window_mask];
    } else {
        int window_mask = [self styleMask] & ~borderStyle;
        window_mask |= NSWindowStyleMaskBorderless;
        [self setStyleMask:window_mask];
    }

    if (![self.adapter isInFullScreenMode]) {
        // XXX: workaround to force redrawing of window decoration
        if (border) {
            NSRect frame = [self frame];
            frame.size.width += 1;
            [self setFrame:frame display:YES];
            frame.size.width -= 1;
            [self setFrame:frame display:YES];
        }

        [self setContentAspectRatio:_unfs_content_frame.size];
    }
}

- (NSRect)frameRect:(NSRect)f forCenteredContentSize:(NSSize)ns
{
    NSRect cr  = [self contentRectForFrameRect:f];
    CGFloat dx = (cr.size.width  - ns.width)  / 2;
    CGFloat dy = (cr.size.height - ns.height) / 2;
    return NSInsetRect(f, dx, dy);
}

- (void)setCenteredContentSize:(NSSize)ns
{
    [self setFrame:[self frameRect:[self frame] forCenteredContentSize:ns]
           display:NO
           animate:NO];
}

- (NSRect)calculateWindowPositionForScreen:(NSScreen *)screen withoutBounds:(BOOL)withoutBounds
{
    NSRect frame = [self frameRectForContentRect:_unfs_content_frame];
    NSRect targetFrame = [screen frame];
    NSRect targetVisibleFrame = [screen visibleFrame];
    NSRect unfsScreenFrame = [self.unfScreen frame];
    NSRect visibleWindow = NSIntersectionRect(unfsScreenFrame, frame);

    // calculate visible area of every side
    CGFloat left = frame.origin.x - unfsScreenFrame.origin.x;
    CGFloat right = unfsScreenFrame.size.width -
        (frame.origin.x - unfsScreenFrame.origin.x + frame.size.width);
    CGFloat bottom = frame.origin.y - unfsScreenFrame.origin.y;
    CGFloat top = unfsScreenFrame.size.height -
        (frame.origin.y - unfsScreenFrame.origin.y + frame.size.height);

    // normalize visible areas, decide which one to take horizontal/vertical
    CGFloat x_per = (unfsScreenFrame.size.width - visibleWindow.size.width);
    CGFloat y_per = (unfsScreenFrame.size.height - visibleWindow.size.height);
    if (x_per != 0) x_per = (left >= 0 || right < 0 ? left : right)/x_per;
    if (y_per != 0) y_per = (bottom >= 0 || top < 0 ? bottom : top)/y_per;

    // calculate visible area for every side for target screen
    CGFloat x_new_left = targetFrame.origin.x +
        (targetFrame.size.width - visibleWindow.size.width)*x_per;
    CGFloat x_new_right = targetFrame.origin.x + targetFrame.size.width -
        (targetFrame.size.width - visibleWindow.size.width)*x_per - frame.size.width;
    CGFloat y_new_bottom = targetFrame.origin.y +
        (targetFrame.size.height - visibleWindow.size.height)*y_per;
    CGFloat y_new_top = targetFrame.origin.y + targetFrame.size.height -
        (targetFrame.size.height - visibleWindow.size.height)*y_per - frame.size.height;

    // calculate new coordinates, decide which one to take horizontal/vertical
    frame.origin.x = left >= 0 || right < 0 ? x_new_left : x_new_right;
    frame.origin.y = bottom >= 0 || top < 0 ? y_new_bottom : y_new_top;

    // don't place new window on top of a visible menubar
    CGFloat top_mar = targetFrame.size.height -
        (frame.origin.y - targetFrame.origin.y + frame.size.height);
    CGFloat menuBarHeight = targetFrame.size.height -
        (targetVisibleFrame.size.height + targetVisibleFrame.origin.y);

    if (top_mar < menuBarHeight)
        frame.origin.y -= top-menuBarHeight;

    if (withoutBounds)
        return frame;

    //screen bounds right and left
    if (frame.origin.x + frame.size.width > targetFrame.origin.x + targetFrame.size.width)
        frame.origin.x = targetFrame.origin.x + targetFrame.size.width - frame.size.width;
    if (frame.origin.x < targetFrame.origin.x)
        frame.origin.x = targetFrame.origin.x;

    //screen bounds top and bottom
    if (frame.origin.y + frame.size.height > targetFrame.origin.y + targetFrame.size.height)
        frame.origin.y = targetFrame.origin.y + targetFrame.size.height - frame.size.height;
    if (frame.origin.y < targetFrame.origin.y)
        frame.origin.y = targetFrame.origin.y;

    return frame;
}

- (NSRect)constrainFrameRect:(NSRect)nf toScreen:(NSScreen *)screen
{
    if ((_is_animating && ![self.adapter isInFullScreenMode]) ||
        (!_is_animating && [self.adapter isInFullScreenMode]))
    {
        return nf;
    }

    screen = screen ?: self.screen ?: [NSScreen mainScreen];
    NSRect of  = [self frame];
    NSRect vf  = [_is_animating ? [self targetScreen] : screen visibleFrame];
    NSRect ncf = [self contentRectForFrameRect:nf];

    // Prevent the window's titlebar from exiting the screen on the top edge.
    // This introduces a 'snap to top' behaviour.
    if (NSMaxY(nf) > NSMaxY(vf))
        nf.origin.y = NSMaxY(vf) - NSHeight(nf);

    // Prevent the window's titlebar from exiting the screen on the bottom edge.
    if (NSMaxY(ncf) < NSMinY(vf))
        nf.origin.y = NSMinY(vf) + NSMinY(ncf) - NSMaxY(ncf);

    // Prevent window from exiting the screen on the right edge
    if (NSMinX(nf) > NSMaxX(vf))
        nf.origin.x = NSMaxX(vf) - NSWidth(nf);

    // Prevent window from exiting the screen on the left
    if (NSMaxX(nf) < NSMinX(vf))
        nf.origin.x = NSMinX(vf);

    if (NSHeight(nf) < NSHeight(vf) && NSHeight(of) > NSHeight(vf) &&
        ![self.adapter isInFullScreenMode])
        // If the window height is smaller than the visible frame, but it was
        // bigger previously recenter the smaller window vertically. This is
        // needed to counter the 'snap to top' behaviour.
        nf.origin.y = (NSHeight(vf) - NSHeight(nf)) / 2;

    return nf;
}

- (void)windowWillStartLiveResize:(NSNotification *)notification
{
    [self.adapter windowWillStartLiveResize:notification];
}

- (void)windowDidEndLiveResize:(NSNotification *)notification
{
    [self.adapter windowDidEndLiveResize:notification];
    [self setFrame:[self constrainFrameRect:self.frame toScreen:self.screen]
           display:NO];
}

- (void)tryDequeueSize
{
    if (_queued_video_size.width <= 0.0 || _queued_video_size.height <= 0.0)
        return;

    [self setContentAspectRatio:_queued_video_size];
    [self setCenteredContentSize:_queued_video_size];
    _queued_video_size = NSZeroSize;
}

- (void)queueNewVideoSize:(NSSize)newSize
{
    _unfs_content_frame = [self frameRect:_unfs_content_frame forCenteredContentSize:newSize];
    if (![self.adapter isInFullScreenMode]) {
        if (NSEqualSizes(_queued_video_size, newSize))
            return;
        _queued_video_size = newSize;
        [self tryDequeueSize];
    }
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
    [self tryDequeueSize];
}
@end
