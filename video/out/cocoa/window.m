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

#include "mpvcore/input/keycodes.h"

#include "osdep/macosx_application.h"
#include "osdep/macosx_events.h"
#include "osdep/macosx_compat.h"

#include "video/out/cocoa/additions.h"
#include "video/out/cocoa_common.h"

#include "window.h"

@implementation MpvVideoWindow {
    NSSize _queued_video_size;
    bool   _fs_resize_scheduled;
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
    }
    return self;
}

- (void)windowDidResize:(NSNotification *) notification
{
    [self.adapter setNeedsResize];
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
    [self.adapter setNeedsResize];
}

- (BOOL)isInFullScreenMode
{
    return (([self styleMask] & NSFullScreenWindowMask) ==
                NSFullScreenWindowMask);
}

- (void)setFullScreen:(BOOL)willBeFullscreen
{
    if (willBeFullscreen && ![self isInFullScreenMode]) {
        [self setContentResizeIncrements:NSMakeSize(1, 1)];
        [self toggleFullScreen:nil];
    }

    if (!willBeFullscreen && [self isInFullScreenMode]) {
        [self setContentAspectRatio:self->_queued_video_size];
        [self toggleFullScreen:nil];
    }

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
    if (![self.adapter isInFullScreenMode]) {
        NSSize size = [self.adapter videoSize];
        size.width  *= multiplier;
        size.height *= multiplier;
        [self setCenteredContentSize:size];
    }
}

- (int)titleHeight
{
    NSRect of    = [self frame];
    NSRect cb    = [[self contentView] bounds];
    return of.size.height - cb.size.height;
}

- (void)setCenteredContentSize:(NSSize)ns
{
    NSRect f   = [self frame];
    CGFloat dx = (f.size.width  - ns.width) / 2;
    CGFloat dy = (f.size.height - ns.height - [self titleHeight]) / 2;
    NSRect nf  = NSRectFromCGRect(CGRectInset(NSRectToCGRect(f), dx, dy));
    [self setFrame:nf display:NO animate:NO];
}

- (NSRect)constrainFrameRect:(NSRect)nf toScreen:(NSScreen *)screen
{
    NSRect s = [[self screen] visibleFrame];
    if (nf.origin.y + nf.size.height > s.origin.y + s.size.height)
        nf.origin.y = s.origin.y + s.size.height - nf.size.height;
    return nf;
}

- (void)queueNewVideoSize:(NSSize)new_size
{
    NSSize prev_size = self->_queued_video_size;
    self->_queued_video_size = new_size;

    if (!CGSizeEqualToSize(prev_size, new_size))
        [self dispatchNewVideoSize];
}

- (void)dispatchNewVideoSize
{
    if ([self.adapter isInFullScreenMode]) {
        self->_fs_resize_scheduled = true;
    } else {
        [self applyNewVideoSize];
    }
}

- (void)applyNewVideoSize
{
    [self setCenteredContentSize:self->_queued_video_size];
    [self setContentAspectRatio:self->_queued_video_size];
}

- (void)didChangeFullScreenState
{
    if (![self.adapter isInFullScreenMode] && self->_fs_resize_scheduled) {
        self->_fs_resize_scheduled = false;
        [self applyNewVideoSize];
    }
}
@end

