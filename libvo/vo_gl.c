#define TEXTUREFORMAT_32BPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
//#include <X11/keysym.h>
#include <GL/glx.h>
#include <errno.h>

#include <GL/gl.h>

#include "x11_common.h"
#include "aspect.h"

static vo_info_t info = 
{
	"X11 (OpenGL)",
	"gl",
	"Arpad Gereoffy <arpi@esp-team.scene.hu>",
	""
};

LIBVO_EXTERN(gl)

/* local data */
static unsigned char *ImageData=NULL;

static GLXContext wsGLXContext;
static int                  wsGLXAttrib[] = { GLX_RGBA,
                                       GLX_RED_SIZE,1,
                                       GLX_GREEN_SIZE,1,
                                       GLX_BLUE_SIZE,1,
                                       GLX_DOUBLEBUFFER,
                                       None };


static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_bytes;

static uint32_t texture_width;
static uint32_t texture_height;

static int slice_height=1;

static void resize(int x,int y){
  printf("[gl] Resize: %dx%d\n",x,y);
  glViewport( 0, 0, x, y );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, image_width, image_height, 0, -1,1);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t 
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
//	int screen;
	unsigned int fg, bg;
	XSizeHints hint;
	XVisualInfo *vinfo;
	XEvent xev;

//	XGCValues xgcv;
	XSetWindowAttributes xswa;
	unsigned long xswamask;

	image_height = height;
	image_width = width;
  
	aspect_save_orig(width,height);
	aspect_save_prescale(d_width,d_height);
	aspect_save_screenres(vo_screenwidth,vo_screenheight);

	aspect(&d_width,&d_height,A_NOZOOM);
#ifdef X11_FULLSCREEN
//        if( flags&0x01 ){ // (-fs)
//          aspect(&d_width,&d_height,A_ZOOM);
//        }
#endif
	hint.x = 0;
	hint.y = 0;
	hint.width = d_width;
	hint.height = d_height;
	hint.flags = PPosition | PSize;

	/* Get some colors */

	bg = WhitePixel(mDisplay, mScreen);
	fg = BlackPixel(mDisplay, mScreen);

	/* Make the window */

  vinfo=glXChooseVisual( mDisplay,mScreen,wsGLXAttrib );
  if (vinfo == NULL)
  {
    printf("[gl] no GLX support present\n");
    return -1;
  }

	xswa.background_pixel = 0;
	xswa.border_pixel     = 1;
	xswa.colormap         = XCreateColormap(mDisplay, mRootWin, vinfo->visual, AllocNone);
	xswamask = CWBackPixel | CWBorderPixel | CWColormap;

	if ( vo_window == None )
	 {
      vo_window = XCreateWindow(mDisplay, mRootWin,
        hint.x, hint.y, hint.width, hint.height, 4, vinfo->depth,CopyFromParent,vinfo->visual,xswamask,&xswa);

      vo_x11_classhint( mDisplay,vo_window,"gl" );
      vo_hidecursor(mDisplay,vo_window);

//      if ( flags&0x01 ) vo_x11_decoration( mDisplay,vo_window,0 );
	  XSelectInput(mDisplay, vo_window, StructureNotifyMask);
	  /* Tell other applications about this window */
	  XSetStandardProperties(mDisplay, vo_window, title, title, None, NULL, 0, &hint);
	  /* Map window. */
	  XMapWindow(mDisplay, vo_window);
	  if ( flags&1 ) vo_x11_fullscreen();
#ifdef HAVE_XINERAMA
	  vo_x11_xinerama_move(mDisplay,vo_window);
#endif

	  /* Wait for map. */
	  do 
	  {
		XNextEvent(mDisplay, &xev);
	  }
	  while (xev.type != MapNotify || xev.xmap.event != vo_window);

	  XSelectInput(mDisplay, vo_window, NoEventMask);
	 }

	if ( vo_config_count ) glXDestroyContext( mDisplay,wsGLXContext );
    wsGLXContext=glXCreateContext( mDisplay,vinfo,NULL,True );
    glXMakeCurrent( mDisplay,vo_window,wsGLXContext );

	XFlush(mDisplay);
	XSync(mDisplay, False);

	vo_x11_selectinput_witherr(mDisplay, vo_window, StructureNotifyMask | KeyPressMask | PointerMotionMask
		     | ButtonPressMask | ButtonReleaseMask
        );

  texture_width=32;
  while(texture_width<image_width || texture_width<image_height) texture_width*=2;
  texture_height=texture_width;

  image_bytes=(IMGFMT_RGB_DEPTH(format)+7)/8;

  if ( ImageData ) free( ImageData );
  ImageData=malloc(texture_width*texture_height*image_bytes);
  memset(ImageData,128,texture_width*texture_height*image_bytes);

  glDisable(GL_BLEND); 
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);

  glEnable(GL_TEXTURE_2D);

  printf("[gl] Creating %dx%d texture...\n",texture_width,texture_height);

