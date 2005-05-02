/*
	vo_macosx.h
	
	by Nicolas Plourde <nicolasplourde@gmail.com>
	
	Copyright (c) Nicolas Plourde - 2005
	
	MPlayer Mac OSX video out module.
*/

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <QuickTime/QuickTime.h>

@interface CustomOpenGLView : NSOpenGLView
{
	//Cocoa
	NSWindow *window;
	NSOpenGLContext *glContext;
	NSEvent *event;
	
	//CoreVideo
	CVPixelBufferRef currentFrameBuffer;
	CVOpenGLTextureCacheRef textureCache;
	CVOpenGLTextureRef texture;
	
    GLfloat	lowerLeft[2]; 
    GLfloat lowerRight[2]; 
    GLfloat upperRight[2];
    GLfloat upperLeft[2];
	
	BOOL mouseHide;
}

- (BOOL) acceptsFirstResponder;
- (BOOL) becomeFirstResponder;
- (BOOL) resignFirstResponder;

//window & rendering
- (void) initOpenGLView;
- (void) prepareOpenGL;
- (void) render;
- (void) reshape;
- (void) setCurrentTexture;
- (void) drawRect: (NSRect *) bounds;
- (void) fullscreen: (BOOL) animate;
- (void) ontop;
- (void) panscan;
- (void) rootwin;

//event
- (void) keyDown: (NSEvent *) theEvent;
- (void) mouseMoved: (NSEvent *) theEvent;
- (void) mouseDown: (NSEvent *) theEvent;
- (void) rightMouseDown: (NSEvent *) theEvent;
- (void) otherMouseDown: (NSEvent *) theEvent;
- (void) scrollWheel: (NSEvent *) theEvent;
- (void) mouseEvent: (NSEvent *) theEvent;
- (void) check_events;
@end