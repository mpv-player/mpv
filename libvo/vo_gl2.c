#define DISP

/* 
 * video_out_gl.c, X11/OpenGL interface
 * based on video_out_x11 by Aaron Holtzman,
 * and WS opengl window manager by Pontscho/Fresh!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN(gl2)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
//#include <X11/keysym.h>
#include <GL/glx.h>
#include <errno.h>
#include "../postproc/rgb2rgb.h"

#include <GL/gl.h>

#include "x11_common.h"
#include "aspect.h"

#define NDEBUG
//#undef NDEBUG

static vo_info_t vo_info = 
{
	"X11 (OpenGL) - multiple textures version",
	"gl2",
	"Arpad Gereoffy & Sven Goethel",
	""
};

/* private prototypes */

static const char * tweaks_used =
#ifdef HAVE_MMX
	"mmx_bpp"
#else
	"none"
#endif
	;

/* local data */
static unsigned char *ImageDataLocal=NULL;
static unsigned char *ImageData=NULL;

/* X11 related variables */
static Window mywindow;
static int X_already_started = 0;

//static int texture_id=1;

static GLXContext wsGLXContext;
static int                  wsGLXAttrib[] = { GLX_RGBA,
                                       GLX_RED_SIZE,1,
                                       GLX_GREEN_SIZE,1,
                                       GLX_BLUE_SIZE,1,
                                       GLX_ALPHA_SIZE,0,
                                       GLX_DOUBLEBUFFER,
                                       None };


static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;
static uint32_t image_bpp;
static int      image_mode;
static uint32_t image_bytes;

static uint32_t texture_width;
static uint32_t texture_height;
static int texnumx, texnumy, memory_x_len, memory_x_start_offset, raw_line_len;
static GLfloat texpercx, texpercy;
static struct TexSquare * texgrid;
static GLint    gl_internal_format;
static char *   gl_internal_format_s;
static int      rgb_sz, r_sz, g_sz, b_sz, a_sz;
static GLint    gl_bitmap_format;
static char *   gl_bitmap_format_s;
static GLint    gl_bitmap_type;
static char *   gl_bitmap_type_s;
static int      gl_alignment;
static int      isGL12 = GL_FALSE;
static int      isFullscreen = GL_FALSE;

static int      gl_bilinear=1;
static int      gl_antialias=0;

static int      used_s=0, used_r=0, used_b=0, used_info_done=0;

static void (*draw_alpha_fnc)
                 (int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

/* The squares that are tiled to make up the game screen polygon */

struct TexSquare
{
  GLubyte *texture;
  GLuint texobj;
  int isTexture;
  GLfloat fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
  GLfloat xcov, ycov;
  int isDirty;
  int dirtyXoff, dirtyYoff, dirtyWidth, dirtyHeight;
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
		  gl_internal_format,
		  texture_width, texture_height,
		  0, gl_bitmap_format, gl_bitmap_type, NULL); 

    glGetTexLevelParameteriv
      (GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);

    if (format != gl_internal_format)
    {
      fprintf (stderr, "[gl2] Needed texture [%dx%d] too big, trying ",
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
  while (format != gl_internal_format && texture_width > 1 && texture_height > 1);

  texnumx = image_width / texture_width;
  if ((image_width % texture_width) > 0)
    texnumx++;

  texnumy = image_height / texture_height;
  if ((image_height % texture_height) > 0)
    texnumy++;

  printf("[gl2] Creating %dx%d textures of size %dx%d ...\n",
	texnumx, texnumy, texture_width,texture_height);

  /* Allocate the texture memory */

  texpercx = (GLfloat) texture_width / (GLfloat) image_width;
  if (texpercx > 1.0)
    texpercx = 1.0;

  texpercy = (GLfloat) texture_height / (GLfloat) image_height;
  if (texpercy > 1.0)
    texpercy = 1.0;

  texgrid = (struct TexSquare *)
    calloc (texnumx * texnumy, sizeof (struct TexSquare));

  line_1 = (unsigned char *) ImageDataLocal;
  line_2 = (unsigned char *) ImageDataLocal+(image_width*image_bytes);

  mem_start = (unsigned char *) ImageDataLocal;

  raw_line_len = line_2 - line_1;

  memory_x_len = raw_line_len / image_bytes;

#ifndef NDEBUG
  fprintf (stderr, "[gl2] texture-usage %d*width=%d, %d*height=%d\n",
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

      tsq->isDirty=GL_TRUE;
      tsq->isTexture=GL_FALSE;
      tsq->texobj=0;
      tsq->dirtyXoff=0; tsq->dirtyYoff=0; tsq->dirtyWidth=-1; tsq->dirtyHeight=-1;

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
		    gl_internal_format,
		    texture_width, texture_height,
		    0, gl_bitmap_format, gl_bitmap_type, NULL); 

      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1.0);

      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

      glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    }	/* for all texnumx */
  }  /* for all texnumy */
}

