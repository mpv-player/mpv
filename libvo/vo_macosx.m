/*
	vo_macosx.m
	by Nicolas Plourde <nicolasplourde@gmail.com>
	
	MPlayer Mac OSX video out module.
 	Copyright (c) Nicolas Plourde - 2005
*/

#import "vo_macosx.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

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

//Cocoa
NSProxy *mplayerosxProxy;
MPlayerOpenGLView *mpGLView;
NSAutoreleasePool *autoreleasepool;
OSType pixelFormat;

//shared memory
int shm_id;
struct shmid_ds shm_desc;
BOOL shared_buffer = false;

//Screen
int screen_id;
BOOL screen_force;
NSRect screen_frame;
NSScreen *screen_handle;
NSArray *screen_array;

//image
unsigned char *image_data;
static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_depth;
static uint32_t image_bytes;
static uint32_t image_format;

//vo
extern int vo_rootwin;
extern int vo_ontop;
extern int vo_fs;
static int isFullscreen;
static int isOntop;
static int isRootwin;
extern float monitor_aspect;
extern int vo_keepaspect;
extern float movie_aspect;
static float old_movie_aspect;
extern float vo_panscan;

static float winAlpha = 1;
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

static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
	int i;
	
	//init screen
	screen_array = [NSScreen screens];
	if(screen_id < [screen_array count])
	{
		screen_handle = [screen_array objectAtIndex:screen_id];
	}
	else
	{
		mp_msg(MSGT_VO, MSGL_FATAL, "Get device error: Device ID %d do not exist, falling back to main device.\n", screen_id);
		screen_handle = [screen_array objectAtIndex:0];
		screen_id = 0;
	}
	screen_frame = [screen_handle frame];

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
	image_data = malloc(image_width*image_height*image_bytes);
		
	if(!shared_buffer)
	{		
		monitor_aspect = (float)screen_frame.size.width/(float)screen_frame.size.height;
		
		//set aspect
		panscan_init();
		aspect_save_orig(width,height);
		aspect_save_prescale(d_width,d_height);
		aspect_save_screenres(screen_frame.size.width, screen_frame.size.height);
		aspect((int *)&d_width,(int *)&d_height,A_NOZOOM);
		
		movie_aspect = (float)d_width/(float)d_height;
		old_movie_aspect = movie_aspect;
		
		vo_fs = flags & VOFLAG_FULLSCREEN;
			
		//config OpenGL View
		[mpGLView config];
		[mpGLView reshape];
	}
	else
	{
		movie_aspect = (float)d_width/(float)d_height;
				
		shm_id = shmget(9849, image_width*image_height*image_bytes, IPC_CREAT | 0666);
		if (shm_id == -1)
		{
			perror("vo_mplayer shmget: ");
			return 1;
		}
		
		image_data = shmat(shm_id, NULL, 0);
		if (!image_data)
		{	
			perror("vo_mplayer shmat: ");
			return 1;
		}
		
		//connnect to mplayerosx
		mplayerosxProxy=[NSConnection rootProxyForConnectionWithRegisteredName:@"mplayerosx" host:nil];
		[mplayerosxProxy startWithWidth: image_width withHeight: image_height withBytes: image_bytes withAspect:(int)(movie_aspect*100)];
	}
	return 0;
}

static void check_events(void)
{
	[mpGLView check_events];
}

static void draw_osd(void)
{
	vo_draw_text(image_width, image_height, draw_alpha);
}

static void flip_page(void)
{
	if(shared_buffer)
		[mplayerosxProxy render];
	else
		[mpGLView render];
}

static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
	[mpGLView setCurrentTexture];
	return 0;
}


static int draw_frame(uint8_t *src[])
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
	
	if(!shared_buffer)
		[mpGLView setCurrentTexture];
	
	return 0;
}

static int query_format(uint32_t format)
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
	if(shared_buffer)
	{
		[mplayerosxProxy stop];

		if (shmdt(image_data) == -1)
			mp_msg(MSGT_VO, MSGL_FATAL, "uninit: shmdt failed\n");
	
		if (shmctl(shm_id, IPC_RMID, &shm_desc) == -1)
			mp_msg(MSGT_VO, MSGL_FATAL, "uninit: shmctl failed\n");
	}

    SetSystemUIMode( kUIModeNormal, 0);
    CGDisplayShowCursor(kCGDirectMainDisplay);
    
    if(mpGLView)
    {
        [autoreleasepool release];
    }
}

