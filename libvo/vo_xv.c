
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
static Display *mydisplay;
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
static XvImage *xvimage[1];

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

static int Shmem_Flag;
static int Quiet_Flag;
static XShmSegmentInfo Shminfo[1];
static int gXErrorFlag;
static int CompletionType = -1;

static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;

static Window                 mRoot;
static uint32_t               drwX,drwY,drwWidth,drwHeight,drwBorderWidth,drwDepth;
static uint32_t               drwcX,drwcY,dwidth,dheight,mFullscreen;

/*
 * connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
 int screen;
 char *hello = (title == NULL) ? "Xv render" : title;
 char *name = ":0.0";
 XSizeHints hint;
 XVisualInfo vinfo;
 XEvent xev;

 XGCValues xgcv;
 XSetWindowAttributes xswa;
 unsigned long xswamask;

 image_height = height;
 image_width = width;
 image_format=format;

 mFullscreen=fullscreen;
 dwidth=d_width; dheight=d_height;

 if(getenv("DISPLAY")) name = getenv("DISPLAY");

 mydisplay = XOpenDisplay(name);

 if (mydisplay == NULL)
  {
   fprintf(stderr,"Can't open display\n");
   return -1;
  }

 screen = DefaultScreen(mydisplay);

 hint.x = 0;
 hint.y = 0;
 hint.width = image_width;
 hint.height = image_height;
 if ( fullscreen )
  {
   hint.width=vo_screenwidth;
   hint.height=vo_screenheight;
  }
 hint.flags = PPosition | PSize;
 XGetWindowAttributes(mydisplay, DefaultRootWindow(mydisplay), &attribs);
 depth=attribs.depth;
 if (depth != 15 && depth != 16 && depth != 24 && depth != 32) depth = 24;
 XMatchVisualInfo(mydisplay, screen, depth, TrueColor, &vinfo);

 xswa.background_pixel = 0;
 xswa.border_pixel     = 0;
 xswamask = CWBackPixel | CWBorderPixel;

 mywindow = XCreateWindow(mydisplay, RootWindow(mydisplay,screen),
 hint.x, hint.y, hint.width, hint.height,
 0, depth,CopyFromParent,vinfo.visual,xswamask,&xswa);

 XSelectInput(mydisplay, mywindow, StructureNotifyMask | KeyPressMask );
 XSetStandardProperties(mydisplay, mywindow, hello, hello, None, NULL, 0, &hint);
 if ( fullscreen ) vo_x11_decoration( mydisplay,mywindow,0 );
 XMapWindow(mydisplay, mywindow);
 XFlush(mydisplay);
 XSync(mydisplay, False);

 mygc = XCreateGC(mydisplay, mywindow, 0L, &xgcv);

 xv_port = 0;
 if (Success == XvQueryExtension(mydisplay,&ver,&rel,&req,&ev,&err))
  {
   /* check for Xvideo support */
   if (Success != XvQueryAdaptors(mydisplay,DefaultRootWindow(mydisplay), &adaptors,&ai))
    {
     fprintf(stderr,"Xv: XvQueryAdaptors failed");
     return -1;
    }
   /* check adaptors */
   for (i = 0; i < adaptors; i++)
    {
     if ((ai[i].type & XvInputMask) && (ai[i].type & XvImageMask) && (xv_port == 0)) xv_port = ai[i].base_id;
    }
   /* check image formats */
   if (xv_port != 0)
    {
     fo = XvListImageFormats(mydisplay, xv_port, (int*)&formats);
     xv_format=0;
     for(i = 0; i < formats; i++)
      {
       fprintf(stderr, "Xvideo image format: 0x%x (%4.4s) %s\n", fo[i].id,(char*)&fo[i].id, (fo[i].format == XvPacked) ? "packed" : "planar");

       if (fo[i].id == format)
        {
         xv_format = fo[i].id;
        }
      }
     if (!xv_format) xv_port = 0;
    }

   if (xv_port != 0)
    {
     fprintf( stderr,"using Xvideo port %d for hw scaling\n",xv_port );

     allocate_xvimage(0);

     XGetGeometry( mydisplay,mywindow,&mRoot,&drwX,&drwY,&drwWidth,&drwHeight,&drwBorderWidth,&drwDepth );
     drwX=0; drwY=0;
     XTranslateCoordinates( mydisplay,mywindow,mRoot,0,0,&drwcX,&drwcY,&mRoot );
     fprintf( stderr,"[xv] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );

     if ( mFullscreen )
      {
       drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
       drwcX+=drwX;
       drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
       drwcY+=drwY;
       drwWidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
       drwHeight=(dheight > vo_screenheight?vo_screenheight:dheight);
       fprintf( stderr,"[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );
      }
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
 xvimage[foo] = XvShmCreateImage(mydisplay, xv_port, xv_format, 0, image_width, image_height, &Shminfo[foo]);

 Shminfo[foo].shmid    = shmget(IPC_PRIVATE, xvimage[foo]->data_size, IPC_CREAT | 0777);
 Shminfo[foo].shmaddr  = (char *) shmat(Shminfo[foo].shmid, 0, 0);
 Shminfo[foo].readOnly = False;

 xvimage[foo]->data = Shminfo[foo].shmaddr;
 XShmAttach(mydisplay, &Shminfo[foo]);
 XSync(mydisplay, False);
 shmctl(Shminfo[foo].shmid, IPC_RMID, 0);
 memset(xvimage[foo]->data,128,xvimage[foo]->data_size);
 return;
}

static void check_events(void)
{
 int e=vo_x11_check_events(mydisplay);
 if(e&VO_EVENT_RESIZE)
  {
   XGetGeometry( mydisplay,mywindow,&mRoot,&drwX,&drwY,&drwWidth,&drwHeight,&drwBorderWidth,&drwDepth );
   drwX=0; drwY=0;
   XTranslateCoordinates( mydisplay,mywindow,mRoot,0,0,&drwcX,&drwcY,&mRoot );
   fprintf( stderr,"[xv] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );

   if ( mFullscreen )
    {
     drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
     drwcX+=drwX;
     drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
     drwcY+=drwY;
     drwWidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
     drwHeight=(dheight > vo_screenheight?vo_screenheight:dheight);
     fprintf( stderr,"[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );
    }
  }
}

static void flip_page(void)
{
 check_events();
 XvShmPutImage(mydisplay, xv_port, mywindow, mygc, xvimage[0],
         0, 0,  image_width, image_height,
         drwX,drwY,drwWidth,(mFullscreen?drwHeight - 1:drwHeight),
         False);
 XFlush(mydisplay);
 return;
}

static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
 uint8_t *src;
 uint8_t *dst;
 int i;

 dst = xvimage[0]->data + image_width * y + x;
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

 dst = xvimage[0]->data + image_width * image_height + image_width/2 * y + x;
 src = image[2];
 if(w==stride[2] && w==image_width/2) memcpy(dst,src,w*h);
  else
   for(i=0;i<h;i++)
    {
     memcpy(dst,src,w);
     src+=stride[2];
     dst+=image_width/2;
   }
 dst = xvimage[0]->data + image_width * image_height * 5 / 4 + image_width/2 * y + x;
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

 if(xv_format==IMGFMT_YUY2)
  {
   // YUY2 packed, flipped
#if 0
   int i;
   unsigned short *s=(unsigned short *)src[0];
   unsigned short *d=(unsigned short *)xvimage[0]->data;
   s+=image_width*image_height;
   for(i=0;i<image_height;i++)
    {
     s-=image_width;
     memcpy(d,s,image_width*2);
     d+=image_width;
    }
#else
   memcpy(xvimage[0]->data,src[0],image_width*image_height*2);
#endif
  }
   else
    {
     // YV12 planar
     memcpy(xvimage[0]->data,src[0],image_width*image_height);
     memcpy(xvimage[0]->data+image_width*image_height,src[2],image_width*image_height/4);
     memcpy(xvimage[0]->data+image_width*image_height*5/4,src[1],image_width*image_height/4);
    }

  return 0;
}

static uint32_t query_format(uint32_t format)
{
 switch(format)
  {
   case IMGFMT_YV12:
   case IMGFMT_YUY2: return 1;
  }
 return 0;
}

static void uninit(void) { }