static void resetTexturePointers(unsigned char *imageSource)
{
  unsigned char *line_1=0, *line_2=0, *mem_start=0;
  struct TexSquare *tsq=0;
  int x=0, y=0;

  line_1 = (unsigned char *) imageSource;
  line_2 = (unsigned char *) imageSource+(image_width*image_bytes);

  mem_start = (unsigned char *) imageSource;

  for (y = 0; y < texnumy; y++)
  {
    for (x = 0; x < texnumx; x++)
    {
      tsq = texgrid + y * texnumx + x;

      /* calculate the pixel store data,
         to use the machine-bitmap for our texture 
      */
      memory_x_start_offset = 0 * image_bytes + 
                              x * texture_width * image_bytes;

      tsq->texture = line_1 +                           
		     y * texture_height * raw_line_len +  
		     memory_x_start_offset;           

    }	/* for all texnumx */
  }  /* for all texnumy */
}

static void setupTextureDirtyArea(int x, int y, int w,int h)
{
  struct TexSquare *square;
  int xi, yi, wd, ht, wh, hh;
  int wdecr, hdecr, xh, yh;
    
  wdecr=w; hdecr=h; xh=x; yh=y;

  for (yi = 0; hdecr>0 && yi < texnumy; yi++)
  {
    if (yi < texnumy - 1)
      ht = texture_height;
    else
      ht = image_height - texture_height * yi;

    xh =x;
    wdecr =w;

    for (xi = 0; wdecr>0 && xi < texnumx; xi++)
    {
        square = texgrid + yi * texnumx + xi;

	if (xi < texnumx - 1)
	  wd = texture_width;
	else
	  wd = image_width - texture_width * xi;

	if( 0 <= xh && xh < wd &&
            0 <= yh && yh < ht
          )
        {
        	square->isDirty=GL_TRUE;

		wh=(wdecr<wd)?wdecr:wd-xh;
		if(wh<0) wh=0;

		hh=(hdecr<ht)?hdecr:ht-yh;
		if(hh<0) hh=0;

/*
#ifndef NDEBUG
     printf("\t %dx%d, %d/%d (%dx%d): %d/%d (%dx%d)\n", 
	xi, yi, xh, yh, wdecr, hdecr, xh, yh, wh, hh);
#endif
*/

		if(xh<square->dirtyXoff)
			square->dirtyXoff=xh;

		if(yh<square->dirtyYoff)
			square->dirtyYoff=yh;

		square->dirtyWidth = wd-square->dirtyXoff;
		square->dirtyHeight = ht-square->dirtyYoff;
		
		wdecr-=wh;

		if ( xi == texnumx - 1 )
			hdecr-=hh;
        }

	xh-=wd;
	if(xh<0) xh=0;
    }
    yh-=ht;
    if(yh<0) yh=0;
  }
}

