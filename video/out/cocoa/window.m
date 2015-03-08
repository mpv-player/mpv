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

#include "input/keycodes.h"

#include "osdep/macosx_events.h"
#include "osdep/macosx_compat.h"
#include "video/out/cocoa_common.h"

#include "window.h"

@interface MpvVideoWindow()
- (NSRect)frameRect:(NSRect)frameRect forCenteredContentSize:(NSSize)newSize;
- (void)setCenteredContentSize:(NSSize)newSize;
@end

@implementation MpvVideoWindow {
    NSSize _queued_video_size;
}

@synthesize adapter = _adapter;
- (id)initWithContentRect:(NSRect)content_rect
                styleMask:(NSUInteger)style_mask
                  backing:(NSBackingStoreType)buffering_type
                    defer:(BOOL)flag
{
    if (self = [super initWithContentRect:content_rect
                                styleMask:style_mask
                                  backing:buffering_type
                                    defer:flag]) {
        [self setBackgroundColor:[NSColor blackColor]];
        [self setMinSize:NSMakeSize(50,50)];
    }
    return self;
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
    // XXX: we maybe only need expose for this
    [self.adapter setNeedsResize];
}

- (void)windowDidChangeScreenProfile:(NSNotification *)notification
{
    [self.adapter didChangeWindowedScreenProfile:[self screen]];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    [self.adapter windowDidResignKey:notification];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    [self.adapter windowDidBecomeKey:notification];
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    [self.adapter windowDidMiniaturize:notification];
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    [self.adapter windowDidDeminiaturize:notification];
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

- (NSRect)constrainFrameRect:(NSRect)nf toScreen:(NSScreen *)screen
{
    NSRect of  = [self frame];
    NSRect vf  = [screen ?: self.screen ?: [NSScreen mainScreen] visibleFrame];
    NSRect ncf = [self contentRectForFrameRect:nf];

    // Prevent the window's titlebar from exiting the screen on the top edge.
    // This introduces a 'snap to top' behaviour.
    if (NSMaxY(nf) > NSMaxY(vf))
        nf.origin.y = NSMaxY(vf) - NSHeight(nf);

    // Prevent the window's titlebar from exiting the screen on the top edge.
    if (NSMaxY(ncf) < NSMinY(vf))
        nf.origin.y = NSMinY(vf) + NSMinY(ncf) - NSMaxY(ncf);

    // Prevent window from exiting the screen on the right edge
    if (NSMinX(nf) > NSMaxX(vf))
        nf.origin.x = NSMaxX(vf) - NSWidth(nf);

    // Prevent window from exiting the screen on the left
    if (NSMaxX(nf) < NSMinX(vf))
        nf.origin.x = NSMinX(vf);

    if (NSHeight(nf) < NSHeight(vf) && NSHeight(of) > NSHeight(vf))
        // If the window height is smaller than the visible frame, but it was
        // bigger previously recenter the smaller window vertically. This is
        // needed to counter the 'snap to top' behaviour.
        nf.origin.y = (NSHeight(vf) - NSHeight(nf)) / 2;

    return nf;
}

- (void)windowDidEndLiveResize:(NSNotification *)notification
{
    [self setFrame:[self constrainFrameRect:self.frame toScreen:self.screen]
           display:NO];
}

- (void)tryDequeueSize {
    if (_queued_video_size.width <= 0.0 || _queued_video_size.height <= 0.0)
        return;

    // XXX find a way to kill this state
    if (![self.adapter isInFullScreenMode]) {
        [self setContentAspectRatio:_queued_video_size];
        [self setCenteredContentSize:_queued_video_size];
        _queued_video_size = NSZeroSize;
    }
}

- (void)queueNewVideoSize:(NSSize)new_size
{
    if (NSEqualSizes(_queued_video_size, new_size))
        return;
    _queued_video_size = new_size;
    [self tryDequeueSize];
}

- (void)windowDidBecomeMain:(NSNotification *)notification {
    [self tryDequeueSize];
}
@end
