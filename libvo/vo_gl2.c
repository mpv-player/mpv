#define DISP

// this can be 3 or 4  (regarding 24bpp and 32bpp)
#define BYTES_PP 3

#define TEXTUREFORMAT_32BPP

/* 
 * video_out_gl.c, X11/OpenGL interface
 * based on video_out_x11 by Aaron Holtzman,
 * and WS opengl window manager by Pontscho/Fresh!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"


LIBVO_EXTERN(gl2)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
//#include <X11/keysym.h>
#include <GL/glx.h>
#include <errno.h>
#include "yuv2rgb.h"

#include <GL/gl.h>

#include "x11_common.h"
#include "aspect.h"

#define NDEBUG
// #undef NDEBUG

static vo_info_t vo_info = 
{
	"X11 (OpenGL)",
	"gl2",
	"Arpad Gereoffy <arpi@esp-team.scene.hu>",
	""
};

/* private prototypes */
// static void Display_Image (unsigned char *ImageData);

/* local data */
static unsigned char *ImageData=NULL;

/* X11 related variables */
//static Display *mydisplay;
static Window mywindow;
//static GC mygc;
//static XImage *myximage;
//static int depth,mode;
//static XWindowAttributes attribs;
static int X_already_started = 0;

//static int texture_id=1;

static GLXContext wsGLXContext;
//XVisualInfo        * wsVisualInfo;
static int                  wsGLXAttrib[] = { GLX_RGBA,
                                       GLX_RED_SIZE,1,
                                       GLX_GREEN_SIZE,1,
                                       GLX_BLUE_SIZE,1,
//                                       GLX_DEPTH_SIZE,16,
                                       GLX_DOUBLEBUFFER,
                                       None };


static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;
static uint32_t image_bpp;
static uint32_t image_bytes;

static uint32_t texture_width;
static uint32_t texture_height;
static int texnumx, texnumy, memory_x_len, memory_x_start_offset, raw_line_len;
static GLfloat texpercx, texpercy;
static struct TexSquare * texgrid;

/* The squares that are tiled to make up the game screen polygon */

struct TexSquare
{
  GLubyte *texture;
  GLuint texobj;
  int isTexture;
  GLfloat fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
  GLfloat xcov, ycov;
};

static void CalcFlatPoint(int x,int y,GLfloat *px,GLfloat *py)
{
  *px=(float)x*texpercx;
  if(*px>1.0) *px=1.0;
  *py=(float)y*texpercy;
  if(*py>1.0) *py=1.0;
}

