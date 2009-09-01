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

@interface MPlayerOpenGLView : NSOpenGLView
{
	//Cocoa
	NSWindow *window;
	NSEvent *event;

	//CoreVideo
	CVPixelBufferRef frameBuffers[2];
	CVOpenGLTextureCacheRef textureCache;
	CVOpenGLTextureRef texture;
	NSRect textureFrame;

    GLfloat	lowerLeft[2];
    GLfloat lowerRight[2];
    GLfloat upperRight[2];
    GLfloat upperLeft[2];

	BOOL mouseHide;

	//menu command id
	NSMenuItem *kQuitCmd;
	NSMenuItem *kHalfScreenCmd;
	NSMenuItem *kNormalScreenCmd;
	NSMenuItem *kDoubleScreenCmd;
	NSMenuItem *kFullScreenCmd;
	NSMenuItem *kKeepAspectCmd;
	NSMenuItem *kAspectOrgCmd;
	NSMenuItem *kAspectFullCmd;
	NSMenuItem *kAspectWideCmd;
	NSMenuItem *kPanScanCmd;

	//timestamps for disabling screensaver and mouse hiding
	int lastMouseHide;
	int lastScreensaverUpdate;
@public
	float winSizeMult;
}

- (BOOL) acceptsFirstResponder;
- (BOOL) becomeFirstResponder;
- (BOOL) resignFirstResponder;

//window & rendering
- (void) preinit;
- (void) config;
- (void) prepareOpenGL;
- (void) render;
- (void) reshape;
- (void) setCurrentTexture;
- (void) drawRect: (NSRect *) bounds;

//vo control
- (void) fullscreen: (BOOL) animate;
- (void) ontop;
- (void) panscan;
- (void) rootwin;

//menu
- (void) initMenu;
- (void) menuAction:(id)sender;

//event
- (void) keyDown: (NSEvent *) theEvent;
- (void) mouseMoved: (NSEvent *) theEvent;
- (void) mouseDown: (NSEvent *) theEvent;
- (void) mouseUp: (NSEvent *) theEvent;
- (void) rightMouseDown: (NSEvent *) theEvent;
- (void) rightMouseUp: (NSEvent *) theEvent;
- (void) otherMouseDown: (NSEvent *) theEvent;
- (void) otherMouseUp: (NSEvent *) theEvent;
- (void) scrollWheel: (NSEvent *) theEvent;
- (void) mouseEvent: (NSEvent *) theEvent;
- (void) check_events;
@end
