/*
	vo_macosx.m
	by Nicolas Plourde <nicolasplourde@gmail.com>
	
	MPlayer Mac OSX video out module.
 	Copyright (c) Nicolas Plourde - 2005
*/

#import "vo_macosx.h"

//MPLAYER
#include "config.h"
#include "fastmemcpy.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"
#include "mp_msg.h"
#include "m_option.h"

#include "input/input.h"
#include "input/mouse.h"

#include "osdep/keycodes.h"

extern void mplayer_put_key(int code);

//Cocoa
CustomOpenGLView *glView;
NSAutoreleasePool *autoreleasepool;
OSType pixelFormat;

//Device
static int device_width;
static int device_height;
static int device_id;
static GDHandle device_handle;

//image
unsigned char *image_data;
static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_depth;
static uint32_t image_bytes;
static uint32_t image_format;
static NSRect image_rec;

//vo
extern int vo_rootwin;
extern int vo_ontop;
extern int vo_fs;
static int isFullscreen;
extern float monitor_aspect;
extern int vo_keepaspect;
extern float movie_aspect;
static float old_movie_aspect;
extern float vo_panscan;

static int int_pause = 0;

static vo_info_t info = 
{
	"Mac OSX Core Video",
	"macosx",
	"Nicolas Plourde <nicolas.plourde@gmail.com>",
	""
};

LIBVO_EXTERN(macosx)

extern void mplayer_put_key(int code);
extern void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src, unsigned char *srca, int stride)
{
	switch (image_format)
	{
		case IMGFMT_RGB32:
			vo_draw_alpha_rgb32(w,h,src,srca,stride,image_data+4*(y0*image_width+x0),4*image_width);
			break;
		case IMGFMT_YUY2:
			vo_draw_alpha_yuy2(w,h,src,srca,stride,image_data + (x0 + y0 * image_width) * 2,image_width*2);
			break;
	}
}

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
	int i;
	
	//Get Main device info///////////////////////////////////////////////////
	device_handle = GetMainDevice();
	
	for(i=0; i<device_id; i++)
	{
		device_handle = GetNextDevice(device_handle);
		
		if(device_handle == NULL)
		{
			mp_msg(MSGT_VO, MSGL_FATAL, "Get device error: Device ID %d do not exist, falling back to main device.\n", device_id);
			device_handle = GetMainDevice();
			device_id = 0;
			break;
		}
	}
	
	NSRect device_rect = [[NSScreen mainScreen] frame];
	device_width = device_rect.size.width;
	device_height = device_rect.size.height;
	monitor_aspect = (float)device_width/(float)device_height;
	
	//misc mplayer setup
	image_width = width;
	image_height = height;
	switch (image_format) 
	{
		case IMGFMT_BGR32:
		case IMGFMT_RGB32:
			image_depth = 32;
			break;
        case IMGFMT_YUY2:
			image_depth = 16;
			break;
	}
	image_bytes = (image_depth + 7) / 8;
	image_data = (unsigned char*)malloc(image_width*image_height*image_bytes);
	
	//set aspect
	panscan_init();
	aspect_save_orig(width,height);
	aspect_save_prescale(d_width,d_height);
	aspect_save_screenres(device_width,device_height);
	aspect(&d_width,&d_height,A_NOZOOM);
	
	movie_aspect = (float)d_width/(float)d_height;
	old_movie_aspect = movie_aspect;
	
	//init OpenGL View
	glView = [[CustomOpenGLView alloc] initWithFrame:NSMakeRect(0, 0, d_width, d_height) pixelFormat:[CustomOpenGLView defaultPixelFormat]];
	[glView initOpenGLView];
	
	vo_fs = flags & VOFLAG_FULLSCREEN;
	
	if(vo_fs)
		[glView fullscreen: NO];
	
	if(vo_ontop)
		[glView ontop];
	
	return 0;
}

static void check_events(void)
{
	[glView check_events];
	
	//update activity every 60 seconds to prevent
	//screensaver from starting up.
	DateTimeRec d;
	unsigned long curTime;
	static unsigned long lastTime = 0;
	
	GetTime(&d);
	DateToSeconds( &d, &curTime);
	
	if( ( (curTime - lastTime) >= 60) || (lastTime == 0))
	{
		UpdateSystemActivity(UsrActivity);
		lastTime = curTime;
	}
}

static void draw_osd(void)
{
	vo_draw_text(image_width, image_height, draw_alpha);
}

static void flip_page(void)
{
	[glView render];
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
	[glView setCurrentTexture];
	return 0;
}