static void initTextures()
{
  unsigned char *line_1=0, *line_2=0, *mem_start=0;
  struct TexSquare *tsq=0;
  int e_x, e_y, s, i=0;
  int x=0, y=0;
  GLint format=0;
  GLenum err;

  /* achieve the 2**e_x:=texture_width, 2**e_y:=texture_height */
  e_x=0; s=1;
  while (s<texture_width)
  { s*=2; e_x++; }
  texture_width=s;

  e_y=0; s=1;
  while (s<texture_height)
  { s*=2; e_y++; }
  texture_height=s;


  /* Test the max texture size */
  do
  {
    glTexImage2D (GL_PROXY_TEXTURE_2D, 0,
		  BYTES_PP,
		  texture_width, texture_height,
		  0, (image_bytes==4)?GL_RGBA:GL_BGR, GL_UNSIGNED_BYTE, NULL); 

    glGetTexLevelParameteriv
      (GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);

    if (format != BYTES_PP)
    {
      fprintf (stderr, "GLINFO: Needed texture [%dx%d] too big, trying ",
		texture_height, texture_width);

      if (texture_width > texture_height)
      {
	e_x--;
	texture_width = 1;
	for (i = e_x; i > 0; i--)
	  texture_width *= 2;
      }
      else
      {
	e_y--;
	texture_height = 1;
	for (i = e_y; i > 0; i--)
	  texture_height *= 2;
      }

      fprintf (stderr, "[%dx%d] !\n", texture_height, texture_width);

      if(texture_width < 64 || texture_height < 64)
      {
      	fprintf (stderr, "GLERROR: Give up .. usable texture size not avaiable, or texture config error !\n");
	exit(1);
      }
    }
  }
  while (format != BYTES_PP && texture_width > 1 && texture_height > 1);

  texnumx = image_width / texture_width;
  if ((image_width % texture_width) > 0)
    texnumx++;

  texnumy = image_height / texture_height;
  if ((image_height % texture_height) > 0)
    texnumy++;

  /* Allocate the texture memory */

  texpercx = (GLfloat) texture_width / (GLfloat) image_width;
  if (texpercx > 1.0)
    texpercx = 1.0;

  texpercy = (GLfloat) texture_height / (GLfloat) image_height;
  if (texpercy > 1.0)
    texpercy = 1.0;

  texgrid = (struct TexSquare *)
    calloc (texnumx * texnumy, sizeof (struct TexSquare));

  line_1 = (unsigned char *) ImageData;
  line_2 = (unsigned char *) ImageData+(image_width*image_bytes);

  mem_start = (unsigned char *) ImageData;

  raw_line_len = line_2 - line_1;

  memory_x_len = raw_line_len / image_bytes;

#ifndef NDEBUG
  fprintf (stderr, "GLINFO: texture-usage %d*width=%d, %d*height=%d\n",
		 (int) texnumx, (int) texture_width, (int) texnumy,
		 (int) texture_height);
#endif

  for (y = 0; y < texnumy; y++)
  {
    for (x = 0; x < texnumx; x++)
    {
      tsq = texgrid + y * texnumx + x;

      if (x == texnumx - 1 && image_width % texture_width)
	tsq->xcov =
	  (GLfloat) (image_width % texture_width) / (GLfloat) texture_width;
      else
	tsq->xcov = 1.0;

      if (y == texnumy - 1 && image_height % texture_height)
	tsq->ycov =
	  (GLfloat) (image_height % texture_height) / (GLfloat) texture_height;
      else
	tsq->ycov = 1.0;

      CalcFlatPoint (x, y, &(tsq->fx1), &(tsq->fy1));
      CalcFlatPoint (x + 1, y, &(tsq->fx2), &(tsq->fy2));
      CalcFlatPoint (x + 1, y + 1, &(tsq->fx3), &(tsq->fy3));
      CalcFlatPoint (x, y + 1, &(tsq->fx4), &(tsq->fy4));

      /* calculate the pixel store data,
         to use the machine-bitmap for our texture 
      */
      memory_x_start_offset = 0 * image_bytes + 
                              x * texture_width * image_bytes;

      tsq->texture = line_1 +                           
		     y * texture_height * raw_line_len +  
		     memory_x_start_offset;           

      tsq->isTexture=GL_FALSE;
      tsq->texobj=0;

      glGenTextures (1, &(tsq->texobj));

      glBindTexture (GL_TEXTURE_2D, tsq->texobj);
      err = glGetError ();
      if(err==GL_INVALID_ENUM)
      {
	fprintf (stderr, "GLERROR glBindTexture (glGenText) := GL_INVALID_ENUM, texnum x=%d, y=%d, texture=%d\n", x, y, tsq->texobj);
      } 

      if(glIsTexture(tsq->texobj) == GL_FALSE)
      {
	fprintf (stderr, "GLERROR ain't a texture (glGenText): texnum x=%d, y=%d, texture=%d\n",
		x, y, tsq->texobj);
      } else {
        tsq->isTexture=GL_TRUE;
      }

      glTexImage2D (GL_TEXTURE_2D, 0,
		    BYTES_PP,
		    texture_width, texture_height,
		    0, (image_bytes==4)?GL_RGBA:GL_BGR, GL_UNSIGNED_BYTE, NULL); 
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1.0);

      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

      glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    }	/* for all texnumx */
  }  /* for all texnumy */
}

