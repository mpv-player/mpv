
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
#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN(xv)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>

#include "x11_common.h"

#include "fastmemcpy.h"
#include "sub.h"
#include "aspect.h"

#include "../postproc/rgb2rgb.h"

#ifdef HAVE_NEW_GUI
#include "../Gui/interface.h"
#endif

static vo_info_t vo_info =
{
        "X11/Xv",
        "xv",
        "Gerd Knorr <kraxel@goldbach.in-berlin.de>",
        ""
};

extern int verbose;

/* local data */
static unsigned char *ImageData;

/* X11 related variables */
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

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

/* since it doesn't seem to be defined on some platforms */
int XShmGetEventBase(Display*);

static XShmSegmentInfo Shminfo[NUM_BUFFERS];
static int Shmem_Flag;
#endif

//static int Quiet_Flag; <-- What is that for ? Albeu.
static int gXErrorFlag;
static int CompletionType = -1;

static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;
static int flip_flag;

static Window                 mRoot;
static uint32_t               drwX,drwY,drwBorderWidth,drwDepth;
static uint32_t               dwidth,dheight;

static void (*draw_alpha_fnc)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

static void draw_alpha_yv12(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   x0+=(vo_panscan_x>>2);
   vo_draw_alpha_yv12(w,h,src,srca,stride,
       xvimage[current_buf]->data+xvimage[current_buf]->offsets[0]+
       xvimage[current_buf]->pitches[0]*y0+x0,xvimage[current_buf]->pitches[0]);
}

static void draw_alpha_yuy2(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   x0+=(vo_panscan_x>>2);
   vo_draw_alpha_yuy2(w,h,src,srca,stride,
       xvimage[current_buf]->data+xvimage[current_buf]->offsets[0]+
       xvimage[current_buf]->pitches[0]*y0+2*x0,xvimage[current_buf]->pitches[0]);
}

static void draw_alpha_uyvy(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
   x0+=(vo_panscan_x>>2);
   vo_draw_alpha_yuy2(w,h,src,srca,stride,
       xvimage[current_buf]->data+xvimage[current_buf]->offsets[0]+
       xvimage[current_buf]->pitches[0]*y0+2*x0+1,xvimage[current_buf]->pitches[0]);
}

static void draw_alpha_null(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
}

static int __xv_set_video_eq( const vidix_video_eq_t *info,int use_reset)
{
 XvAttribute *attributes;
 int howmany, xv_min,xv_max,xv_atomka;
 static int was_reset = 0;
/* get available attributes */
     attributes = XvQueryPortAttributes(mDisplay, xv_port, &howmany);
     /* first pass try reset */
     if(use_reset)
     {
	for (i = 0; i < howmany && attributes; i++)
        {
            if (attributes[i].flags & XvSettable && !strcmp(attributes[i].name,"XV_SET_DEFAULTS"
))
            {
		was_reset = 1;
		mp_msg(MSGT_VO,MSGL_V,"vo_xv: reset gamma correction\n");
                xv_atomka = XInternAtom(mDisplay, attributes[i].name, True);
                XvSetPortAttribute(mDisplay, xv_port, xv_atomka, attributes[i].max_value);
	    }
        }
	/* for safety purposes */
	if(!was_reset) return ENOSYS;
     }
     for (i = 0; i < howmany && attributes; i++)
     {
            if (attributes[i].flags & XvSettable)
            {
                xv_min = attributes[i].min_value;
                xv_max = attributes[i].max_value;
                xv_atomka = XInternAtom(mDisplay, attributes[i].name, True);
/* since we have SET_DEFAULTS first in our list, we can check if it's available
   then trigger it if it's ok so that the other values are at default upon query */
                if (xv_atomka != None)
                {
		    int port_value,port_min,port_max,port_mid;
		    if(strcmp(attributes[i].name,"XV_BRIGHTNESS") == 0
			      && (info->cap & VEQ_CAP_BRIGHTNESS))
				port_value = info->brightness;
		    else
		    if(strcmp(attributes[i].name,"XV_SATURATION") == 0
			      && (info->cap & VEQ_CAP_SATURATION))
				port_value = info->saturation;
		    else
		    if(strcmp(attributes[i].name,"XV_CONTRAST") == 0
			      && (info->cap & VEQ_CAP_CONTRAST))
				port_value = info->contrast;
		    else
		    if(strcmp(attributes[i].name,"XV_HUE") == 0
			      && (info->cap & VEQ_CAP_HUE))
				port_value = info->hue;
		    else
                    /* Note: since 22.01.2002 GATOS supports these attrs for radeons (NK) */
		    if(strcmp(attributes[i].name,"XV_RED_INTENSITY") == 0
			      && (info->cap & VEQ_CAP_RGB_INTENSITY))
				port_value = info->red_intensity;
		    else
		    if(strcmp(attributes[i].name,"XV_GREEN_INTENSITY") == 0
			      && (info->cap & VEQ_CAP_RGB_INTENSITY))
				port_value = info->green_intensity;
		    else
		    if(strcmp(attributes[i].name,"XV_BLUE_INTENSITY") == 0
			      && (info->cap & VEQ_CAP_RGB_INTENSITY))
				port_value = info->blue_intensity;
		    else continue;
		    /* means that user has untouched this parameter since
		       NVidia driver has default == min for XV_HUE but not mid */
		    if(!port_value && use_reset) continue;
		    port_min = xv_min;
		    port_max = xv_max;
		    port_mid = (port_min + port_max) / 2;
		    port_value = port_mid + (port_value * (port_max - port_min)) / 2000;
		    mp_msg(MSGT_VO,MSGL_V,"vo_xv: set gamma %s to %i (min %i max %i mid %i)\n",attributes[i].name,port_value,port_min,port_max,port_mid);
                    XvSetPortAttribute(mDisplay, xv_port, xv_atomka, port_value);
                }
        }
    }
    return 0;
}


