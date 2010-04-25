/*
 * CoreVideo video output driver
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

#import "vo_corevideo.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <unistd.h>
#include <CoreServices/CoreServices.h>
//special workaround for Apple bug #6267445
//(OSServices Power API disabled in OSServices.h for 64bit systems)
#ifndef __POWER__
#include <CoreServices/../Frameworks/OSServices.framework/Headers/Power.h>
#endif

//MPLAYER
#include "config.h"
#include "fastmemcpy.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "aspect.h"
#include "mp_msg.h"
#include "m_option.h"
#include "mp_fifo.h"
#include "libvo/sub.h"
#include "subopt-helper.h"

#include "input/input.h"
#include "input/mouse.h"

#include "osdep/keycodes.h"
#include "osx_common.h"

//Cocoa
NSDistantObject *mplayerosxProxy;
id <MPlayerOSXVOProto> mplayerosxProto;
MPlayerOpenGLView *mpGLView;
NSAutoreleasePool *autoreleasepool;
OSType pixelFormat;

//shared memory
BOOL shared_buffer = false;
#define DEFAULT_BUFFER_NAME "mplayerosx"
static char *buffer_name;

//Screen
int screen_id = -1;
NSRect screen_frame;
NSScreen *screen_handle;
NSArray *screen_array;

//image
unsigned char *image_data;
// For double buffering
static uint8_t image_page = 0;
static unsigned char *image_datas[2];

static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_depth;
static uint32_t image_bytes;
static uint32_t image_format;

//vo
static int isFullscreen;
static int isOntop;
static int isRootwin;

static float winAlpha = 1;
static int int_pause = 0;

static BOOL isLeopardOrLater;

static vo_info_t info =
{
	"Mac OS X Core Video",
	"corevideo",
	"Nicolas Plourde <nicolas.plourde@gmail.com>",
	""
};

LIBVO_EXTERN(corevideo)

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src, unsigned char *srca, int stride)
{
	switch (image_format)
	{
		case IMGFMT_RGB24:
			vo_draw_alpha_rgb24(w,h,src,srca,stride,image_data+3*(y0*image_width+x0),3*image_width);
			break;
		case IMGFMT_ARGB:
		case IMGFMT_BGRA:
			vo_draw_alpha_rgb32(w,h,src,srca,stride,image_data+4*(y0*image_width+x0),4*image_width);
			break;
		case IMGFMT_YUY2:
			vo_draw_alpha_yuy2(w,h,src,srca,stride,image_data + (x0 + y0 * image_width) * 2,image_width*2);
			break;
	}
}

static void update_screen_info(void)
{
	if (screen_id == -1 && xinerama_screen > -1)
		screen_id = xinerama_screen;

	screen_array = [NSScreen screens];
	if(screen_id < (int)[screen_array count])
	{
		screen_handle = [screen_array objectAtIndex:(screen_id < 0 ? 0 : screen_id)];
	}
	else
	{
		mp_msg(MSGT_VO, MSGL_INFO, "[vo_corevideo] Device ID %d does not exist, falling back to main device\n", screen_id);
		screen_handle = [screen_array objectAtIndex:0];
		screen_id = -1;
	}

	screen_frame = ![mpGLView window] || screen_id >= 0 ? [screen_handle frame] : [[[mpGLView window] screen] frame];
	vo_screenwidth = screen_frame.size.width;
	vo_screenheight = screen_frame.size.height;
	xinerama_x = xinerama_y = 0;
	aspect_save_screenres(vo_screenwidth, vo_screenheight);
}

static void free_file_specific(void)
{
	if(shared_buffer)
	{
		[mplayerosxProto stop];
		mplayerosxProto = nil;
		[mplayerosxProxy release];
		mplayerosxProxy = nil;

		if (munmap(image_data, image_width*image_height*image_bytes) == -1)
			mp_msg(MSGT_VO, MSGL_FATAL, "[vo_corevideo] uninit: munmap failed. Error: %s\n", strerror(errno));

		if (shm_unlink(buffer_name) == -1)
			mp_msg(MSGT_VO, MSGL_FATAL, "[vo_corevideo] uninit: shm_unlink failed. Error: %s\n", strerror(errno));
    } else {
        free(image_datas[0]);
        if (vo_doublebuffering)
            free(image_datas[1]);
        image_datas[0] = NULL;
        image_datas[1] = NULL;
        image_data = NULL;
    }
}

static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
	free_file_specific();

	//misc mplayer setup
	image_width = width;
	image_height = height;
	switch (image_format)
	{
		case IMGFMT_RGB24:
			image_depth = 24;
			break;
		case IMGFMT_ARGB:
		case IMGFMT_BGRA:
			image_depth = 32;
			break;
		case IMGFMT_YUY2:
			image_depth = 16;
			break;
	}
	image_bytes = (image_depth + 7) / 8;

	if(!shared_buffer)
	{
		config_movie_aspect((float)d_width/d_height);

		vo_dwidth  = d_width  *= mpGLView->winSizeMult;
		vo_dheight = d_height *= mpGLView->winSizeMult;

		image_data = malloc(image_width*image_height*image_bytes);
		image_datas[0] = image_data;
		if (vo_doublebuffering)
			image_datas[1] = malloc(image_width*image_height*image_bytes);
		image_page = 0;

		vo_fs = flags & VOFLAG_FULLSCREEN;

		//config OpenGL View
		[mpGLView config];
		[mpGLView reshape];
	}
	else
	{
		int shm_fd;
		mp_msg(MSGT_VO, MSGL_INFO, "[vo_corevideo] writing output to a shared buffer "
				"named \"%s\"\n",buffer_name);

		// create shared memory
		shm_fd = shm_open(buffer_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if (shm_fd == -1)
		{
			mp_msg(MSGT_VO, MSGL_FATAL,
				   "[vo_corevideo] failed to open shared memory. Error: %s\n", strerror(errno));
			return 1;
		}


		if (ftruncate(shm_fd, image_width*image_height*image_bytes) == -1)
		{
			mp_msg(MSGT_VO, MSGL_FATAL,
				   "[vo_corevideo] failed to size shared memory, possibly already in use. Error: %s\n", strerror(errno));
			close(shm_fd);
			shm_unlink(buffer_name);
			return 1;
		}

		image_data = mmap(NULL, image_width*image_height*image_bytes,
					PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		close(shm_fd);

		if (image_data == MAP_FAILED)
		{
			mp_msg(MSGT_VO, MSGL_FATAL,
				   "[vo_corevideo] failed to map shared memory. Error: %s\n", strerror(errno));
			shm_unlink(buffer_name);
			return 1;
		}

		//connect to mplayerosx
		mplayerosxProxy=[NSConnection rootProxyForConnectionWithRegisteredName:[NSString stringWithCString:buffer_name] host:nil];
		if ([mplayerosxProxy conformsToProtocol:@protocol(MPlayerOSXVOProto)]) {
			[mplayerosxProxy setProtocolForProxy:@protocol(MPlayerOSXVOProto)];
			mplayerosxProto = (id <MPlayerOSXVOProto>)mplayerosxProxy;
			[mplayerosxProto startWithWidth: image_width withHeight: image_height withBytes: image_bytes withAspect:d_width*100/d_height];
		}
		else {
			[mplayerosxProxy release];
			mplayerosxProxy = nil;
			mplayerosxProto = nil;
		}
	}
	return 0;
}

static void check_events(void)
{
	if (mpGLView)
		[mpGLView check_events];
}

static void draw_osd(void)
{
	vo_draw_text(image_width, image_height, draw_alpha);
}

static void flip_page(void)
{
	if(shared_buffer) {
		NSAutoreleasePool *pool = [NSAutoreleasePool new];
		[mplayerosxProto render];
		[pool release];
	} else {
		[mpGLView setCurrentTexture];
		[mpGLView render];
		if (vo_doublebuffering) {
			image_page = 1 - image_page;
			image_data = image_datas[image_page];
		}
	}
}

static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
	return 0;
}


static int draw_frame(uint8_t *src[])
{
	return 0;
}

static uint32_t draw_image(mp_image_t *mpi)
{
	memcpy_pic(image_data, mpi->planes[0], image_width*image_bytes, image_height, image_width*image_bytes, mpi->stride[0]);

	return 0;
}

static int query_format(uint32_t format)
{
    const int supportflags = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
	image_format = format;

    switch(format)
	{
		case IMGFMT_YUY2:
			pixelFormat = kYUVSPixelFormat;
			return supportflags;

		case IMGFMT_RGB24:
			pixelFormat = k24RGBPixelFormat;
			return supportflags;

		case IMGFMT_ARGB:
			pixelFormat = k32ARGBPixelFormat;
			return supportflags;

		case IMGFMT_BGRA:
			pixelFormat = k32BGRAPixelFormat;
			return supportflags;
    }
    return 0;
}

static void uninit(void)
{
    SetSystemUIMode( kUIModeNormal, 0);
    CGDisplayShowCursor(kCGDirectMainDisplay);

    free_file_specific();

    if(mpGLView)
    {
        NSAutoreleasePool *finalPool;
        mpGLView = nil;
        [autoreleasepool release];
        finalPool = [[NSAutoreleasePool alloc] init];
        [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
        [finalPool release];
    }

    if (buffer_name) free(buffer_name);
    buffer_name = NULL;
}

static const opt_t subopts[] = {
{"device_id",     OPT_ARG_INT,  &screen_id,     NULL},
{"shared_buffer", OPT_ARG_BOOL, &shared_buffer, NULL},
{"buffer_name",   OPT_ARG_MSTRZ,&buffer_name,   NULL},
{NULL}
};

static int preinit(const char *arg)
{

	// set defaults
	screen_id = -1;
	shared_buffer = false;
	buffer_name = NULL;

	if (subopt_parse(arg, subopts) != 0) {
		mp_msg(MSGT_VO, MSGL_FATAL,
				"\n-vo corevideo command line help:\n"
				"Example: mplayer -vo corevideo:device_id=1:shared_buffer:buffer_name=mybuff\n"
				"\nOptions:\n"
				"  device_id=<0-...>\n"
				"    Set screen device ID for fullscreen.\n"
				"  shared_buffer\n"
				"    Write output to a shared memory buffer instead of displaying it.\n"
				"  buffer_name=<name>\n"
				"    Name of the shared buffer created with shm_open() as well as\n"
				"    the name of the NSConnection MPlayer will try to open.\n"
				"    Setting buffer_name implicitly enables shared_buffer.\n"
				"\n" );
		return -1;
	}

	autoreleasepool = [[NSAutoreleasePool alloc] init];

	if (!buffer_name)
		buffer_name = strdup(DEFAULT_BUFFER_NAME);
	else
		shared_buffer = true;

	if(!shared_buffer)
	{
		NSApplicationLoad();
		NSApp = [NSApplication sharedApplication];
		isLeopardOrLater = floor(NSAppKitVersionNumber) > 824;

		osx_foreground_hack();

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
		case VOCTRL_DRAW_IMAGE: return draw_image(data);
		case VOCTRL_PAUSE: return int_pause = 1;
		case VOCTRL_RESUME: return int_pause = 0;
		case VOCTRL_QUERY_FORMAT: return query_format(*((uint32_t*)data));
		case VOCTRL_ONTOP: vo_ontop = (!(vo_ontop)); if(!shared_buffer){ [mpGLView ontop]; } else { [mplayerosxProto ontop]; } return VO_TRUE;
		case VOCTRL_ROOTWIN: vo_rootwin = (!(vo_rootwin)); [mpGLView rootwin]; return VO_TRUE;
		case VOCTRL_FULLSCREEN: vo_fs = (!(vo_fs)); if(!shared_buffer){ [mpGLView fullscreen: NO]; } else { [mplayerosxProto toggleFullscreen]; } return VO_TRUE;
		case VOCTRL_GET_PANSCAN: return VO_TRUE;
		case VOCTRL_SET_PANSCAN: [mpGLView panscan]; return VO_TRUE;
		case VOCTRL_UPDATE_SCREENINFO: update_screen_info(); return VO_TRUE;
	}
	return VO_NOTIMPL;
}

//////////////////////////////////////////////////////////////////////////
// NSOpenGLView Subclass
//////////////////////////////////////////////////////////////////////////
@implementation MPlayerOpenGLView
- (void) preinit
{
	NSOpenGLContext *glContext;
	GLint swapInterval = 1;
	CVReturn error;

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

	//create OpenGL Context
	glContext = [[NSOpenGLContext alloc] initWithFormat:[NSOpenGLView defaultPixelFormat] shareContext:nil];

	[self setOpenGLContext:glContext];
	[glContext setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];
	[glContext setView:self];
	[glContext makeCurrentContext];
	[glContext release];

	error = CVOpenGLTextureCacheCreate(NULL, 0, [glContext CGLContextObj], [[self pixelFormat] CGLPixelFormatObj], 0, &textureCache);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL texture Cache(%d)\n", error);
}

- (void) releaseVideoSpecific
{
	CVPixelBufferRelease(frameBuffers[0]);
	frameBuffers[0] = NULL;
	CVPixelBufferRelease(frameBuffers[1]);
	frameBuffers[1] = NULL;
	CVOpenGLTextureRelease(texture);
	texture = NULL;
}

- (void) dealloc
{
	[self releaseVideoSpecific];
	CVOpenGLTextureCacheRelease(textureCache);
	textureCache = NULL;
	[self setOpenGLContext:nil];
	[super dealloc];
}

- (void) config
{
	NSRect visibleFrame;
	CVReturn error = kCVReturnSuccess;

	//config window
	[window setContentSize:NSMakeSize(vo_dwidth, vo_dheight)];

	// Use visibleFrame to position the window taking the menu bar and dock into account.
	// Also flip vo_dy since the screen origin is in the bottom left on OSX.
	if (screen_id < 0)
		visibleFrame = [[[mpGLView window] screen] visibleFrame];
	else
		visibleFrame = [[[NSScreen screens] objectAtIndex:screen_id] visibleFrame];
	[window setFrameTopLeftPoint:NSMakePoint(
		visibleFrame.origin.x + vo_dx,
		visibleFrame.origin.y + visibleFrame.size.height - vo_dy)];

	[self releaseVideoSpecific];
	error = CVPixelBufferCreateWithBytes(NULL, image_width, image_height, pixelFormat, image_datas[0], image_width*image_bytes, NULL, NULL, NULL, &frameBuffers[0]);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create Pixel Buffer(%d)\n", error);
	if (vo_doublebuffering) {
		error = CVPixelBufferCreateWithBytes(NULL, image_width, image_height, pixelFormat, image_datas[1], image_width*image_bytes, NULL, NULL, NULL, &frameBuffers[1]);
		if(error != kCVReturnSuccess)
			mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create Pixel Double Buffer(%d)\n", error);
	}

	error = CVOpenGLTextureCacheCreateTextureFromImage(NULL, textureCache, frameBuffers[image_page], 0, &texture);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL texture(%d)\n", error);

	//show window
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

- (void)set_winSizeMult:(float)mult
{
    NSRect frame;
    int d_width, d_height;
    aspect(&d_width, &d_height, A_NOZOOM);

    if (isFullscreen) {
        vo_fs = !vo_fs;
        [self fullscreen:NO];
    }

    winSizeMult = mult;
    frame.size.width  = d_width  * mult;
    frame.size.height = d_height * mult;
    [window setContentSize: frame.size];
    [self reshape];
}

/*
	Menu Action
 */
