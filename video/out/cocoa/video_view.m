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

#include "osdep/macosx_compat.h"
#include "video/out/cocoa_common.h"
#include "video_view.h"

@implementation MpvVideoView
@synthesize adapter = _adapter;

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    }
    return self;
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [self.adapter setNeedsResize];
}

- (NSRect)frameInPixels
{
    return [self convertRectToBacking:[self frame]];
}


- (void)drawRect:(NSRect)rect
{
    [self.adapter performAsyncResize:[self frameInPixels].size];
}
@end
