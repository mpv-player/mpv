
// Number of buffers _FOR_DOUBLEBUFFERING_MODE_
// Use option -double to enable double buffering! (default: single buffer)
#define NUM_BUFFERS 2

/*
 * vo_xv.c, X11 Xv interface
 *
 * Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved.
 *
 * Hacked into mpeg2dec by
 *
 * Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * Xv image suuport by Gerd Knorr <kraxel@goldbach.in-berlin.de>
 * fullscreen support by Pontscho
 * double buffering support by A'rpi
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN(xv)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <errno.h>

#include "x11_common.h"

#include "fastmemcpy.h"
#include "sub.h"

static vo_info_t vo_info =
{
        "X11/Xv",
        "xv",
        "Gerd Knorr <kraxel@goldbach.in-berlin.de>",
        ""
};

/* since it doesn't seem to be defined on some platforms */
int XShmGetEventBase(Display*);

/* local data */
static unsigned char *ImageData;

/* X11 related variables */
//static Display *mydisplay;
static Window mywindow;
static GC mygc;
static XImage *myximage;
static int depth, bpp, mode;
static XWindowAttributes attribs;

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
// FIXME: dynamically allocate this stuff
static void allocate_xvimage(int);
static unsigned int ver,rel,req,ev,err;
static unsigned int formats, adaptors,i,xv_port,xv_format;
static XvAdaptorInfo        *ai;
static XvImageFormatValues  *fo;

static int current_buf=0;
static int num_buffers=1; // default
static XvImage* xvimage[NUM_BUFFERS];

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

static int Shmem_Flag;
static int Quiet_Flag;
static XShmSegmentInfo Shminfo[NUM_BUFFERS];
static int gXErrorFlag;
static int CompletionType = -1;

static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;

static Window                 mRoot;
static uint32_t               drwX,drwY,drwWidth,drwHeight,drwBorderWidth,drwDepth;
static uint32_t               drwcX,drwcY,dwidth,dheight,mFullscreen;

#ifdef HAVE_NEW_GUI
 static uint32_t               mdwidth,mdheight;
#endif


static void (*draw_alpha_fnc)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

static void draw_alpha_yv12(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   vo_draw_alpha_yv12(w,h,src,srca,stride,xvimage[current_buf]->data+image_width*y0+x0,image_width);
}

static void draw_alpha_yuy2(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   vo_draw_alpha_yuy2(w,h,src,srca,stride,xvimage[current_buf]->data+2*(image_width*y0+x0),2*image_width);
}

static void draw_alpha_uyvy(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   vo_draw_alpha_yuy2(w,h,src,srca,stride,xvimage[current_buf]->data+2*(image_width*y0+x0)+1,2*image_width);
}

static void draw_alpha_null(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
}