- (void)menuAction:(id)sender
{
	if(sender == kQuitCmd)
	{
		mplayer_put_key(KEY_ESC);
	}

	if(sender == kHalfScreenCmd)
		[self set_winSizeMult: 0.5];
	if(sender == kNormalScreenCmd)
		[self set_winSizeMult: 1];
	if(sender == kDoubleScreenCmd)
		[self set_winSizeMult: 2];
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
		change_movie_aspect(-1);

	if(sender == kAspectFullCmd)
		change_movie_aspect(4.0f/3.0f);

	if(sender == kAspectWideCmd)
		change_movie_aspect(16.0f/9.0f);
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

	NSRect frame = [self frame];
	vo_dwidth  = frame.size.width;
	vo_dheight = frame.size.height;

	glViewport(0, 0, frame.size.width, frame.size.height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, frame.size.width, frame.size.height, 0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//set texture frame
	if(vo_keepaspect)
	{
		aspect( (int *)&d_width, (int *)&d_height, A_WINZOOM);

		textureFrame = NSMakeRect((vo_dwidth - d_width) / 2, (vo_dheight - d_height) / 2, d_width, d_height);
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

	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(CVOpenGLTextureGetTarget(texture));
	glBindTexture(CVOpenGLTextureGetTarget(texture), CVOpenGLTextureGetName(texture));

	glColor3f(1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(upperLeft[0], upperLeft[1]); glVertex2i(	textureFrame.origin.x-(vo_panscan_x >> 1), textureFrame.origin.y-(vo_panscan_y >> 1));
	glTexCoord2f(lowerLeft[0], lowerLeft[1]); glVertex2i(textureFrame.origin.x-(vo_panscan_x >> 1), NSMaxY(textureFrame)+(vo_panscan_y >> 1));
	glTexCoord2f(lowerRight[0], lowerRight[1]); glVertex2i(NSMaxX(textureFrame)+(vo_panscan_x >> 1), NSMaxY(textureFrame)+(vo_panscan_y >> 1));
	glTexCoord2f(upperRight[0], upperRight[1]); glVertex2i(NSMaxX(textureFrame)+(vo_panscan_x >> 1), textureFrame.origin.y-(vo_panscan_y >> 1));
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

	curTime  = TickCount()/60;

	//automatically hide mouse cursor (and future on-screen control?)
	if(isFullscreen && !mouseHide && !isRootwin)
	{
		if( ((curTime - lastMouseHide) >= 5) || (lastMouseHide == 0) )
		{
			CGDisplayHideCursor(kCGDirectMainDisplay);
			mouseHide = TRUE;
			lastMouseHide = curTime;
		}
	}

	//update activity every 30 seconds to prevent
	//screensaver from starting up.
	if( ((curTime - lastScreensaverUpdate) >= 30) || (lastScreensaverUpdate == 0) )
	{
		UpdateSystemActivity(UsrActivity);
		lastScreensaverUpdate = curTime;
	}
}

/*
	Create OpenGL texture from current frame & set texco
*/
- (void) setCurrentTexture
{
	CVReturn error = kCVReturnSuccess;

	CVOpenGLTextureRelease(texture);
	error = CVOpenGLTextureCacheCreateTextureFromImage(NULL, textureCache, frameBuffers[image_page], 0, &texture);
	if(error != kCVReturnSuccess)
		mp_msg(MSGT_VO, MSGL_ERR,"[vo_corevideo] Failed to create OpenGL texture(%d)\n", error);

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
		update_screen_info();

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
	if (event == nil)
		return;
	[NSApp sendEvent:event];
	// Without SDL's bootstrap code (include SDL.h in mplayer.c),
	// on Leopard, we have trouble to get the play window automatically focused
	// when the app is actived. The Following code fix this problem.
#ifndef CONFIG_SDL
	if (isLeopardOrLater && [event type] == NSAppKitDefined
			&& [event subtype] == NSApplicationActivatedEventType) {
		[window makeMainWindow];
		[window makeKeyAndOrderFront:mpGLView];
	}
#endif
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
	int key = convert_key([theEvent keyCode], *[[theEvent characters] UTF8String]);
	if (key != -1)
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
	if (enable_mouse_movements && !isRootwin) {
		NSPoint p =[self convertPoint:[theEvent locationInWindow] fromView:nil];
		if ([self mouse:p inRect:textureFrame]) {
			vo_mouse_movement(vo_fs ? p.x : p.x - textureFrame.origin.x,
			                  vo_fs ? [self frame].size.height - p.y : NSMaxY(textureFrame) - p.y);
		}
	}
}

- (void) mouseDown: (NSEvent *) theEvent
{
	[self mouseEvent: theEvent];
}

- (void) mouseUp: (NSEvent *) theEvent
{
	[self mouseEvent: theEvent];
}

- (void) rightMouseDown: (NSEvent *) theEvent
{
	[self mouseEvent: theEvent];
}

- (void) rightMouseUp: (NSEvent *) theEvent
{
	[self mouseEvent: theEvent];
}

- (void) otherMouseDown: (NSEvent *) theEvent
{
	[self mouseEvent: theEvent];
}

- (void) otherMouseUp: (NSEvent *) theEvent
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
	if ( [theEvent buttonNumber] >= 0 && [theEvent buttonNumber] <= 9 )
	{
		int buttonNumber = [theEvent buttonNumber];
		// Fix to mplayer defined button order: left, middle, right
		if (buttonNumber == 1)
			buttonNumber = 2;
		else if (buttonNumber == 2)
			buttonNumber = 1;
		switch([theEvent type])
		{
			case NSLeftMouseDown:
			case NSRightMouseDown:
			case NSOtherMouseDown:
				mplayer_put_key((MOUSE_BTN0 + buttonNumber) | MP_KEY_DOWN);
				break;
			case NSLeftMouseUp:
			case NSRightMouseUp:
			case NSOtherMouseUp:
				mplayer_put_key(MOUSE_BTN0 + buttonNumber);
				break;
		}
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