#if 1
//  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  /* Old OpenGL 1.0 used the third parameter (known as internalFormat) as an
     integer, which indicated the bytes per pixel (bpp). Later in OpenGL 1.1
     they switched to constants, like GL_RGB8. GL_RGB8 means 8 bits for each
     channel (R,G,B), so it's equal to RGB24. It should be safe to pass the
     image_bytes to internalFormat with newer OpenGL versions.
     Anyway, I'm leaving this so as it was, it doesn't hurt, as OpenGL 1.1 is
     about 10 years old too. -- alex
  */
#ifdef TEXTUREFORMAT_32BPP
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texture_width, texture_height, 0,
#else
  glTexImage2D(GL_TEXTURE_2D, 0, image_bytes, texture_width, texture_height, 0,
#endif
       (image_bytes==4)?GL_RGBA:GL_BGR, GL_UNSIGNED_BYTE, ImageData);
#endif

  resize(d_width,d_height);

  glClearColor( 1.0f,0.0f,1.0f,0.0f );
  glClear( GL_COLOR_BUFFER_BIT );

//  printf("OpenGL setup OK!\n");

      saver_off(mDisplay);  // turning off screen saver

	return 0;
}

static void check_events(void)
{
    int e=vo_x11_check_events(mDisplay);
    if(e&VO_EVENT_RESIZE) resize(vo_dwidth,vo_dheight);
}

static void draw_osd(void)
{
}

static void
flip_page(void)
{

//  glEnable(GL_TEXTURE_2D);
//  glBindTexture(GL_TEXTURE_2D, texture_id);

  glColor3f(1,1,1);
  glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2i(0,0);
    glTexCoord2f(0,1);glVertex2i(0,texture_height);
    glTexCoord2f(1,1);glVertex2i(texture_width,texture_height);
    glTexCoord2f(1,0);glVertex2i(texture_width,0);
  glEnd();

//  glFlush();
  glFinish();
  glXSwapBuffers( mDisplay,vo_window );
 
}

//static inline uint32_t draw_slice_x11(uint8_t *src[], uint32_t slice_num)
static uint32_t draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
	return 0;
}


static uint32_t
draw_frame(uint8_t *src[])
{
int i;
uint8_t *ImageData=src[0];

    for(i=0;i<image_height;i+=slice_height){
      glTexSubImage2D( GL_TEXTURE_2D,  // target
		       0,              // level
		       0,              // x offset
//		       image_height-1-i,  // y offset
		       i,  // y offset
		       image_width,    // width
		       (i+slice_height<=image_height)?slice_height:image_height-i,              // height
		       (image_bytes==4)?GL_RGBA:GL_RGB,        // format
		       GL_UNSIGNED_BYTE, // type
		       ImageData+i*image_bytes*image_width );        // *pixels
    }

	return 0; 
}

static uint32_t
query_format(uint32_t format)
{
    if ((format == IMGFMT_RGB24) || (format == IMGFMT_RGB32))
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
    return 0;
}


static void
uninit(void)
{
  if ( !vo_config_count ) return;
  saver_on(mDisplay); // screen saver back on
  vo_x11_uninit();
}

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	    slice_height = atoi(arg);
	    if (slice_height <= 0)
		    slice_height = 65536;
    }
    else
    {
	    slice_height = 4;
    }
    printf("[vo_gl] Using %d as slice_height (0 means image_height).\n", slice_height);

    if( !vo_init() ) return -1; // Can't open X11

    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen();
    return VO_TRUE;
  }
  return VO_NOTIMPL;
}