static uint32_t draw_frame(uint8_t *src[])
{
	switch (image_format)
	{
		case IMGFMT_BGR32:
		case IMGFMT_RGB32:
			memcpy(image_data, src[0], image_width*image_height*image_bytes);
			break;

		case IMGFMT_YUY2:
			memcpy_pic(image_data, src[0], image_width * 2, image_height, image_width * 2, image_width * 2);
			break;
	}
	[glView setCurrentTexture];
	return 0;
}

static uint32_t query_format(uint32_t format)
{
	image_format = format;
	
    switch(format)
	{
		case IMGFMT_YUY2:
			pixelFormat = kYUVSPixelFormat;
			return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
		
		case IMGFMT_RGB32:
		case IMGFMT_BGR32:
			pixelFormat = k32ARGBPixelFormat;
			return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
    }
    return 0;
}

static void uninit(void)
{
	[autoreleasepool release];
}

static uint32_t preinit(const char *arg)
{
	int parse_err = 0;
	
    if(arg) 
    {
        char *parse_pos = (char *)&arg[0];
        while (parse_pos[0] && !parse_err) 
		{
			if (strncmp (parse_pos, "device_id=", 10) == 0)
			{
				parse_pos = &parse_pos[10];
                device_id = strtol(parse_pos, &parse_pos, 0);
            }
            if (parse_pos[0] == ':') parse_pos = &parse_pos[1];
            else if (parse_pos[0]) parse_err = 1;
        }
    }
	
	#if !defined (MACOSX_FINDER_SUPPORT) || !defined (HAVE_SDL)
	//this chunk of code is heavily based off SDL_macosx.m from SDL 
	//it uses an Apple private function to request foreground operation
	void CPSEnableForegroundOperation(ProcessSerialNumber* psn);
	ProcessSerialNumber myProc, frProc;
	Boolean sameProc;
	
	if (GetFrontProcess(&frProc) == noErr)
	{
		if (GetCurrentProcess(&myProc) == noErr)
		{
			if (SameProcess(&frProc, &myProc, &sameProc) == noErr && !sameProc)
			{
				CPSEnableForegroundOperation(&myProc);
			}
			SetFrontProcess(&myProc);
		}
	}
	#endif

	NSApplicationLoad();
	autoreleasepool = [[NSAutoreleasePool alloc] init];
		
    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
	switch (request)
	{
		case VOCTRL_PAUSE: return (int_pause=1);
		case VOCTRL_RESUME: return (int_pause=0);
		case VOCTRL_QUERY_FORMAT: return query_format(*((uint32_t*)data));
		case VOCTRL_ONTOP: vo_ontop = (!(vo_ontop)); [glView ontop]; return VO_TRUE;
		case VOCTRL_FULLSCREEN: vo_fs = (!(vo_fs)); [glView fullscreen: YES]; return VO_TRUE;
		case VOCTRL_GET_PANSCAN: return VO_TRUE;
		case VOCTRL_SET_PANSCAN: [glView panscan]; return VO_TRUE;
	}
	return VO_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////
// NSOpenGLView Subclass
//////////////////////////////////////////////////////////////////////////
@implementation CustomOpenGLView
- (void) initOpenGLView
{
	NSRect frame = [self frame];
	CVReturn error = kCVReturnSuccess;
	
	//create OpenGL Context
	glContext = [[NSOpenGLContext alloc] initWithFormat:[NSOpenGLView defaultPixelFormat] shareContext:nil];	
	
	
	//create window
	window = [[NSWindow alloc]	initWithContentRect:NSMakeRect(0, 0, frame.size.width, frame.size.height) 
								styleMask:NSTitledWindowMask|NSTexturedBackgroundWindowMask|NSClosableWindowMask|NSMiniaturizableWindowMask|NSResizableWindowMask
								backing:NSBackingStoreBuffered
								defer:NO];

	[window setContentView:self];
	[window setInitialFirstResponder:self];
	[window setAcceptsMouseMovedEvents:YES];
    [window setTitle:@"MPlayer - The Movie Player"];
	[window center];
	[window makeKeyAndOrderFront:nil];
	
	[self setOpenGLContext:glContext];
	[glContext setView:self];
	[glContext makeCurrentContext];	
	
	error = CVPixelBufferCreateWithBytes(	NULL,
											image_width, image_height,
											pixelFormat,
											image_data,
											image_width*image_bytes,
											NULL, NULL, NULL,
											&currentFrameBuffer);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"Failed to create Pixel Buffer(%d)\n", error);
	
	error = CVOpenGLTextureCacheCreate(NULL, 0, [glContext CGLContextObj], [[self pixelFormat] CGLPixelFormatObj], 0, &textureCache);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"Failed to create OpenGL texture Cache(%d)\n", error);
	
	error = CVOpenGLTextureCacheCreateTextureFromImage(	NULL, textureCache, currentFrameBuffer, 0, &texture);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"Failed to create OpenGL texture(%d)\n", error);
	
	isFullscreen = 0;
}