static int xv_set_video_eq( const vidix_video_eq_t *info)
{
    return __xv_set_video_eq(info,0);
}

static int xv_get_video_eq( vidix_video_eq_t *info)
{
 XvAttribute *attributes;
 int howmany, xv_min,xv_max,xv_atomka;
/* get available attributes */
     memset(info,0,sizeof(vidix_video_eq_t));
     attributes = XvQueryPortAttributes(mDisplay, xv_port, &howmany);
     for (i = 0; i < howmany && attributes; i++)
     {
            if (attributes[i].flags & XvGettable)
            {
                xv_min = attributes[i].min_value;
                xv_max = attributes[i].max_value;
                xv_atomka = XInternAtom(mDisplay, attributes[i].name, True);
/* since we have SET_DEFAULTS first in our list, we can check if it's available
   then trigger it if it's ok so that the other values are at default upon query */
                if (xv_atomka != None)
                {
		    int port_value,port_min,port_max,port_mid;
                    XvGetPortAttribute(mDisplay, xv_port, xv_atomka, &port_value);
		    mp_msg(MSGT_VO,MSGL_V,"vo_xv: get: %s = %i\n",attributes[i].name,port_value);

		    port_min = xv_min;
		    port_max = xv_max;
		    port_mid = (port_min + port_max) / 2;		    
		    port_value = ((port_value - port_mid)*2000)/(port_max-port_min);
		    
		    mp_msg(MSGT_VO,MSGL_V,"vo_xv: assume: %s = %i\n",attributes[i].name,port_value);
		    
		    if(strcmp(attributes[i].name,"XV_BRIGHTNESS") == 0)
		    {
			info->cap |= VEQ_CAP_BRIGHTNESS;
			info->brightness = port_value;
		    }
		    else
		    if(strcmp(attributes[i].name,"XV_SATURATION") == 0)
		    {
			info->cap |= VEQ_CAP_SATURATION;
			info->saturation = port_value;
		    }
		    else
		    if(strcmp(attributes[i].name,"XV_CONTRAST") == 0)
		    {
			info->cap |= VEQ_CAP_CONTRAST;
			info->contrast = port_value;
		    }
		    else
		    if(strcmp(attributes[i].name,"XV_HUE") == 0)
		    {
			info->cap |= VEQ_CAP_HUE;
			info->hue = port_value;
		    }
		    else
                    /* Note: since 22.01.2002 GATOS supports these attrs for radeons (NK) */
		    if(strcmp(attributes[i].name,"XV_RED_INTENSITY") == 0)
		    {
			info->cap |= VEQ_CAP_RGB_INTENSITY;
			info->red_intensity = port_value;
		    }
		    else
		    if(strcmp(attributes[i].name,"XV_GREEN_INTENSITY") == 0)
		    {
			info->cap |= VEQ_CAP_RGB_INTENSITY;
			info->green_intensity = port_value;
		    }
		    else
		    if(strcmp(attributes[i].name,"XV_BLUE_INTENSITY") == 0)
		    {
			info->cap |= VEQ_CAP_RGB_INTENSITY;
			info->blue_intensity = port_value;
		    }
		    else continue;
                }
        }
    }
    return 0;
}