static void drawTextureDisplay ()
{
  struct TexSquare *square;
  int x, y, xoff=0, yoff=0, wd, ht;
  GLenum err;

  glColor3f(1.0,1.0,1.0);

  for (y = 0; y < texnumy; y++)
  {
    for (x = 0; x < texnumx; x++)
    {
      square = texgrid + y * texnumx + x;

      if(square->isTexture==GL_FALSE)
      {
	#ifndef NDEBUG
	  fprintf (stderr, "GLINFO ain't a texture(update): texnum x=%d, y=%d, texture=%d\n",
	  	x, y, square->texobj);
	#endif
      	continue;
      }

      glBindTexture (GL_TEXTURE_2D, square->texobj);
      err = glGetError ();
      if(err==GL_INVALID_ENUM)
      {
	fprintf (stderr, "GLERROR glBindTexture := GL_INVALID_ENUM, texnum x=%d, y=%d, texture=%d\n", x, y, square->texobj);
      }
      #ifndef NDEBUG
	      else if(err==GL_INVALID_OPERATION) {
		fprintf (stderr, "GLERROR glBindTexture := GL_INVALID_OPERATION, texnum x=%d, y=%d, texture=%d\n", x, y, square->texobj);
	      }
      #endif

      if(glIsTexture(square->texobj) == GL_FALSE)
      {
        square->isTexture=GL_FALSE;
	fprintf (stderr, "GLERROR ain't a texture(update): texnum x=%d, y=%d, texture=%d\n",
		x, y, square->texobj);
      }

      /* This is the quickest way I know of to update the texture */
	if (x < texnumx - 1)
	  wd = texture_width;
	else
	  wd = image_width - texture_width * x;

	if (y < texnumy - 1)
	  ht = texture_height;
	else
	  ht = image_height - texture_height * y;

	glTexSubImage2D (GL_TEXTURE_2D, 0, 
		 xoff, yoff,
		 wd, ht,
		 (BYTES_PP==4)?GL_RGBA:GL_RGB,        // format
		 GL_UNSIGNED_BYTE, // type
		 square->texture);

#ifndef NDEBUG
        fprintf (stdout, "GLINFO glTexSubImage2D texnum x=%d, y=%d, %d/%d - %d/%d\n", x, y, xoff, yoff, wd, ht);
#endif

	glBegin(GL_QUADS);

	glTexCoord2f (0, 0);
	glVertex2f (square->fx1, square->fy1);

	glTexCoord2f (0, square->ycov);
	glVertex2f (square->fx4, square->fy4);

	glTexCoord2f (square->xcov, square->ycov);
	glVertex2f (square->fx3, square->fy3);

	glTexCoord2f (square->xcov, 0);
	glVertex2f (square->fx2, square->fy2);

#ifndef NDEBUG
        fprintf (stdout, "GLINFO GL_QUADS texnum x=%d, y=%d, %f/%f %f/%f %f/%f %f/%f\n\n", x, y, square->fx1, square->fy1, square->fx4, square->fy4,
	square->fx3, square->fy3, square->fx2, square->fy2);
#endif

   /*
    glTexCoord2f(0,0);glVertex2i(0,0);
    glTexCoord2f(0,1);glVertex2i(0,texture_height);
    glTexCoord2f(1,1);glVertex2i(texture_width,texture_height);
    glTexCoord2f(1,0);glVertex2i(texture_width,0);
   */

	glEnd();
    } /* for all texnumx */
  } /* for all texnumy */

  /* YES - lets catch this error ... 
   */
  (void) glGetError ();
}