/*
 * connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
// int screen;
 char *hello = (title == NULL) ? "Xv render" : title;
// char *name = ":0.0";
 XSizeHints hint;
 XVisualInfo vinfo;
 XEvent xev;
 XvPortID xv_p;

 XGCValues xgcv;
 XSetWindowAttributes xswa;
 unsigned long xswamask;

 image_height = height;
 image_width = width;
 image_format=format;

#ifdef HAVE_NEW_GUI
 mdwidth=width;
 mdheight=height;
#endif

 mFullscreen=flags&1;
 dwidth=d_width; dheight=d_height;
 num_buffers=vo_doublebuffering?NUM_BUFFERS:1;
 
 if (!vo_init()) return -1;

#ifdef HAVE_NEW_GUI
 if ( vo_window == None )
  {
#endif
   hint.x = 0;
   hint.y = 0;
   hint.width = d_width;
   hint.height = d_height;
   if ( mFullscreen )
    {
     hint.width=vo_screenwidth;
     hint.height=vo_screenheight;
    }
   hint.flags = PPosition | PSize;
   XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);
   depth=attribs.depth;
   if (depth != 15 && depth != 16 && depth != 24 && depth != 32) depth = 24;
   XMatchVisualInfo(mDisplay, mScreen, depth, TrueColor, &vinfo);

   xswa.background_pixel = 0;
   xswa.border_pixel     = 0;
   xswamask = CWBackPixel | CWBorderPixel;

   mywindow = XCreateWindow(mDisplay, RootWindow(mDisplay,mScreen),
   hint.x, hint.y, hint.width, hint.height,
   0, depth,CopyFromParent,vinfo.visual,xswamask,&xswa);
   vo_x11_classhint( mDisplay,mywindow,"xv" );
   vo_hidecursor(mDisplay,mywindow);

   XSelectInput(mDisplay, mywindow, StructureNotifyMask | KeyPressMask );
   XSetStandardProperties(mDisplay, mywindow, hello, hello, None, NULL, 0, &hint);
   if ( mFullscreen ) vo_x11_decoration( mDisplay,mywindow,0 );
   XMapWindow(mDisplay, mywindow);
   mygc = XCreateGC(mDisplay, mywindow, 0L, &xgcv);
   XFlush(mDisplay);
   XSync(mDisplay, False);
#ifdef HAVE_NEW_GUI
  }
  else
    {
     mywindow=vo_window;
     mygc=vo_gc;
    }
#endif

 xv_port = 0;
 if (Success == XvQueryExtension(mDisplay,&ver,&rel,&req,&ev,&err))
  {
   /* check for Xvideo support */
   if (Success != XvQueryAdaptors(mDisplay,DefaultRootWindow(mDisplay), &adaptors,&ai))
    {
     printf("Xv: XvQueryAdaptors failed");
     return -1;
    }
   /* check adaptors */
   for (i = 0; i < adaptors && xv_port == 0; i++)
    {
     if ((ai[i].type & XvInputMask) && (ai[i].type & XvImageMask))
	 for (xv_p = ai[i].base_id; xv_p < ai[i].base_id+ai[i].num_ports; ++xv_p)
	     if (!XvGrabPort(mDisplay, xv_p, CurrentTime)) {
		 xv_port = xv_p;
		 break;
	     } else {
		 printf("Xv: could not grab port %i\n", (int)xv_p);
	     }
    }
   /* check image formats */
   if (xv_port != 0)
    {
     fo = XvListImageFormats(mDisplay, xv_port, (int*)&formats);
     xv_format=0;
     for(i = 0; i < formats; i++)
      {
       printf("Xvideo image format: 0x%x (%4.4s) %s\n", fo[i].id,(char*)&fo[i].id, (fo[i].format == XvPacked) ? "packed" : "planar");

       if (fo[i].id == format)
        {
         xv_format = fo[i].id;
        }
      }
     if (!xv_format) xv_port = 0;
    }

   if (xv_port != 0)
    {
     printf( "using Xvideo port %d for hw scaling\n",xv_port );
       
       switch (xv_format){
	case IMGFMT_YV12:  
	case IMGFMT_I420:
        case IMGFMT_IYUV: draw_alpha_fnc=draw_alpha_yv12; break;
	case IMGFMT_YUY2:
	case IMGFMT_YVYU: draw_alpha_fnc=draw_alpha_yuy2; break;	
	case IMGFMT_UYVY: draw_alpha_fnc=draw_alpha_uyvy; break;
	default:   	  draw_alpha_fnc=draw_alpha_null;
       }

      for(current_buf=0;current_buf<num_buffers;++current_buf)
       allocate_xvimage(current_buf);

     current_buf=0;

     #ifdef HAVE_NEW_GUI
      if ( vo_window != None )
       {
        mFullscreen=0;
        dwidth=mdwidth; dheight=mdheight;
        if ( ( vo_dwidth == vo_screenwidth )&&( vo_dheight == vo_screenheight ) )
         {
          mFullscreen=1;
          dwidth=vo_screenwidth;
          dheight=vo_screenwidth * mdheight / mdwidth;
         }
       }
     #endif

     XGetGeometry( mDisplay,mywindow,&mRoot,&drwX,&drwY,&drwWidth,&drwHeight,&drwBorderWidth,&drwDepth );
     drwX=0; drwY=0;
     XTranslateCoordinates( mDisplay,mywindow,mRoot,0,0,&drwcX,&drwcY,&mRoot );
     printf( "[xv] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );

     if ( mFullscreen )
      {
       drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
       drwcX+=drwX;
       drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
       drwcY+=drwY;
       drwWidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
       drwHeight=(dheight > vo_screenheight?vo_screenheight:dheight);
       printf( "[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );
      }
#ifdef HAVE_NEW_GUI
     if ( vo_window == None )
#endif
      saver_off(mDisplay);  // turning off screen saver
     return 0;
    }
  }

 printf("Sorry, Xv not supported by this X11 version/driver\n");
 printf("******** Try with  -vo x11  or  -vo sdl  *********\n");
 return 1;
}

static const vo_info_t * get_info(void)
{ return &vo_info; }

static void allocate_xvimage(int foo)
{
 /*
  * allocate XvImages.  FIXME: no error checking, without
  * mit-shm this will bomb...
  */
 xvimage[foo] = XvShmCreateImage(mDisplay, xv_port, xv_format, 0, image_width, image_height, &Shminfo[foo]);

 Shminfo[foo].shmid    = shmget(IPC_PRIVATE, xvimage[foo]->data_size, IPC_CREAT | 0777);
 Shminfo[foo].shmaddr  = (char *) shmat(Shminfo[foo].shmid, 0, 0);
 Shminfo[foo].readOnly = False;

 xvimage[foo]->data = Shminfo[foo].shmaddr;
 XShmAttach(mDisplay, &Shminfo[foo]);
 XSync(mDisplay, False);
 shmctl(Shminfo[foo].shmid, IPC_RMID, 0);
 memset(xvimage[foo]->data,128,xvimage[foo]->data_size);
 return;
}

static void deallocate_xvimage(int foo)
{
 XShmDetach( mDisplay,&Shminfo[foo] );
 shmdt( Shminfo[foo].shmaddr );
 XFlush( mDisplay );
 XSync(mDisplay, False);
 return;
}