extern int vo_gamma_brightness;
extern int vo_gamma_saturation;
extern int vo_gamma_contrast;
extern int vo_gamma_hue;
extern int vo_gamma_red_intensity;
extern int vo_gamma_green_intensity;
extern int vo_gamma_blue_intensity;

static void set_gamma_correction( void )
{
  vidix_video_eq_t info;
  /* try all */
  info.cap = VEQ_CAP_BRIGHTNESS | VEQ_CAP_CONTRAST | VEQ_CAP_SATURATION |
	     VEQ_CAP_HUE | VEQ_CAP_RGB_INTENSITY;
  info.flags = 0; /* doesn't matter for xv */
  info.brightness = vo_gamma_brightness;
  info.contrast = vo_gamma_contrast;
  info.saturation = vo_gamma_saturation;
  info.hue = vo_gamma_hue;
  info.red_intensity = vo_gamma_red_intensity;
  info.green_intensity = vo_gamma_green_intensity;
  info.blue_intensity = vo_gamma_blue_intensity;
  /* reset with XV_SET_DEFAULTS only once */
  __xv_set_video_eq(&info,1);
}

/*
 * connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format,const vo_tune_info_t *info)
{
// int screen;
 char *hello = (title == NULL) ? "Xv render" : title;
// char *name = ":0.0";
 XSizeHints hint;
 XVisualInfo vinfo;
 XEvent xev;

 XGCValues xgcv;
 XSetWindowAttributes xswa;
 unsigned long xswamask;
#ifdef HAVE_XF86VM
 int vm=0;
 unsigned int modeline_width, modeline_height;
 static uint32_t vm_width;
 static uint32_t vm_height;
#endif

 panscan_init();

 aspect_save_orig(width,height);
 aspect_save_prescale(d_width,d_height);

 image_height = height;
 image_width = width;
 image_format=format;
 
 vo_mouse_autohide=1;

 vo_dx=( vo_screenwidth - d_width ) / 2; vo_dy=( vo_screenheight - d_height ) / 2;
 vo_dwidth=d_width; vo_dheight=d_height;
     
#ifdef HAVE_XF86VM
 if( flags&0x02 ) vm = 1;
#endif
 flip_flag=flags&8;
 num_buffers=vo_doublebuffering?NUM_BUFFERS:1;

   /* check image formats */
     fo = XvListImageFormats(mDisplay, xv_port, (int*)&formats);
     xv_format=0;
     if(format==IMGFMT_BGR24) format=IMGFMT_YV12;
     for(i = 0; i < formats; i++){
       mp_msg(MSGT_VO,MSGL_V,"Xvideo image format: 0x%x (%4.4s) %s\n", fo[i].id,(char*)&fo[i].id, (fo[i].format == XvPacked) ? "packed" : "planar");
       if (fo[i].id == format) xv_format = fo[i].id;
     }
     if (!xv_format) return -1;
 
 aspect_save_screenres(vo_screenwidth,vo_screenheight);

#ifdef HAVE_NEW_GUI
  if(use_gui)
    guiGetEvent( guiSetShVideo,0 ); // let the GUI to setup/resize our window
  else