static int preinit(const char *arg)
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
				screen_id = strtol(parse_pos, &parse_pos, 0);
				screen_force = YES;
            }
			if (strncmp (parse_pos, "shared_buffer", 13) == 0)
			{
				parse_pos = &parse_pos[13];
				shared_buffer = YES;
            }
            if (parse_pos[0] == ':') parse_pos = &parse_pos[1];
            else if (parse_pos[0]) parse_err = 1;
        }
    }

	NSApplicationLoad();
	autoreleasepool = [[NSAutoreleasePool alloc] init];
	NSApp = [NSApplication sharedApplication];
	
	if(!shared_buffer)
	{
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

		if(!mpGLView)
		{
			mpGLView = [[MPlayerOpenGLView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) pixelFormat:[MPlayerOpenGLView defaultPixelFormat]];
			[mpGLView autorelease];
		}
	
		[mpGLView display];
		[mpGLView preinit];
	}
	
    return 0;
}

static int control(uint32_t request, void *data, ...)
{
	switch (request)
	{
		case VOCTRL_PAUSE: return (int_pause=1);
		case VOCTRL_RESUME: return (int_pause=0);
		case VOCTRL_QUERY_FORMAT: return query_format(*((uint32_t*)data));
		case VOCTRL_ONTOP: vo_ontop = (!(vo_ontop)); [mpGLView ontop]; return VO_TRUE;
		case VOCTRL_ROOTWIN: vo_rootwin = (!(vo_rootwin)); [mpGLView rootwin]; return VO_TRUE;
		case VOCTRL_FULLSCREEN: vo_fs = (!(vo_fs)); [mpGLView fullscreen: NO]; return VO_TRUE;
		case VOCTRL_GET_PANSCAN: return VO_TRUE;
		case VOCTRL_SET_PANSCAN: [mpGLView panscan]; return VO_TRUE;
	}
	return VO_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////
// NSOpenGLView Subclass
//////////////////////////////////////////////////////////////////////////
@implementation MPlayerOpenGLView
- (id) preinit
{
	//init menu
	[self initMenu];
	
	//create window
	window = [[NSWindow alloc]	initWithContentRect:NSMakeRect(0, 0, 100, 100) 
								styleMask:NSTitledWindowMask|NSTexturedBackgroundWindowMask|NSClosableWindowMask|NSMiniaturizableWindowMask|NSResizableWindowMask
								backing:NSBackingStoreBuffered defer:NO];

	[window autorelease];
	[window setDelegate:mpGLView];
	[window setContentView:mpGLView];
	[window setInitialFirstResponder:mpGLView];
	[window setAcceptsMouseMovedEvents:YES];
    [window setTitle:@"MPlayer - The Movie Player"];
	
	isFullscreen = 0;
	winSizeMult = 1;
}

- (id) config
{
	uint32_t d_width;
	uint32_t d_height;
	
	long swapInterval = 1;
	
	NSRect frame;
	CVReturn error = kCVReturnSuccess;
	
	//config window
	aspect((int *)&d_width, (int *)&d_height,A_NOZOOM);
	frame = NSMakeRect(0, 0, d_width, d_height);
	[window setContentSize: frame.size];
	
	//create OpenGL Context
	glContext = [[NSOpenGLContext alloc] initWithFormat:[NSOpenGLView defaultPixelFormat] shareContext:nil];	
	
	[self setOpenGLContext:glContext];
	[glContext setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];
	[glContext setView:self];
	[glContext makeCurrentContext];	
	
	error = CVPixelBufferCreateWithBytes( NULL, image_width, image_height, pixelFormat, image_data, image_width*image_bytes, NULL, NULL, NULL, &currentFrameBuffer);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"Failed to create Pixel Buffer(%d)\n", error);
	
	error = CVOpenGLTextureCacheCreate(NULL, 0, [glContext CGLContextObj], [[self pixelFormat] CGLPixelFormatObj], 0, &textureCache);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"Failed to create OpenGL texture Cache(%d)\n", error);
	
	error = CVOpenGLTextureCacheCreateTextureFromImage(	NULL, textureCache, currentFrameBuffer, 0, &texture);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"Failed to create OpenGL texture(%d)\n", error);
	
	//show window
	[window center];
	[window makeKeyAndOrderFront:mpGLView];
	
	if(vo_rootwin)
		[mpGLView rootwin];	

	if(vo_fs)
		[mpGLView fullscreen: NO];
	
	if(vo_ontop)
		[mpGLView ontop];
}