static void check_events(void)
{
 int e=vo_x11_check_events(mDisplay);
 if(e&VO_EVENT_RESIZE)
  {
   XGetGeometry( mDisplay,mywindow,&mRoot,&drwX,&drwY,&drwWidth,&drwHeight,&drwBorderWidth,&drwDepth );
   drwX=0; drwY=0;
   XTranslateCoordinates( mDisplay,mywindow,mRoot,0,0,&drwcX,&drwcY,&mRoot );
   printf( "[xv] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );

   #ifdef HAVE_NEW_GUI
    if ( vo_window != None )
     {
      mFullscreen=0;
      dwidth=mdwidth; dheight=mdheight;
      if ( ( vo_dwidth == vo_screenwidth )&&( vo_dheight == vo_screenheight ) )
       {
        mFullscreen=1;
        dwidth=vo_screenwidth;
        dheight=vo_screenwidth * mdheight / mdwidth;
       }
     }
   #endif

   if ( mFullscreen )
    {
     drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
     drwcX+=drwX;
     drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
     drwcY+=drwY;
     drwWidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
     drwHeight=(dheight > vo_screenheight?vo_screenheight:dheight);
     printf( "[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );
    }
  }
 if ( e & VO_EVENT_EXPOSE )
  {
   XvShmPutImage(mDisplay, xv_port, mywindow, mygc, xvimage[current_buf], 0, 0,  image_width, image_height, drwX, drwY, 1, 1, False);
   XvShmPutImage(mDisplay, xv_port, mywindow, mygc, xvimage[current_buf], 0, 0,  image_width, image_height, drwX,drwY,drwWidth,(mFullscreen?drwHeight - 1:drwHeight), False);
  }
}

static void draw_osd(void)
{ vo_draw_text(image_width,image_height,draw_alpha_fnc);}

static void flip_page(void)
{
 XvShmPutImage(mDisplay, xv_port, mywindow, mygc, xvimage[current_buf],
         0, 0,  image_width, image_height,
         drwX,drwY,drwWidth,(mFullscreen?drwHeight - 1:drwHeight),
         False);
 if (num_buffers>1){
    current_buf=(current_buf+1)%num_buffers;
    XFlush(mDisplay);
 } else
    XSync(mDisplay, False);   
 return;
}



static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
 uint8_t *src;
 uint8_t *dst;
 int i;

 dst = xvimage[current_buf]->data + image_width * y + x;
 src = image[0];
 if(w==stride[0] && w==image_width) memcpy(dst,src,w*h);
   else
    for(i=0;i<h;i++)
     {
      memcpy(dst,src,w);
      src+=stride[0];
      dst+=image_width;
     }

 x/=2;y/=2;w/=2;h/=2;

 dst = xvimage[current_buf]->data + image_width * image_height + image_width/2 * y + x;
 src = image[2];
 if(w==stride[2] && w==image_width/2) memcpy(dst,src,w*h);
  else
   for(i=0;i<h;i++)
    {
     memcpy(dst,src,w);
     src+=stride[2];
     dst+=image_width/2;
   }
 dst = xvimage[current_buf]->data + image_width * image_height * 5 / 4 + image_width/2 * y + x;
 src = image[1];
 if(w==stride[1] && w==image_width/2) memcpy(dst,src,w*h);
  else
   for(i=0;i<h;i++)
    {
     memcpy(dst,src,w);
     src+=stride[1];
     dst+=image_width/2;
    }
 return 0;
}

static uint32_t draw_frame(uint8_t *src[])
{
 int foo;

 switch (xv_format) {
 case IMGFMT_YUY2:
 case IMGFMT_UYVY:
 case IMGFMT_YVYU:

     // YUY2 packed, flipped
#if 0
     int i;
     unsigned short *s=(unsigned short *)src[0];
     unsigned short *d=(unsigned short *)xvimage[current_buf]->data;
     s+=image_width*image_height;
     for(i=0;i<image_height;i++) {
	 s-=image_width;
	 memcpy(d,s,image_width*2);
	 d+=image_width;
     }
#else
     memcpy(xvimage[current_buf]->data,src[0],image_width*image_height*2);
#endif
     break;

 case IMGFMT_YV12:
 case IMGFMT_I420:
 case IMGFMT_IYUV:

     // YV12 planar
     memcpy(xvimage[current_buf]->data,src[0],image_width*image_height);
     memcpy(xvimage[current_buf]->data+image_width*image_height,src[2],image_width*image_height/4);
     memcpy(xvimage[current_buf]->data+image_width*image_height*5/4,src[1],image_width*image_height/4);
     break;
 }

  return 0;
}

static uint32_t query_format(uint32_t format)
{

// umm, this is a kludge, we need to ask the server.. (see init function above)
    return 1;
/*
 switch(format)
  {
   case IMGFMT_YV12:
   case IMGFMT_YUY2: 
       return 1;
  }
 return 0;
*/
}

static void uninit(void) 
{
 int i;
#ifdef HAVE_NEW_GUI
 if ( vo_window == None )
#endif
 {
  saver_on(mDisplay); // screen saver back on
  XDestroyWindow( mDisplay,mywindow );
 }
 for( i=0;i<num_buffers;i++ ) deallocate_xvimage( i );
}