#endif
  {
   hint.x = vo_dx;
   hint.y = vo_dy;
   hint.width = d_width;
   hint.height = d_height;
   aspect(&d_width,&d_height,A_NOZOOM);
#ifdef HAVE_XF86VM
    if ( vm )
      {
	if ((d_width==0) && (d_height==0))
	  { vm_width=image_width; vm_height=image_height; }
	else
	  { vm_width=d_width; vm_height=d_height; }
	vo_vm_switch(vm_width, vm_height,&modeline_width, &modeline_height);
	hint.x=(vo_screenwidth-modeline_width)/2;
	hint.y=(vo_screenheight-modeline_height)/2;
	hint.width=modeline_width;
	hint.height=modeline_height;
	aspect_save_screenres(modeline_width,modeline_height);
      }
    else
#endif
   if ( vo_fs )
    {
#ifdef X11_FULLSCREEN
     /* this code replaces X11_FULLSCREEN hack in mplayer.c
      * aspect() is available through aspect.h for all vos.
      * besides zooming should only be done with -zoom,
      * but I leave the old -fs behaviour so users don't get
      * irritated for now (and send lots o' mails ;) ::atmos
      */

     aspect(&d_width,&d_height,A_ZOOM);
#endif

    }
//   dwidth=d_width; dheight=d_height; //XXX: what are the copy vars used for?
   vo_dwidth=d_width; vo_dheight=d_height;
   hint.flags = PPosition | PSize /* | PBaseSize */;
   hint.base_width = hint.width; hint.base_height = hint.height;
   XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);
   depth=attribs.depth;
   if (depth != 15 && depth != 16 && depth != 24 && depth != 32) depth = 24;
   XMatchVisualInfo(mDisplay, mScreen, depth, TrueColor, &vinfo);

   xswa.background_pixel = 0;
   xswa.border_pixel     = 0;
   xswamask = CWBackPixel | CWBorderPixel;

    if ( WinID>=0 ){
      vo_window = WinID ? ((Window)WinID) : mRootWin;
      if ( WinID ) 
       {
        XUnmapWindow( mDisplay,vo_window );
        XChangeWindowAttributes( mDisplay,vo_window,xswamask,&xswa );
	XSelectInput( mDisplay,vo_window,StructureNotifyMask | KeyPressMask | PropertyChangeMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | ExposureMask );
        XMapWindow( mDisplay,vo_window );
       } else { drwX=vo_dx; drwY=vo_dy; }
    } else 
       {
        vo_window = XCreateWindow(mDisplay, mRootWin,
          hint.x, hint.y, hint.width, hint.height,
          0, depth,CopyFromParent,vinfo.visual,xswamask,&xswa);

        vo_x11_classhint( mDisplay,vo_window,"xv" );
        vo_hidecursor(mDisplay,vo_window);

        XSelectInput(mDisplay, vo_window, StructureNotifyMask | KeyPressMask | PropertyChangeMask |
	((WinID==0) ? 0 : (PointerMotionMask
#ifdef HAVE_NEW_INPUT
		| ButtonPressMask | ButtonReleaseMask
#endif
	  )));
        XSetStandardProperties(mDisplay, vo_window, hello, hello, None, NULL, 0, &hint);
        XSetWMNormalHints( mDisplay,vo_window,&hint );
	XMapWindow(mDisplay, vo_window);
	if ( flags&1 ) vo_x11_fullscreen();
#ifdef HAVE_XINERAMA
	vo_x11_xinerama_move(mDisplay,vo_window);
#endif
       }
    vo_gc = XCreateGC(mDisplay, vo_window, 0L, &xgcv);
    XFlush(mDisplay);
    XSync(mDisplay, False);
#ifdef HAVE_XF86VM
    if ( vm )
     {
      /* Grab the mouse pointer in our window */
      XGrabPointer(mDisplay, vo_window, True, 0,
                   GrabModeAsync, GrabModeAsync,
                   vo_window, None, CurrentTime);
      XSetInputFocus(mDisplay, vo_window, RevertToNone, CurrentTime);
     }