/*
	Init Menu
*/
- (void)initMenu
{
	NSMenu *menu, *aspectMenu;
	NSMenuItem *menuItem;
	
	[NSApp setMainMenu:[[NSMenu alloc] init]];

//Create Movie Menu
	menu = [[NSMenu alloc] initWithTitle:@"Movie"];
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Half Size" action:@selector(menuAction:) keyEquivalent:@"0"]; [menu addItem:menuItem];
	kHalfScreenCmd = menuItem;
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Normal Size" action:@selector(menuAction:) keyEquivalent:@"1"]; [menu addItem:menuItem];
	kNormalScreenCmd = menuItem;
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Double Size" action:@selector(menuAction:) keyEquivalent:@"2"]; [menu addItem:menuItem];
	kDoubleScreenCmd = menuItem;
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Full Size" action:@selector(menuAction:) keyEquivalent:@"f"]; [menu addItem:menuItem];
	kFullScreenCmd = menuItem;
	menuItem = (NSMenuItem *)[NSMenuItem separatorItem]; [menu addItem:menuItem];
	
		aspectMenu = [[NSMenu alloc] initWithTitle:@"Aspect Ratio"];
		menuItem = [[NSMenuItem alloc] initWithTitle:@"Keep" action:@selector(menuAction:) keyEquivalent:@""]; [aspectMenu addItem:menuItem];
		if(vo_keepaspect) [menuItem setState:NSOnState];
		kKeepAspectCmd = menuItem;
		menuItem = [[NSMenuItem alloc] initWithTitle:@"Pan-Scan" action:@selector(menuAction:) keyEquivalent:@""]; [aspectMenu addItem:menuItem];
		if(vo_panscan) [menuItem setState:NSOnState];
		kPanScanCmd = menuItem;
		menuItem = (NSMenuItem *)[NSMenuItem separatorItem]; [aspectMenu addItem:menuItem];
		menuItem = [[NSMenuItem alloc] initWithTitle:@"Original" action:@selector(menuAction:) keyEquivalent:@""]; [aspectMenu addItem:menuItem];
		kAspectOrgCmd = menuItem;
		menuItem = [[NSMenuItem alloc] initWithTitle:@"4:3" action:@selector(menuAction:) keyEquivalent:@""]; [aspectMenu addItem:menuItem];
		kAspectFullCmd = menuItem;
		menuItem = [[NSMenuItem alloc] initWithTitle:@"16:9" action:@selector(menuAction:) keyEquivalent:@""];	[aspectMenu addItem:menuItem];
		kAspectWideCmd = menuItem;
		menuItem = [[NSMenuItem alloc] initWithTitle:@"Aspect Ratio" action:nil keyEquivalent:@""];
		[menuItem setSubmenu:aspectMenu];
		[menu addItem:menuItem];
		[aspectMenu release];
	
	//Add to menubar
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Movie" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:menu];
	[[NSApp mainMenu] addItem:menuItem];
	
//Create Window Menu
	menu = [[NSMenu alloc] initWithTitle:@"Window"];
	
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"]; [menu addItem:menuItem];
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""]; [menu addItem:menuItem];

	//Add to menubar
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:menu];
	[[NSApp mainMenu] addItem:menuItem];
	[NSApp setWindowsMenu:menu];
	
	[menu release];
	[menuItem release];
}

/*
	Menu Action
 */
