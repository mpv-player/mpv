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

#include "osdep/macosx_application.h"
#include "osdep/macosx_events.h"
#include "osdep/macosx_compat.h"

#include "video/out/cocoa/additions.h"
#include "video/out/cocoa_common.h"

#include "window.h"

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

- (void)windowDidResize:(NSNotification *) notification
{
    [self.adapter setNeedsResize];
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
    [self.adapter setNeedsResize];
}

- (void)windowDidChangeScreenProfile:(NSNotification *)notification
{
    [self.adapter didChangeWindowedScreenProfile:[self screen]];
}

- (BOOL)isInFullScreenMode
{
    return !!([self styleMask] & NSFullScreenWindowMask);
}

- (void)setFullScreen:(BOOL)willBeFullscreen
{
    if (willBeFullscreen != [self isInFullScreenMode]) {
        [super toggleFullScreen:nil];
    }
}

- (void)toggleFullScreen:(id)sender {
    if ([self isInFullScreenMode]) {
        [self.adapter putCommand:"set fullscreen no"];
    } else {
        [self.adapter putCommand:"set fullscreen yes"];
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

- (void)windowDidEndLiveResize:(NSNotification *)notification
{
    [self setFrame:[self constrainFrameRect:self.frame toScreen:self.screen]
           display:NO];
}

- (void)tryDequeueSize {
    if (_queued_video_size.width <= 0.0 || _queued_video_size.height <= 0.0)
        return;

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

- (NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)size {
    return window.screen.frame.size;
}

- (NSApplicationPresentationOptions)window:(NSWindow *)window
      willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)opts {
    return NSApplicationPresentationFullScreen      |
           NSApplicationPresentationAutoHideDock    |
           NSApplicationPresentationAutoHideMenuBar |
           NSApplicationPresentationAutoHideToolbar;
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
    [self tryDequeueSize];
}
@end