/*
	Setup OpenGL
*/
- (void)prepareOpenGL
{
	glDisable(GL_BLEND); 
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	[self reshape];
}

/*
	reshape OpenGL viewport
*/ 
- (void)reshape
{
	uint32_t d_width;
	uint32_t d_height;
	float aspectX;
	float aspectY;
	int padding = 0;
	
	NSRect frame = [self frame];
	
	glViewport(0, 0, frame.size.width, frame.size.height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, frame.size.width, frame.size.height, 0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	//set image_rec
	if(vo_keepaspect)
	{
		aspect( &d_width, &d_height, A_NOZOOM);
		d_height = ((float)d_width/movie_aspect);
		
		aspectX = (float)((float)frame.size.width/(float)d_width);
		aspectY = (float)((float)(frame.size.height)/(float)d_height);
		
		if((d_height*aspectX)>(frame.size.height))
		{
			padding = (frame.size.width - d_width*aspectY)/2;
			image_rec.origin.x = padding;
			image_rec.origin.y = 0;
			image_rec.size.width = d_width*aspectY+padding;
			image_rec.size.height = d_height*aspectY;
		}
		else
		{
			padding = ((frame.size.height) - d_height*aspectX)/2;
			image_rec.origin.x = 0;
			image_rec.origin.y = padding;
			image_rec.size.width = d_width*aspectX;
			image_rec.size.height = d_height*aspectX+padding;
		}
	}
	else
	{
		image_rec = frame;
	}
}

/*
	Render frame
*/ 
- (void) render
{
	glClear(GL_COLOR_BUFFER_BIT);	
	
	glEnable(CVOpenGLTextureGetTarget(texture));
	glBindTexture(CVOpenGLTextureGetTarget(texture), CVOpenGLTextureGetName(texture));
	
	glColor3f(1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(upperLeft[0], upperLeft[1]); glVertex2i(image_rec.origin.x, image_rec.origin.y);
	glTexCoord2f(lowerLeft[0], lowerLeft[1]); glVertex2i(image_rec.origin.x, image_rec.size.height);
	glTexCoord2f(lowerRight[0], lowerRight[1]); glVertex2i(image_rec.size.width, image_rec.size.height);	
	glTexCoord2f(upperRight[0], upperRight[1]); glVertex2i(image_rec.size.width, image_rec.origin.y);
	glEnd();
	
	glFlush();
	
	//auto hide mouse cursor and futur on-screen control?
	if(isFullscreen && !mouseHide)
	{
		DateTimeRec d;
		unsigned long curTime;
		static unsigned long lastTime = 0;
		
		GetTime(&d);
		DateToSeconds( &d, &curTime);
	
		if( ((curTime - lastTime) >= 5) || (lastTime == 0) )
		{
			HideMenuBar();
			HideCursor();
			mouseHide = YES;
			lastTime = curTime;
		}
	}
}

/*
	Create OpenGL texture from current frame & set texco 
*/ 
- (void) setCurrentTexture
{
	CVReturn error = kCVReturnSuccess;
	
	error = CVOpenGLTextureCacheCreateTextureFromImage (NULL, textureCache,  currentFrameBuffer,  0, &texture);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"Failed to create OpenGL texture(%d)\n", error);

    CVOpenGLTextureGetCleanTexCoords(texture, lowerLeft, lowerRight, upperRight, upperLeft);
}

/*
	redraw win rect
*/ 
- (void) drawRect: (NSRect *) bounds
{
	[self render];
}

/*
	Toggle Fullscreen
*/
- (void) fullscreen: (BOOL) animate
{
	static NSRect old_frame;
	static NSRect old_view_frame;
	NSRect device_rect = [[window screen] frame];

	//go fullscreen
	if(vo_fs)
	{
		//hide menubar and mouse if fullscreen on main display
		HideMenuBar();
		HideCursor();
		mouseHide = YES;
		
		panscan_calc();
		old_frame = [window frame];	//save main window size & position
		[window setFrame:device_rect display:YES animate:animate]; //zoom-in window with nice useless sfx
		old_view_frame = [self bounds];
		
		//fix origin for multi screen setup
		device_rect.origin.x = 0;
		device_rect.origin.y = 0;
		[self setFrame:device_rect];
		[self setNeedsDisplay:YES];
		[window setHasShadow:NO];
		isFullscreen = 1;
	}
	else
	{
		isFullscreen = 0;
		ShowMenuBar();
		ShowCursor();
		mouseHide = NO;

		//revert window to previous setting
		[self setFrame:old_view_frame];
		[self setNeedsDisplay:YES];
		[window setHasShadow:NO];
		[window setFrame:old_frame display:YES animate:animate];//zoom-out window with nice useless sfx
	}
}

/*
	Toggle ontop
*/
- (void) ontop
{
}

/*
	Toggle panscan
*/
- (void) panscan
{
}

/*
	Check event for new event
*/ 
- (void) check_events
{
	event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate dateWithTimeIntervalSinceNow:0.0001] inMode:NSEventTrackingRunLoopMode dequeue:YES];
	[NSApp sendEvent:event];
}

