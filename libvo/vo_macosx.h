/*
	vo_macosx.h
	
	by Nicolas Plourde <nicolasplourde@gmail.com>
	
	Copyright (c) Nicolas Plourde - 2005
	
	MPlayer Mac OSX video out module.
*/

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <QuickTime/QuickTime.h>

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
	NSOpenGLContext *glContext;
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
	float winSizeMult;
	
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