- (void)menuAction:(id)sender
{
	uint32_t d_width;
	uint32_t d_height;
	NSRect frame;
	
	aspect((int *)&d_width, (int *)&d_height,A_NOZOOM);
	
	if(sender == kQuitCmd)
	{
		mplayer_put_key(KEY_ESC);
	}
	
	if(sender == kHalfScreenCmd)
	{
		if(isFullscreen) {
			vo_fs = (!(vo_fs)); [self fullscreen:NO];
		}
		
		winSizeMult = 0.5;
		frame.size.width = (d_width*winSizeMult);
		frame.size.height = ((d_width/movie_aspect)*winSizeMult);
		[window setContentSize: frame.size];
		[self reshape];
	}
	if(sender == kNormalScreenCmd)
	{
		if(isFullscreen) {
			vo_fs = (!(vo_fs)); [self fullscreen:NO];
		}
		
		winSizeMult = 1;
		frame.size.width = d_width;
		frame.size.height = d_width/movie_aspect;
		[window setContentSize: frame.size];
		[self reshape];
	}
	if(sender == kDoubleScreenCmd)
	{
		if(isFullscreen) {
			vo_fs = (!(vo_fs)); [self fullscreen:NO];
		}
		
		winSizeMult = 2;
		frame.size.width = d_width*winSizeMult;
		frame.size.height = (d_width/movie_aspect)*winSizeMult;
		[window setContentSize: frame.size];
		[self reshape];
	}
	if(sender == kFullScreenCmd)
	{
		vo_fs = (!(vo_fs));
		[self fullscreen:NO];
	}

	if(sender == kKeepAspectCmd)
	{
		vo_keepaspect = (!(vo_keepaspect));
		if(vo_keepaspect)
			[kKeepAspectCmd setState:NSOnState];
		else
			[kKeepAspectCmd setState:NSOffState];
			
		[self reshape];
	}
	
	if(sender == kPanScanCmd)
	{
		vo_panscan = (!(vo_panscan));
		if(vo_panscan)
			[kPanScanCmd setState:NSOnState];
		else
			[kPanScanCmd setState:NSOffState];
			
		[self panscan];
	}
	
	if(sender == kAspectOrgCmd)
	{
		movie_aspect = old_movie_aspect;
		
		if(isFullscreen)
		{
			[self reshape];
		}
		else
		{
			frame.size.width = d_width*winSizeMult;
			frame.size.height = (d_width/movie_aspect)*winSizeMult;
			[window setContentSize: frame.size];
			[self reshape];
		}
	}
	
	if(sender == kAspectFullCmd)
	{
		movie_aspect = 4.0f/3.0f;
		
		if(isFullscreen)
		{
			[self reshape];
		}
		else
		{
			frame.size.width = d_width*winSizeMult;
			frame.size.height = (d_width/movie_aspect)*winSizeMult;
			[window setContentSize: frame.size];
			[self reshape];
		}
	}
		
	if(sender == kAspectWideCmd)
	{
		movie_aspect = 16.0f/9.0f;

		if(isFullscreen)
		{
			[self reshape];
		}
		else
		{
			frame.size.width = d_width*winSizeMult;
			frame.size.height = (d_width/movie_aspect)*winSizeMult;
			[window setContentSize: frame.size];
			[self reshape];
		}
	}
}

/*
	Setup OpenGL
*/
- (void)prepareOpenGL
{
	glEnable(GL_BLEND); 
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
	
	//set texture frame
	if(vo_keepaspect)
	{
		aspect( (int *)&d_width, (int *)&d_height, A_NOZOOM);
		d_height = ((float)d_width/movie_aspect);
		
		aspectX = (float)((float)frame.size.width/(float)d_width);
		aspectY = (float)((float)(frame.size.height)/(float)d_height);
		
		if((d_height*aspectX)>(frame.size.height))
		{
			padding = (frame.size.width - d_width*aspectY)/2;
			textureFrame = NSMakeRect(padding, 0, d_width*aspectY+padding, d_height*aspectY);
		}
		else
		{
			padding = ((frame.size.height) - d_height*aspectX)/2;
			textureFrame = NSMakeRect(0, padding, d_width*aspectX, d_height*aspectX+padding);
		}
	}
	else
	{
		textureFrame = frame;
	}
}