static void resize(int x,int y){
  printf("[gl] Resize: %dx%d\n",x,y);
  glViewport( 0, 0, x, y );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  //glOrtho(0, image_width, image_height, 0, -1,1);
  glOrtho (0, 1, 1, 0, -1.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t 
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
//	int screen;
        int dwidth,dheight;
	unsigned int fg, bg;
	char *hello = (title == NULL) ? "OpenGL rulez" : title;
//	char *name = ":0.0";
	XSizeHints hint;
	XVisualInfo *vinfo;
	XEvent xev;

//	XGCValues xgcv;
	XSetWindowAttributes xswa;
	unsigned long xswamask;

	image_height = height;
	image_width = width;
	image_format = format;
  
	if (X_already_started) return -1;
	if(!vo_init()) return -1;

	X_already_started++;

        dwidth=d_width; dheight=d_height;
#ifdef X11_FULLSCREEN
        if( flags&0x01 ){ // (-fs)
          aspect(&d_width,&d_height,vo_screenwidth,vo_screenheight);
          dwidth=d_width; dheight=d_height;
        }
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

//	XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);

//	XMatchVisualInfo(mDisplay, screen, depth, TrueColor, &vinfo);
  vinfo=glXChooseVisual( mDisplay,mScreen,wsGLXAttrib );
  if (vinfo == NULL)
  {
    printf("[gl] no GLX support present\n");
    return -1;
  }

	xswa.background_pixel = 0;
	xswa.border_pixel     = 1;
//	xswa.colormap         = XCreateColormap(mDisplay, mRootWin, vinfo.visual, AllocNone);
	xswa.colormap         = XCreateColormap(mDisplay, mRootWin, vinfo->visual, AllocNone);
	xswamask = CWBackPixel | CWBorderPixel | CWColormap;
//  xswamask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask | CWCursor | CWOverrideRedirect | CWSaveUnder | CWX | CWY | CWWidth | CWHeight;

  mywindow = XCreateWindow(mDisplay, RootWindow(mDisplay,mScreen),
    hint.x, hint.y, hint.width, hint.height, 4, vinfo->depth,CopyFromParent,vinfo->visual,xswamask,&xswa);

  vo_x11_classhint( mDisplay,mywindow,"gl" );
  vo_hidecursor(mDisplay,mywindow);

  wsGLXContext=glXCreateContext( mDisplay,vinfo,NULL,True );
//  XStoreName( wsDisplay,wsMyWin,wsSysName );

//  printf("GLXcontext ok\n");

  if ( flags&0x01 ) vo_x11_decoration( mDisplay,mywindow,0 );

	XSelectInput(mDisplay, mywindow, StructureNotifyMask);

	/* Tell other applications about this window */

	XSetStandardProperties(mDisplay, mywindow, hello, hello, None, NULL, 0, &hint);

	/* Map window. */

	XMapWindow(mDisplay, mywindow);

	/* Wait for map. */
	do 
	{
		XNextEvent(mDisplay, &xev);
	}
	while (xev.type != MapNotify || xev.xmap.event != mywindow);

	XSelectInput(mDisplay, mywindow, NoEventMask);

  glXMakeCurrent( mDisplay,mywindow,wsGLXContext );

	XFlush(mDisplay);
	XSync(mDisplay, False);

//	mygc = XCreateGC(mDisplay, mywindow, 0L, &xgcv);

//		myximage = XGetImage(mDisplay, mywindow, 0, 0,
//		width, image_height, AllPlanes, ZPixmap);
//		ImageData = myximage->data;
//	bpp = myximage->bits_per_pixel;

	//XSelectInput(mDisplay, mywindow, StructureNotifyMask); // !!!!
        XSelectInput(mDisplay, mywindow, StructureNotifyMask | KeyPressMask );

//  printf("Window setup ok\n");

#if 0
	// If we have blue in the lowest bit then obviously RGB 
	mode = ((myximage->blue_mask & 0x01) != 0) ? MODE_RGB : MODE_BGR;
#ifdef WORDS_BIGENDIAN 
	if (myximage->byte_order != MSBFirst)
#else
	if (myximage->byte_order != LSBFirst) 
#endif
	{
		printf("[gl] no support for non-native XImage byte order!\n");
		return -1;
	}

  printf("DEPTH=%d  BPP=%d\n",depth,bpp);
#endif

	/* 
	 * If depth is 24 then it may either be a 3 or 4 byte per pixel
	 * format. We can't use bpp because then we would lose the 
	 * distinction between 15/16bit depth (2 byte formate assumed).
	 *
	 * FIXME - change yuv2rgb_init to take both depth and bpp
	 * parameters
	 */

  if(format==IMGFMT_YV12){
    yuv2rgb_init(8*BYTES_PP, MODE_BGR);
    printf("[gl] YUV init OK!\n");
    image_bpp=8*BYTES_PP;
    image_bytes=BYTES_PP;
  } else {
    image_bpp=format&0xFF;
    image_bytes=(image_bpp+7)/8;
  }

  ImageData=malloc(image_width*image_height*image_bytes);
  memset(ImageData,128,image_width*image_height*image_bytes);

  texture_width=image_width;
  texture_height=image_height;
  initTextures();

  printf("[gl] Creating %dx%d texture...\n",texture_width,texture_height);

  glDisable(GL_BLEND); 
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);

  glPixelStorei (GL_UNPACK_ROW_LENGTH, memory_x_len);
//  glPixelStorei (GL_UNPACK_ALIGNMENT, 8); // causes non-12*n wide images to be broken
  glEnable (GL_TEXTURE_2D);

  drawTextureDisplay ();

  printf("[gl] Creating %dx%d texture...\n",texture_width,texture_height);

/*
#if 1
//  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#ifdef TEXTUREFORMAT_32BPP
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texture_width, texture_height, 0,
#else
  glTexImage2D(GL_TEXTURE_2D, 0, BYTES_PP, texture_width, texture_height, 0,
#endif
       (image_bytes==4)?GL_RGBA:GL_BGR, GL_UNSIGNED_BYTE, NULL);
#endif
*/

  resize(d_width,d_height);

  glClearColor( 1.0f,0.0f,1.0f,0.0f );
  glClear( GL_COLOR_BUFFER_BIT );

//  printf("OpenGL setup OK!\n");

      saver_off(mDisplay);  // turning off screen saver

	return 0;
}