/*
	Process key event
*/
- (void) keyDown: (NSEvent *) theEvent
{
	unsigned int key;
	
	switch([theEvent keyCode])
    {
		case 0x34:
		case 0x24: key = KEY_ENTER; break;
		case 0x35: key = KEY_ESC; break;
		case 0x33: key = KEY_BACKSPACE; break;
		case 0x3A: key = KEY_BACKSPACE; break;
		case 0x3B: key = KEY_BACKSPACE; break;
		case 0x38: key = KEY_BACKSPACE; break;
		case 0x7A: key = KEY_F+1; break;
		case 0x78: key = KEY_F+2; break;
		case 0x63: key = KEY_F+3; break;
		case 0x76: key = KEY_F+4; break;
		case 0x60: key = KEY_F+5; break;
		case 0x61: key = KEY_F+6; break;
		case 0x62: key = KEY_F+7; break;
		case 0x64: key = KEY_F+8; break;
		case 0x65: key = KEY_F+9; break;
		case 0x6D: key = KEY_F+10; break;
		case 0x67: key = KEY_F+11; break;
		case 0x6F: key = KEY_F+12; break;
		case 0x72: key = KEY_INSERT; break;
		case 0x75: key = KEY_DELETE; break;
		case 0x73: key = KEY_HOME; break;
		case 0x77: key = KEY_END; break;
		case 0x45: key = '+'; break;
		case 0x4E: key = '-'; break;
		case 0x30: key = KEY_TAB; break;
		case 0x74: key = KEY_PAGE_UP; break;
		case 0x79: key = KEY_PAGE_DOWN; break;  
		case 0x7B: key = KEY_LEFT; break;
		case 0x7C: key = KEY_RIGHT; break;
		case 0x7D: key = KEY_DOWN; break;
		case 0x7E: key = KEY_UP; break;
		case 0x43: key = '*'; break;
		case 0x4B: key = '/'; break;
		case 0x4C: key = KEY_BACKSPACE; break;
		case 0x41: key = KEY_KPDEC; break;
		case 0x52: key = KEY_KP0; break;
		case 0x53: key = KEY_KP1; break;
		case 0x54: key = KEY_KP2; break;
		case 0x55: key = KEY_KP3; break;
		case 0x56: key = KEY_KP4; break;
		case 0x57: key = KEY_KP5; break;
		case 0x58: key = KEY_KP6; break;
		case 0x59: key = KEY_KP7; break;
		case 0x5B: key = KEY_KP8; break;
		case 0x5C: key = KEY_KP9; break;
		default: key = *[[theEvent characters] UTF8String]; break;
    }
	mplayer_put_key(key);
}

/*
	Process mouse button event
*/
- (void) mouseMoved: (NSEvent *) theEvent
{
	if(isFullscreen)
	{
		ShowMenuBar();
		ShowCursor();
		mouseHide = NO;
	}
}

- (void) mouseDown: (NSEvent *) theEvent
{
	[self mouseEvent: theEvent];
}

- (void) rightMouseDown: (NSEvent *) theEvent
{
	[self mouseEvent: theEvent];
}

- (void) otherMouseDown: (NSEvent *) theEvent
{
	[self mouseEvent: theEvent];
}

- (void) scrollWheel: (NSEvent *) theEvent
{
	if([theEvent deltaY] > 0)
		mplayer_put_key(MOUSE_BTN3);
	else
		mplayer_put_key(MOUSE_BTN4);
}

- (void) mouseEvent: (NSEvent *) theEvent
{
	switch( [theEvent buttonNumber] )
	{ 
		case 0: mplayer_put_key(MOUSE_BTN0);break;
		case 1: mplayer_put_key(MOUSE_BTN1);break;
		case 2: mplayer_put_key(MOUSE_BTN2);break;
	}
}

/*
	NSResponder
*/ 
- (BOOL) acceptsFirstResponder
{
	return YES;
}

- (BOOL) becomeFirstResponder
{
	return YES;
}

- (BOOL) resignFirstResponder
{
	return YES;
}
@end