static void gl_set_bilinear (int val)
{
  int x, y;

  if(val>=0)
	  gl_bilinear = val;
  else 
	  gl_bilinear++;

  gl_bilinear=gl_bilinear%2;
  /* no mipmap yet .. */

  for (y = 0; y < texnumy; y++)
  {
      for (x = 0; x < texnumx; x++)
      {
        glBindTexture (GL_TEXTURE_2D, texgrid[y * texnumx + x].texobj);

	switch (gl_bilinear)
	{
		case 0:
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
				GL_NEAREST);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
				GL_NEAREST);
			printf("[gl2] bilinear off\n");
			break;
		case 1:
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
				GL_LINEAR);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
				GL_LINEAR);
			printf("[gl2] bilinear linear\n");
			break;
		case 2:
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
				GL_LINEAR_MIPMAP_NEAREST);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
				GL_LINEAR_MIPMAP_NEAREST);
			printf("[gl2] bilinear mipmap nearest\n");
			break;
		case 3:
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
				GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
				GL_LINEAR_MIPMAP_LINEAR);
			printf("[gl2] bilinear mipmap linear\n");
			break;
        }
      }
  }
  fflush(0);
}

static void gl_set_antialias (int val)
{
  gl_antialias=val;

  if (gl_antialias)
  {
    glShadeModel (GL_SMOOTH);
    glEnable (GL_POLYGON_SMOOTH);
    glEnable (GL_LINE_SMOOTH);
    glEnable (GL_POINT_SMOOTH);
    printf("[gl2] antialiasing on\n");
  }
  else
  {
    glShadeModel (GL_FLAT);
    glDisable (GL_POLYGON_SMOOTH);
    glDisable (GL_LINE_SMOOTH);
    glDisable (GL_POINT_SMOOTH);
    printf("[gl2] antialiasing off\n");
  }
  fflush(0);
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
	  fprintf (stderr, "[gl2] ain't a texture(update): texnum x=%d, y=%d, texture=%d\n",
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

      if(square->isDirty)
      {
	glTexSubImage2D (GL_TEXTURE_2D, 0, 
		 square->dirtyXoff, square->dirtyYoff,
		 square->dirtyWidth, square->dirtyHeight,
		 gl_bitmap_format, gl_bitmap_type, square->texture);

        square->isDirty=GL_FALSE;
        square->dirtyXoff=0; square->dirtyYoff=0; square->dirtyWidth=-1; square->dirtyHeight=-1;
      }

#ifndef NDEBUG
        fprintf (stdout, "[gl2] glTexSubImage2D texnum x=%d, y=%d, %d/%d - %d/%d\n", 
		x, y, square->dirtyXoff, square->dirtyYoff, square->dirtyWidth, square->dirtyHeight);
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

	glEnd();
/*
#ifndef NDEBUG
        fprintf (stdout, "[gl2] GL_QUADS texnum x=%d, y=%d, %f/%f %f/%f %f/%f %f/%f\n\n", x, y, square->fx1, square->fy1, square->fx4, square->fy4,
	square->fx3, square->fy3, square->fx2, square->fy2);
#endif
*/
    } /* for all texnumx */
  } /* for all texnumy */

  /* YES - lets catch this error ... 
   */
  (void) glGetError ();
}