static const vo_info_t*
get_info(void)
{
	return &vo_info;
}

static void 
Terminate_Display_Process(void) 
{
	getchar();	/* wait for enter to remove window */
	XDestroyWindow(mDisplay, mywindow);
	XCloseDisplay(mDisplay);
	X_already_started = 0;
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

  drawTextureDisplay();

//  glFlush();
  glFinish();
  glXSwapBuffers( mDisplay,mywindow );
  
}

//static inline uint32_t draw_slice_x11(uint8_t *src[], uint32_t slice_num)
static uint32_t draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
    int i;
    int dstride=w*BYTES_PP;
    
//    dstride=(dstride+15)&(~15);

	yuv2rgb(ImageData+y*raw_line_len, src[0], src[1], src[2], 
			w,h, dstride, stride[0],stride[1]);

//	emms ();

#ifndef NDEBUG
     printf("slice: %d/%d -> %d/%d (%dx%d)\n", 
	x, y, x+w-1, y+h-1, w, h);
#endif

	return 0;
}

static inline uint32_t 
draw_frame_x11_bgr(uint8_t *src[])
{
uint8_t *s=src[0];
uint8_t *d=ImageData;
uint8_t *de=d+3*image_width*image_height;
int i;

      // RGB->BGR
      while(d<de){
#if 1
        d[0]=s[2];
        d[1]=s[1];
        d[2]=s[0];
        s+=3;d+=3;
#else
	// R1 G1 B1 R2   G2 B2
	// B1 G1 R1 B2   G2 R2

        unsigned int a=*((unsigned int*)s);
	unsigned short b=*((unsigned short*)(s+4));
	*((unsigned int*)d)=((a>>16)&0xFF)|(a&0xFF00)|((a&0xFF)<<16)|((b>>8)<<24);
	*((unsigned short*)(d+4))=(b&0xFF)|((a>>24)<<8);
	s+=6;d+=6;
#endif
      }

    for(i=0;i<image_height;i++) ImageData[image_width*BYTES_PP*i+20]=128;

//     printf("draw_frame_x11_bgr\n");
//    drawTextureDisplay ();

//	Display_Image(ImageData);
	return 0; 
}

static inline uint32_t 
draw_frame_x11_rgb(uint8_t *src[])
{
int i;
uint8_t *ImageData=src[0];

     printf("draw_frame_x11_rgb not implemented\n");
//    drawTextureDisplay ();

//	Display_Image(ImageData);
	return 0; 
}


static uint32_t
draw_frame(uint8_t *src[])
{
    uint32_t res = 0;

    if((image_format&IMGFMT_RGB_MASK)==IMGFMT_RGB)
	res = draw_frame_x11_rgb(src);
    else
	res = draw_frame_x11_bgr(src);

    //flip_page();
    return res;
}

static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
    case IMGFMT_RGB|24:
    case IMGFMT_BGR|24:
        return 1;
    }
    return 0;
}


static void
uninit(void)
{
  saver_on(mDisplay); // screen saver back on
  XDestroyWindow( mDisplay,mywindow );
}