/*
	Render frame
*/ 
- (void) render
{
	int curTime;
	static int lastTime;

	glClear(GL_COLOR_BUFFER_BIT);	
	
	glEnable(CVOpenGLTextureGetTarget(texture));
	glBindTexture(CVOpenGLTextureGetTarget(texture), CVOpenGLTextureGetName(texture));
	
	glColor3f(1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(upperLeft[0], upperLeft[1]); glVertex2i(	textureFrame.origin.x-(vo_panscan_x >> 1), textureFrame.origin.y-(vo_panscan_y >> 1));
	glTexCoord2f(lowerLeft[0], lowerLeft[1]); glVertex2i(	textureFrame.origin.x-(vo_panscan_x >> 1), textureFrame.size.height+(vo_panscan_y >> 1));
	glTexCoord2f(lowerRight[0], lowerRight[1]); glVertex2i(	textureFrame.size.width+(vo_panscan_x >> 1), textureFrame.size.height+(vo_panscan_y >> 1));
	glTexCoord2f(upperRight[0], upperRight[1]); glVertex2i(	textureFrame.size.width+(vo_panscan_x >> 1), textureFrame.origin.y-(vo_panscan_y >> 1));
	glEnd();
	glDisable(CVOpenGLTextureGetTarget(texture));
	
	//render resize box
	if(!isFullscreen)
	{
		NSRect frame = [self frame];
		
		glBegin(GL_LINES);
		glColor4f(0.2, 0.2, 0.2, 0.5);
		glVertex2i(frame.size.width-1, frame.size.height-1); glVertex2i(frame.size.width-1, frame.size.height-1);
		glVertex2i(frame.size.width-1, frame.size.height-5); glVertex2i(frame.size.width-5, frame.size.height-1);
		glVertex2i(frame.size.width-1, frame.size.height-9); glVertex2i(frame.size.width-9, frame.size.height-1);

		glColor4f(0.4, 0.4, 0.4, 0.5);
		glVertex2i(frame.size.width-1, frame.size.height-2); glVertex2i(frame.size.width-2, frame.size.height-1);
		glVertex2i(frame.size.width-1, frame.size.height-6); glVertex2i(frame.size.width-6, frame.size.height-1);
		glVertex2i(frame.size.width-1, frame.size.height-10); glVertex2i(frame.size.width-10, frame.size.height-1);
		
		glColor4f(0.6, 0.6, 0.6, 0.5);
		glVertex2i(frame.size.width-1, frame.size.height-3); glVertex2i(frame.size.width-3, frame.size.height-1);
		glVertex2i(frame.size.width-1, frame.size.height-7); glVertex2i(frame.size.width-7, frame.size.height-1);
		glVertex2i(frame.size.width-1, frame.size.height-11); glVertex2i(frame.size.width-11, frame.size.height-1);
		glEnd();
	}
	
	glFlush();
	
	//auto hide mouse cursor and futur on-screen control?
	if(isFullscreen && !mouseHide && !isRootwin)
	{
		int curTime = TickCount()/60;
		static int lastTime = 0;
		
		if( ((curTime - lastTime) >= 5) || (lastTime == 0) )
		{
			CGDisplayHideCursor(kCGDirectMainDisplay);
			mouseHide = YES;
			lastTime = curTime;
		}
	}
	
	//update activity every 30 seconds to prevent
	//screensaver from starting up.
	curTime  = TickCount()/60;
	lastTime = 0;
		
	if( ((curTime - lastTime) >= 30) || (lastTime == 0) )
	{
		UpdateSystemActivity(UsrActivity);
		lastTime = curTime;
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
	
	if(screen_force)
		screen_frame = [screen_handle frame];
	else
		screen_frame = [[window screen] frame];

	panscan_calc();
			
	//go fullscreen
	if(vo_fs)
	{
		if(!isRootwin)
		{
			SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
			CGDisplayHideCursor(kCGDirectMainDisplay);
			mouseHide = YES;
		}
		
		old_frame = [window frame];	//save main window size & position
		[window setFrame:screen_frame display:YES animate:animate]; //zoom-in window with nice useless sfx
		old_view_frame = [self bounds];
		
		//fix origin for multi screen setup
		screen_frame.origin.x = 0;
		screen_frame.origin.y = 0;
		[self setFrame:screen_frame];
		[self setNeedsDisplay:YES];
		[window setHasShadow:NO];
		isFullscreen = 1;
	}
	else
	{	
		SetSystemUIMode( kUIModeNormal, 0);
		
		isFullscreen = 0;
		CGDisplayShowCursor(kCGDirectMainDisplay);
		mouseHide = NO;

		//revert window to previous setting
		[self setFrame:old_view_frame];
		[self setNeedsDisplay:YES];
		[window setHasShadow:YES];
		[window setFrame:old_frame display:YES animate:animate];//zoom-out window with nice useless sfx
	}
}

/*
	Toggle ontop
*/
- (void) ontop
{
	if(vo_ontop)
	{
		[window setLevel:NSScreenSaverWindowLevel];
		isOntop = YES;
	}
	else
	{
		[window setLevel:NSNormalWindowLevel];
		isOntop = NO;
	}
}

/*
	Toggle panscan
*/
- (void) panscan
{
	panscan_calc();
}

/*
	Toggle rootwin
 */
- (void) rootwin
{
	if(vo_rootwin)
	{
		[window setLevel:CGWindowLevelForKey(kCGDesktopWindowLevelKey)];
		[window orderBack:self];
		isRootwin = YES;
	}
	else
	{
		[window setLevel:NSNormalWindowLevel];
		isRootwin = NO;
	}
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
	From NSView, respond to key equivalents.
*/
- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
	switch([theEvent keyCode])
    {
		case 0x21: [window setAlphaValue: winAlpha-=0.05]; return YES;
		case 0x1e: [window setAlphaValue: winAlpha+=0.05]; return YES;
    }
	return NO;
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
	if(isFullscreen && !isRootwin)
	{
		CGDisplayShowCursor(kCGDirectMainDisplay);
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

- (void)windowWillClose:(NSNotification *)aNotification
{
    mpGLView = NULL;
	mplayer_put_key(KEY_ESC);
}
@end
