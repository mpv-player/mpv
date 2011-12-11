/*
 * CoreVideo video output driver
 *
 * Copyright (c) 2005 Nicolas Plourde <nicolasplourde@gmail.com>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_VO_COREVIDEO_H
#define MPLAYER_VO_COREVIDEO_H

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <Carbon/Carbon.h>

// MPlayer OS X VO Protocol
@protocol MPlayerOSXVOProto
- (int) startWithWidth: (bycopy int)width
            withHeight: (bycopy int)height
             withBytes: (bycopy int)bytes
            withAspect: (bycopy int)aspect;
- (void) stop;
- (void) render;
- (void) toggleFullscreen;
- (void) ontop;
@end

#endif /* MPLAYER_VO_COREVIDEO_H */