static void resize(int x,int y){
  printf("[gl2] Resize: %dx%d\n",x,y);
  if( isFullscreen )
	  glViewport( (vo_screenwidth-x)/2, (vo_screenheight-y)/2, x, y);
  else 
	  glViewport( 0, 0, x, y );

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho (0, 1, 1, 0, -1.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void draw_alpha_32(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   vo_draw_alpha_rgb32(w,h,src,srca,stride,ImageData+4*(y0*image_width+x0),4*image_width);
}

static void draw_alpha_24(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   vo_draw_alpha_rgb24(w,h,src,srca,stride,ImageData+3*(y0*image_width+x0),3*image_width);
}

static void draw_alpha_16(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   vo_draw_alpha_rgb16(w,h,src,srca,stride,ImageData+2*(y0*image_width+x0),2*image_width);
}

static void draw_alpha_15(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   vo_draw_alpha_rgb15(w,h,src,srca,stride,ImageData+2*(y0*image_width+x0),2*image_width);
}

static void draw_alpha_null(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t 
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format,const vo_tune_info_t *info)
{
//	int screen;
	unsigned int fg, bg;
	char *hello = (title == NULL) ? "OpenGL rulez" : title;
//	char *name = ":0.0";
	XSizeHints hint;
	XVisualInfo *vinfo;
	XEvent xev;

//	XGCValues xgcv;
	XSetWindowAttributes xswa;
	unsigned long xswamask;

        const unsigned char * glVersion;

	image_height = height;
	image_width = width;
	image_format = format;
  
	if (X_already_started) return -1;
	if(!vo_init()) return -1;

	aspect_save_orig(width,height);
	aspect_save_prescale(d_width,d_height);
	aspect_save_screenres(vo_screenwidth,vo_screenheight);

	aspect(&d_width,&d_height,A_NOZOOM);

	X_already_started++;

        if( flags&0x01 )
        {
	        isFullscreen = GL_TRUE;
                aspect(&d_width,&d_height,A_ZOOM);
		hint.x = 0;
		hint.y = 0;
		hint.width = vo_screenwidth;
		hint.height = vo_screenheight;
		hint.flags = PPosition | PSize;
        } else {
		hint.x = 0;
		hint.y = 0;
		hint.width = d_width;
		hint.height = d_height;
		hint.flags = PPosition | PSize;
        }

	/* Get some colors */

	bg = WhitePixel(mDisplay, mScreen);
	fg = BlackPixel(mDisplay, mScreen);

	/* Make the window */

//	XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);

//	XMatchVisualInfo(mDisplay, screen, depth, TrueColor, &vinfo);
  vinfo=glXChooseVisual( mDisplay,mScreen,wsGLXAttrib );
  if (vinfo == NULL)
  {
    printf("[gl2] no GLX support present\n");
    return -1;
  }

	xswa.background_pixel = 0;
	xswa.border_pixel     = 1;
	xswa.colormap         = XCreateColormap(mDisplay, mRootWin, vinfo->visual, AllocNone);
	xswamask = CWBackPixel | CWBorderPixel | CWColormap;

  mywindow = XCreateWindow(mDisplay, RootWindow(mDisplay,mScreen),
    hint.x, hint.y, hint.width, hint.height, 4, vinfo->depth,CopyFromParent,vinfo->visual,xswamask,&xswa);

  vo_x11_classhint( mDisplay,mywindow,"gl2" );
  vo_hidecursor(mDisplay,mywindow);

  wsGLXContext=glXCreateContext( mDisplay,vinfo,NULL,True );

  if ( flags&0x01 ) vo_x11_decoration( mDisplay,mywindow,0 );

	XSelectInput(mDisplay, mywindow, StructureNotifyMask);

	/* Tell other applications about this window */

	XSetStandardProperties(mDisplay, mywindow, hello, hello, None, NULL, 0, &hint);

	/* Map window. */

	XMapWindow(mDisplay, mywindow);
#ifdef HAVE_XINERAMA
	vo_x11_xinerama_move(mDisplay,mywindow);
#endif
        XClearWindow(mDisplay,mywindow);

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

	//XSelectInput(mDisplay, mywindow, StructureNotifyMask); // !!!!
        XSelectInput(mDisplay, mywindow, StructureNotifyMask | KeyPressMask | PointerMotionMask
#ifdef HAVE_NEW_INPUT
		 | ButtonPressMask | ButtonReleaseMask
#endif
        );

  glVersion = glGetString(GL_VERSION);

  printf("[gl2] OpenGL Driver Information:\n");
  printf("\tvendor: %s,\n\trenderer %s,\n\tversion %s\n", 
  	glGetString(GL_VENDOR), 
	glGetString(GL_RENDERER),
	glVersion);

  if(glVersion[0]>'1' ||
     (glVersion[0]=='1' && glVersion[2]>='2') )
	  isGL12 = GL_TRUE;
  else
	  isGL12 = GL_FALSE;

  if(isGL12)
  {
	printf("[gl2] You have an OpenGL >= 1.2 capable drivers, GOOD (16bpp and BGR is ok !)\n");
  } else {
	printf("[gl2] You have an OpenGL < 1.2 drivers, BAD (16bpp and BGR may be damaged  !)\n");
  }

  if(glXGetConfig(mDisplay,vinfo,GLX_RED_SIZE, &r_sz)!=0) 
	  r_sz=0;
  if(glXGetConfig(mDisplay,vinfo,GLX_RED_SIZE, &g_sz)!=0) 
	  g_sz=0;
  if(glXGetConfig(mDisplay,vinfo,GLX_RED_SIZE, &b_sz)!=0) 
	  b_sz=0;
  if(glXGetConfig(mDisplay,vinfo,GLX_ALPHA_SIZE, &a_sz)!=0) 
	  b_sz=0;

  rgb_sz=r_sz+g_sz+b_sz;
  if(rgb_sz<=0) rgb_sz=24;

  if(r_sz==3 && g_sz==3 && b_sz==2 && a_sz==0) {
	  gl_internal_format=GL_R3_G3_B2;
	  gl_internal_format_s="GL_R3_G3_B2";
	  image_bpp = 8;
  } else if(r_sz==4 && g_sz==4 && b_sz==4 && a_sz==0) {
	  gl_internal_format=GL_RGB4;
	  gl_internal_format_s="GL_RGB4";
	  image_bpp = 16;
  } else if(r_sz==5 && g_sz==5 && b_sz==5 && a_sz==0) {
	  gl_internal_format=GL_RGB5;
	  gl_internal_format_s="GL_RGB5";
	  image_bpp = 16;
  } else if(r_sz==8 && g_sz==8 && b_sz==8 && a_sz==0) {
	  gl_internal_format=GL_RGB8;
	  gl_internal_format_s="GL_RGB8";
#ifdef HAVE_MMX
	  image_bpp = 32;
#else
	  image_bpp = 24;
#endif
  } else if(r_sz==10 && g_sz==10 && b_sz==10 && a_sz==0) {
	  gl_internal_format=GL_RGB10;
	  gl_internal_format_s="GL_RGB10";
	  image_bpp = 32;
  } else if(r_sz==2 && g_sz==2 && b_sz==2 && a_sz==2) {
	  gl_internal_format=GL_RGBA2;
	  gl_internal_format_s="GL_RGBA2";
	  image_bpp = 8;
  } else if(r_sz==4 && g_sz==4 && b_sz==4 && a_sz==4) {
	  gl_internal_format=GL_RGBA4;
	  gl_internal_format_s="GL_RGBA4";
	  image_bpp = 16;
  } else if(r_sz==5 && g_sz==5 && b_sz==5 && a_sz==1) {
	  gl_internal_format=GL_RGB5_A1;
	  gl_internal_format_s="GL_RGB5_A1";
	  image_bpp = 16;
  } else if(r_sz==8 && g_sz==8 && b_sz==8 && a_sz==8) {
	  gl_internal_format=GL_RGBA8;
	  gl_internal_format_s="GL_RGBA8";
#ifdef HAVE_MMX
	  image_bpp = 32;
#else
	  image_bpp = 24;
#endif
  } else if(r_sz==10 && g_sz==10 && b_sz==10 && a_sz==2) {
	  gl_internal_format=GL_RGB10_A2;
	  gl_internal_format_s="GL_RGB10_A2";
	  image_bpp = 32;
  } else {
	  gl_internal_format=GL_RGB;
	  gl_internal_format_s="GL_RGB";
#ifdef HAVE_MMX
	  image_bpp = 16;
#else
	  image_bpp = 24;
#endif
  }

  if(image_format==IMGFMT_YV12) 
  {
    image_mode= MODE_RGB;
    yuv2rgb_init(image_bpp, image_mode);
    printf("[gl2] YUV init OK!\n");
  } else {
    image_bpp=format&0xFF;

    if((format & IMGFMT_BGR_MASK) == IMGFMT_BGR)
        image_mode= MODE_BGR;
    else
        image_mode= MODE_RGB;
  }

  image_bytes=(image_bpp+7)/8;

  draw_alpha_fnc=draw_alpha_null;

  switch(image_bpp)
  {
  	case 15:
  	case 16:	
                        if(image_mode!=MODE_BGR)
			{
			        gl_bitmap_format   = GL_RGB;
			        gl_bitmap_format_s ="GL_RGB";
				gl_bitmap_type     = GL_UNSIGNED_SHORT_5_6_5;
				gl_bitmap_type_s   ="GL_UNSIGNED_SHORT_5_6_5";
			} else {
			        gl_bitmap_format   = GL_BGR;
			        gl_bitmap_format_s ="GL_BGR";
				gl_bitmap_type     = GL_UNSIGNED_SHORT_5_6_5;
				gl_bitmap_type_s   ="GL_UNSIGNED_SHORT_5_6_5";
			}

			if (image_bpp==15)
			     draw_alpha_fnc=draw_alpha_15;
			else
			     draw_alpha_fnc=draw_alpha_16;

			break;
  	case 24:	
                        if(image_mode!=MODE_BGR)
			{
				/* RGB888 */
				gl_bitmap_format   = GL_RGB;
				gl_bitmap_format_s ="GL_RGB";
			} else {
				/* BGR888 */
				gl_bitmap_format   = GL_BGR;
				gl_bitmap_format_s ="GL_BGR";
			}
			gl_bitmap_type   = GL_UNSIGNED_BYTE;
			gl_bitmap_type_s ="GL_UNSIGNED_BYTE";

			draw_alpha_fnc=draw_alpha_24; break;
			break;
  	case 32:	
			/* RGBA8888 */
			gl_bitmap_format   = GL_BGRA;
			gl_bitmap_format_s ="GL_BGRA";

                        if(image_mode!=MODE_BGR)
			{
				gl_bitmap_type   = GL_UNSIGNED_INT_8_8_8_8_REV;
				gl_bitmap_type_s ="GL_UNSIGNED_INT_8_8_8_8_REV";
			} else {
				gl_bitmap_type   = GL_UNSIGNED_INT_8_8_8_8;
				gl_bitmap_type_s ="GL_UNSIGNED_INT_8_8_8_8";
			}

			draw_alpha_fnc=draw_alpha_32; break;
			break;
  }

  r_sz=0; g_sz=0; b_sz=0;
  rgb_sz=0;
  a_sz=0;

  ImageDataLocal=malloc(image_width*image_height*image_bytes);
  memset(ImageDataLocal,128,image_width*image_height*image_bytes);

  ImageData=ImageDataLocal;

  texture_width=image_width;
  texture_height=image_height;
  initTextures();

  glDisable(GL_BLEND); 
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);

  glPixelStorei (GL_UNPACK_ROW_LENGTH, memory_x_len);

  /**
   * may give a little speed up for a kinda burst read ..
   */
  if( (image_width*image_bpp)%8 == 0 )
  	gl_alignment=8;
  else if( (image_width*image_bpp)%4 == 0 )
  	gl_alignment=4;
  else if( (image_width*image_bpp)%2 == 0 )
  	gl_alignment=2;
  else
  	gl_alignment=1;

  glPixelStorei (GL_UNPACK_ALIGNMENT, gl_alignment); 

  glEnable (GL_TEXTURE_2D);

  gl_set_antialias(0);
  gl_set_bilinear(1);
  
  drawTextureDisplay ();

  printf("[gl2] Using image_bpp=%d, image_bytes=%d, isBGR=%d, \n\tgl_bitmap_format=%s, gl_bitmap_type=%s, \n\tgl_alignment=%d, rgb_size=%d (%d,%d,%d), a_sz=%d, \n\tgl_internal_format=%s, tweaks=%s\n",
  	image_bpp, image_bytes, image_mode==MODE_BGR, 
        gl_bitmap_format_s, gl_bitmap_type_s, gl_alignment,
	rgb_sz, r_sz, g_sz, b_sz, a_sz, gl_internal_format_s, tweaks_used);

  resize(d_width,d_height);

  glClearColor( 0.0f,0.0f,0.0f,0.0f );
  glClear( GL_COLOR_BUFFER_BIT );

  used_s=0;
  used_r=0;
  used_b=0;
  used_info_done=0;

//  printf("OpenGL setup OK!\n");

      saver_off(mDisplay);  // turning off screen saver

	return 0;
}

static const vo_info_t*
get_info(void)
{
	return &vo_info;
}

static int gl_handlekey(int key)
{
	if(key=='a'||key=='A')
	{
		gl_set_antialias(!gl_antialias);
		return 0;
	}
	else if(key=='b'||key=='B')
	{
		gl_set_bilinear(-1);
		return 0;
	}
	return 1;
}

static void check_events(void)
{
	 XEvent         Event;
	 char           buf[100];
	 KeySym         keySym;
	 int            key;
	 static XComposeStatus stat;
	 int e;

	 while ( XPending( mDisplay ) )
	 {
	      XNextEvent( mDisplay,&Event );
	      if( Event.type == KeyPress )
	      {

		       XLookupString( &Event.xkey,buf,sizeof(buf),&keySym,&stat );
		       key = (keySym&0xff00) != 0? ( (keySym&0x00ff) + 256 ) 
		                                 : ( keySym ) ;
		       if(gl_handlekey(key))
			       XPutBackEvent(mDisplay, &Event);
		       break;
	      } else {
	      	       XPutBackEvent(mDisplay, &Event);
	               break;
	      }
         }
         e=vo_x11_check_events(mDisplay);
         if(e&VO_EVENT_RESIZE) resize(vo_dwidth,vo_dheight);
}

static void draw_osd(void)
{ vo_draw_text(image_width,image_height,draw_alpha_fnc); }

static void
flip_page(void)
{

  drawTextureDisplay();

//  glFlush();
  glFinish();
  glXSwapBuffers( mDisplay,mywindow );
  
  if(!used_info_done)
  {
	  if(used_s) printf("[gl2] using slice method yuv\n");
	  if(used_r) printf("[gl2] using frame method rgb\n");
	  if(used_b) printf("[gl2] using frame method bgr\n");
	  used_info_done=1;
          fflush(0);
  }
}

//static inline uint32_t draw_slice_x11(uint8_t *src[], uint32_t slice_num)
static uint32_t draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
    yuv2rgb(ImageData+y*raw_line_len, src[0], src[1], src[2], 
			w,h, image_width*image_bytes, stride[0],stride[1]);

#ifndef NDEBUG
     printf("slice: %d/%d -> %d/%d (%dx%d)\n", 
	x, y, x+w-1, y+h-1, w, h);
#endif

     used_s=1;

    setupTextureDirtyArea(x, y, w, h);

    return 0;
}

static inline uint32_t 
draw_frame_x11_bgr(uint8_t *src[])
{
      resetTexturePointers((unsigned char *)src[0]);
      ImageData=(unsigned char *)src[0];

      // for(i=0;i<image_height;i++) ImageData[image_width*image_bytes*i+20]=128;

     used_b=1;

     setupTextureDirtyArea(0, 0, image_width, image_height);
	return 0; 
}

static inline uint32_t 
draw_frame_x11_rgb(uint8_t *src[])
{
      resetTexturePointers((unsigned char *)src[0]);
      ImageData=(unsigned char *)src[0];

     used_r=1;

     setupTextureDirtyArea(0, 0, image_width, image_height);
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

  vo_x11_uninit(mDisplay, mywindow);
}

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("[gl2] Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