#endif
  }

     mp_msg(MSGT_VO,MSGL_V, "using Xvideo port %d for hw scaling\n",xv_port );
       
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

     set_gamma_correction();

     aspect(&vo_dwidth,&vo_dheight,A_NOZOOM);
     if ( ( flags&1 )&&( !WinID ) )
      {
       aspect(&vo_dwidth,&vo_dheight,A_ZOOM);
       drwX=( vo_screenwidth - (vo_dwidth > vo_screenwidth?vo_screenwidth:vo_dwidth) ) / 2;
       drwY=( vo_screenheight - (vo_dheight > vo_screenheight?vo_screenheight:vo_dheight) ) / 2;
       vo_dwidth=(vo_dwidth > vo_screenwidth?vo_screenwidth:vo_dwidth);
       vo_dheight=(vo_dheight > vo_screenheight?vo_screenheight:vo_dheight);
       mp_msg(MSGT_VO,MSGL_V, "[xv-fs] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );
      }

     panscan_calc();
     
     mp_msg(MSGT_VO,MSGL_V, "[xv] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );

     saver_off(mDisplay);  // turning off screen saver
     return 0;
}

static const vo_info_t * get_info(void)
{ return &vo_info; }

static void allocate_xvimage(int foo)
{
 /*
  * allocate XvImages.  FIXME: no error checking, without
  * mit-shm this will bomb... trzing to fix ::atmos
  */
#ifdef HAVE_SHM
 if ( mLocalDisplay && XShmQueryExtension( mDisplay ) ) Shmem_Flag = 1;
 else
  {
   Shmem_Flag = 0;
   mp_msg(MSGT_VO,MSGL_INFO, "Shared memory not supported\nReverting to normal Xv\n" );
  }
 if ( Shmem_Flag ) 
  {
   xvimage[foo] = XvShmCreateImage(mDisplay, xv_port, xv_format, 0, image_width, image_height, &Shminfo[foo]);

   Shminfo[foo].shmid    = shmget(IPC_PRIVATE, xvimage[foo]->data_size, IPC_CREAT | 0777);
   Shminfo[foo].shmaddr  = (char *) shmat(Shminfo[foo].shmid, 0, 0);
   Shminfo[foo].readOnly = False;

   xvimage[foo]->data = Shminfo[foo].shmaddr;
   XShmAttach(mDisplay, &Shminfo[foo]);
   XSync(mDisplay, False);
   shmctl(Shminfo[foo].shmid, IPC_RMID, 0);
  }
 else
#endif
  {
    xvimage[foo] = XvCreateImage(mDisplay, xv_port, xv_format, 0, image_width, image_height);
    xvimage[foo]->data = malloc(xvimage[foo]->data_size);
    XSync(mDisplay,False);
  }
 memset(xvimage[foo]->data,128,xvimage[foo]->data_size);
 return;
}

static void deallocate_xvimage(int foo)
{
#ifdef HAVE_SHM
 if ( Shmem_Flag )
  {
   XShmDetach( mDisplay,&Shminfo[foo] );
   shmdt( Shminfo[foo].shmaddr );
  }
 else
#endif
  {
   free(xvimage[foo]->data);
  }
 XFree(xvimage[foo]);
 
 XFlush( mDisplay );
 XSync(mDisplay, False);
 return;
}

static void check_events(void)
{
 int e=vo_x11_check_events(mDisplay);
 if(e&VO_EVENT_RESIZE)
  {
   XGetGeometry( mDisplay,vo_window,&mRoot,&drwX,&drwY,&vo_dwidth,&vo_dheight,&drwBorderWidth,&drwDepth );
   drwX = drwY = 0;
   mp_msg(MSGT_VO,MSGL_V, "[xv] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );

   aspect(&dwidth,&dheight,A_NOZOOM);
   if ( vo_fs )
    {
     aspect(&dwidth,&dheight,A_ZOOM);
     drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
     drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
     vo_dwidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
     vo_dheight=(dheight > vo_screenheight?vo_screenheight:dheight);
     mp_msg(MSGT_VO,MSGL_V, "[xv-fs] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );
    }
  }
 if ( e & VO_EVENT_EXPOSE )
  {
#ifdef HAVE_SHM
   if ( Shmem_Flag )
    {
     XvShmPutImage(mDisplay, xv_port, vo_window, vo_gc, xvimage[current_buf], 0, 0,  image_width, image_height, drwX, drwY, 1, 1, False);
     XvShmPutImage(mDisplay, xv_port, vo_window, vo_gc, xvimage[current_buf], 0, 0,  image_width, image_height, drwX,drwY,vo_dwidth,(vo_fs?vo_dheight - 1:vo_dheight), False);
    }
   else
#endif
    {
     XvPutImage(mDisplay, xv_port, vo_window, vo_gc, xvimage[current_buf], 0, 0,  image_width, image_height, drwX, drwY, 1, 1);
     XvPutImage(mDisplay, xv_port, vo_window, vo_gc, xvimage[current_buf], 0, 0,  image_width, image_height, drwX,drwY,vo_dwidth,(vo_fs?vo_dheight - 1:vo_dheight));
    }
  }
}

static void draw_osd(void)
{ vo_draw_text(image_width-(vo_panscan_x>>1),image_height,draw_alpha_fnc);}

static void flip_page(void)
{

#ifdef HAVE_SHM
 if ( Shmem_Flag )
  {
   XvShmPutImage(mDisplay, xv_port, vo_window, vo_gc, xvimage[current_buf],
         0, 0,  image_width, image_height,
         drwX-(vo_panscan_x>>1),drwY-(vo_panscan_y>>1),vo_dwidth+vo_panscan_x,(vo_fs?vo_dheight - 1:vo_dheight)+vo_panscan_y,
         False);
  }
 else
#endif
  {
   XvPutImage(mDisplay, xv_port, vo_window, vo_gc, xvimage[current_buf],
         0, 0,  image_width, image_height,
         drwX-(vo_panscan_x>>1),drwY-(vo_panscan_y>>1),vo_dwidth+vo_panscan_x,(vo_fs?vo_dheight - 1:vo_dheight)+vo_panscan_y);
  }
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
 int srcstride;

 dst = xvimage[current_buf]->data + xvimage[current_buf]->offsets[0] + 
       xvimage[current_buf]->pitches[0]*y + x;
 src = image[0];
 if(w==stride[0] && w==xvimage[current_buf]->pitches[0]) memcpy(dst,src,w*h);
   else
    for(i=0;i<h;i++)
     {
      memcpy(dst,src,w);
      src+=stride[0];
      dst+=xvimage[current_buf]->pitches[0];
     }

 x/=2;y/=2;w/=2;h/=2;

 dst = xvimage[current_buf]->data + xvimage[current_buf]->offsets[1] + 
       xvimage[current_buf]->pitches[1]*y + x;
 src=image[2];srcstride=stride[2];
 if(image_format!=IMGFMT_YV12){ src=image[1];srcstride=stride[1]; }
 if(w==srcstride && w==xvimage[current_buf]->pitches[1]) memcpy(dst,src,w*h);
  else
   for(i=0;i<h;i++)
    {
     memcpy(dst,src,w);
     src+=srcstride;
     dst+=xvimage[current_buf]->pitches[1];
   }

 dst = xvimage[current_buf]->data + xvimage[current_buf]->offsets[2] + 
       xvimage[current_buf]->pitches[2]*y + x;
 src=image[2];srcstride=stride[2];
 if(image_format==IMGFMT_YV12){ src=image[1];srcstride=stride[1]; }
 if(w==srcstride && w==xvimage[current_buf]->pitches[2]) memcpy(dst,src,w*h);
  else
   for(i=0;i<h;i++)
    {
     memcpy(dst,src,w);
     src+=srcstride;
     dst+=xvimage[current_buf]->pitches[2];
   }

 return 0;
}

static uint32_t draw_frame(uint8_t *src[])
{

 switch (image_format) {
 case IMGFMT_YUY2:
 case IMGFMT_UYVY:
 case IMGFMT_YVYU:

//    printf("off=0x%X  pitch=%d  width=%d  \n",xvimage[current_buf]->offsets[0],xvimage[current_buf]->pitches[0],image_width);

    // YUY2 packed
    if(image_width*2==xvimage[current_buf]->pitches[0]){
	memcpy(xvimage[current_buf]->data+xvimage[current_buf]->offsets[0],
	    src[0], image_width*image_height*2);
    } else {
	unsigned char* s=src[0];
	unsigned char* d=xvimage[current_buf]->data+xvimage[current_buf]->offsets[0];
	int i;
	for(i=0;i<image_height;i++) {
	    memcpy(d,s,image_width*2);
	    s+=image_width*2;
	    d+=xvimage[current_buf]->pitches[0];
	}
    }
    break;

 case IMGFMT_BGR24:

    if(flip_flag)	// tricky, using negative src stride:
     rgb24toyv12(src[0]+3*image_width*(image_height-1),
                 xvimage[current_buf]->data+xvimage[current_buf]->offsets[0],
                 xvimage[current_buf]->data+xvimage[current_buf]->offsets[2],
                 xvimage[current_buf]->data+xvimage[current_buf]->offsets[1],
		 image_width,image_height,
		 xvimage[current_buf]->pitches[0],
		 xvimage[current_buf]->pitches[1],
		 -3*image_width);
    else
     rgb24toyv12(src[0],
                 xvimage[current_buf]->data+xvimage[current_buf]->offsets[0],
                 xvimage[current_buf]->data+xvimage[current_buf]->offsets[2],
                 xvimage[current_buf]->data+xvimage[current_buf]->offsets[1],
		 image_width,image_height,
		 xvimage[current_buf]->pitches[0],
		 xvimage[current_buf]->pitches[1],
		 3*image_width);
     break;

 }

  return 0;
}

static uint32_t get_image(mp_image_t *mpi){
    if(mpi->type==MP_IMGTYPE_STATIC && num_buffers>1) return VO_FALSE; // it is not static
    if(mpi->type==MP_IMGTYPE_IPB && num_buffers<3 && mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE; // not enough
    if(mpi->type==MP_IMGTYPE_IP  && num_buffers<2 && mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE; // not enough
    if(mpi->imgfmt!=image_format || mpi->imgfmt==IMGFMT_BGR24) return VO_FALSE; // needs conversion :(
    if(  xvimage[current_buf]->pitches[0]*mpi->height 
       > xvimage[current_buf]->offsets[1] - xvimage[current_buf]->offsets[0]) return VO_FALSE; //buffer to small
    if(  xvimage[current_buf]->pitches[0] < mpi->width /*FIXME non yv12*/) return VO_FALSE; //buffer to small
//    if(mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE; // slow video ram
    if( (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH)) ||
	(mpi->width*(mpi->bpp/8)==xvimage[current_buf]->pitches[0]) ){
	mpi->planes[0]=xvimage[current_buf]->data+xvimage[current_buf]->offsets[0];
	mpi->stride[0]=xvimage[current_buf]->pitches[0];
	mpi->width=mpi->stride[0]/(mpi->bpp/8);
	if(mpi->flags&MP_IMGFLAG_PLANAR){
	    if(mpi->flags&MP_IMGFLAG_SWAPPED){
		// I420
		mpi->planes[1]=xvimage[current_buf]->data+xvimage[current_buf]->offsets[1];
		mpi->planes[2]=xvimage[current_buf]->data+xvimage[current_buf]->offsets[2];
		mpi->stride[1]=xvimage[current_buf]->pitches[1];
		mpi->stride[2]=xvimage[current_buf]->pitches[2];
	    } else {
	       // YV12
		mpi->planes[1]=xvimage[current_buf]->data+xvimage[current_buf]->offsets[2];
		mpi->planes[2]=xvimage[current_buf]->data+xvimage[current_buf]->offsets[1];
		mpi->stride[1]=xvimage[current_buf]->pitches[2];
		mpi->stride[2]=xvimage[current_buf]->pitches[1];
	    }
	}
       mpi->flags|=MP_IMGFLAG_DIRECT;
//	printf("mga: get_image() SUCCESS -> Direct Rendering ENABLED\n");
       return VO_TRUE;
    }
    return VO_FALSE;
}

static uint32_t query_format(uint32_t format)
{
    int flag=3|VFCAP_HWSCALE_UP|VFCAP_HWSCALE_DOWN|VFCAP_OSD; // FIXME! check for DOWN
   /* check image formats */
     if(format==IMGFMT_BGR24){ format=IMGFMT_YV12;flag&=~2;flag|=VFCAP_FLIP;} // conversion!
     for(i = 0; i < formats; i++){
       if (fo[i].id == format) return flag; //xv_format = fo[i].id;
     }
     return 0;
}

static void uninit(void) 
{
 int i;
 if ( !vo_config_count ) return;
 saver_on(mDisplay); // screen saver back on
 for( i=0;i<num_buffers;i++ ) deallocate_xvimage( i );
#ifdef HAVE_XF86VM
 vo_vm_close(mDisplay);
#endif
 vo_x11_uninit();
}

static uint32_t preinit(const char *arg)
{
    XvPortID xv_p;
    int busy_ports=0;
    
    if(arg) 
    {
	mp_msg(MSGT_VO,MSGL_ERR,"vo_xv: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    if (!vo_init()) return -1;

    xv_port = 0;
   /* check for Xvideo extension */
    if (Success != XvQueryExtension(mDisplay,&ver,&rel,&req,&ev,&err)){
	mp_msg(MSGT_VO,MSGL_ERR,"Sorry, Xv not supported by this X11 version/driver\n");
	mp_msg(MSGT_VO,MSGL_ERR,"******** Try with  -vo x11  or  -vo sdl  *********\n");
	return -1;
    }
    
   /* check for Xvideo support */
    if (Success != XvQueryAdaptors(mDisplay,DefaultRootWindow(mDisplay), &adaptors,&ai)){
	mp_msg(MSGT_VO,MSGL_ERR,"Xv: XvQueryAdaptors failed");
	return -1;
    }

   /* check adaptors */
    for (i = 0; i < adaptors && xv_port == 0; i++){
      if ((ai[i].type & XvInputMask) && (ai[i].type & XvImageMask)) {
	 for (xv_p = ai[i].base_id; xv_p < ai[i].base_id+ai[i].num_ports; ++xv_p)
	     if (!XvGrabPort(mDisplay, xv_p, CurrentTime)) {
		 xv_port = xv_p;
		 break;
	     } else {
		 mp_msg(MSGT_VO,MSGL_WARN,"Xv: could not grab port %i\n", (int)xv_p);
		 ++busy_ports;
	     }
      }
    }
    if(!xv_port){
      if(busy_ports)
	mp_msg(MSGT_VO,MSGL_ERR,"Could not find free Xvideo port - maybe another process is already using it.\n"
	    "Close all video applications, and try again. If that does not help,\n"
	    "see 'mplayer -vo help' for other (non-xv) video out drivers.\n");
      else
	mp_msg(MSGT_VO,MSGL_ERR,"It seems there is no Xvideo support for your video card available.\n"
	    "Run 'xvinfo' to verify its Xv support, and read Xv section of DOCS/video.html !\n"
	    "See 'mplayer -vo help' for other (non-xv) video out drivers. Try -vo x11\n");
	return -1;
    }

    fo = XvListImageFormats(mDisplay, xv_port, (int*)&formats);

    return 0;
}

static void query_vaa(vo_vaa_t *vaa)
{
  memset(vaa,0,sizeof(vo_vaa_t));
  vaa->get_video_eq = xv_get_video_eq;
  vaa->set_video_eq = xv_set_video_eq;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_VAA:
    query_vaa((vo_vaa_t*)data);
    return VO_TRUE;
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_GET_IMAGE:
    return get_image(data);
  case VOCTRL_GUISUPPORT:
    return VO_TRUE;
  case VOCTRL_GET_PANSCAN:
    if ( !vo_config_count || !vo_fs ) return VO_FALSE;
    return VO_TRUE;
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen();
  /* indended, fallthrough to update panscan on fullscreen/windowed switch */
  case VOCTRL_SET_PANSCAN:
   if ( ( vo_fs && ( vo_panscan != vo_panscan_amount ) ) || ( !vo_fs && vo_panscan_amount ) )
     {
      int old_y = vo_panscan_y;
      panscan_calc();
      
      if(old_y != vo_panscan_y)
       {
        XClearWindow(mDisplay, vo_window);
        XFlush(mDisplay);
       }
     }
    return VO_TRUE;
  }
  return VO_NOTIMPL;
}
