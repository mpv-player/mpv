#define DISP

/*
 * video_out_x11.c,X11 interface
 *
 *
 * Copyright ( C ) 1996,MPEG Software Simulation Group. All Rights Reserved.
 *
 * Hacked into mpeg2dec by
 *
 * Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * 15 & 16 bpp support added by Franck Sicard <Franck.Sicard@solsoft.fr>
 * use swScaler instead of lots of tricky converters by Michael Niedermayer <michaelni@gmx.at>
 * runtime fullscreen switching by alex
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN( x11 )

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif
#include <errno.h>

#include "x11_common.h"

#include "fastmemcpy.h"
#include "sub.h"

#include "../postproc/swscale.h"
#include "../postproc/rgb2rgb.h"

#include "../mp_msg.h"

#include "../mp_image.h"

static vo_info_t vo_info =
{
        "X11 ( XImage/Shm )",
        "x11",
        "Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
        ""
};

/* private prototypes */
static void Display_Image ( XImage * myximage,unsigned char *ImageData );
static void (*draw_alpha_fnc)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

/* local data */
static unsigned char *ImageData;

/* X11 related variables */
static XImage *myximage;
static int depth,bpp,mode;
static XWindowAttributes attribs;

static int Flip_Flag;
static int zoomFlag;

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

static int Shmem_Flag;
static int Quiet_Flag;
static XShmSegmentInfo Shminfo[1];
static int gXErrorFlag;
static int CompletionType=-1;

/* since it doesn't seem to be defined on some platforms */
int XShmGetEventBase( Display* );
#endif

static uint32_t image_width;
static uint32_t image_height;
static uint32_t in_format;
static uint32_t out_format=0;
static int srcW=-1;
static int srcH=-1;
static int aspect; // 1<<16 based fixed point aspect, so that the aspect stays correct during resizing

static void check_events(){
  int ret = vo_x11_check_events(mDisplay);
  
   /* clear the old window */
  if (ret & VO_EVENT_RESIZE)
  {
    XSetBackground(mDisplay, vo_gc, 0);
    XClearWindow(mDisplay, vo_window);
  }
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

static SwsContext *swsContext=NULL;
extern int sws_flags;

static XVisualInfo vinfo;

static void getMyXImage()
{
#ifdef HAVE_SHM
 if ( mLocalDisplay && XShmQueryExtension( mDisplay ) ) Shmem_Flag=1;
  else
   {
    Shmem_Flag=0;
    if ( !Quiet_Flag ) printf( "Shared memory not supported\nReverting to normal Xlib\n" );
   }
 if ( Shmem_Flag ) CompletionType=XShmGetEventBase( mDisplay ) + ShmCompletion;

 if ( Shmem_Flag )
  {
   myximage=XShmCreateImage( mDisplay,vinfo.visual,depth,ZPixmap,NULL,&Shminfo[0],image_width,image_height );
   if ( myximage == NULL )
    {
     if ( myximage != NULL ) XDestroyImage( myximage );
     if ( !Quiet_Flag ) printf( "Shared memory error,disabling ( Ximage error )\n" );
     goto shmemerror;
    }
   Shminfo[0].shmid=shmget( IPC_PRIVATE,
   myximage->bytes_per_line * myximage->height ,
   IPC_CREAT | 0777 );
   if ( Shminfo[0].shmid < 0 )
   {
    XDestroyImage( myximage );
    if ( !Quiet_Flag )
     {
      printf( "%s\n",strerror( errno ) );
      perror( strerror( errno ) );
      printf( "Shared memory error,disabling ( seg id error )\n" );
     }
    goto shmemerror;
   }
   Shminfo[0].shmaddr=( char * ) shmat( Shminfo[0].shmid,0,0 );

   if ( Shminfo[0].shmaddr == ( ( char * ) -1 ) )
   {
    XDestroyImage( myximage );
    if ( Shminfo[0].shmaddr != ( ( char * ) -1 ) ) shmdt( Shminfo[0].shmaddr );
    if ( !Quiet_Flag ) printf( "Shared memory error,disabling ( address error )\n" );
    goto shmemerror;
   }
   myximage->data=Shminfo[0].shmaddr;
   ImageData=( unsigned char * ) myximage->data;
   Shminfo[0].readOnly=False;
   XShmAttach( mDisplay,&Shminfo[0] );

   XSync( mDisplay,False );

   if ( gXErrorFlag )
   {
    XDestroyImage( myximage );
    shmdt( Shminfo[0].shmaddr );
    if ( !Quiet_Flag ) printf( "Shared memory error,disabling.\n" );
    gXErrorFlag=0;
    goto shmemerror;
   }
   else
    shmctl( Shminfo[0].shmid,IPC_RMID,0 );

   {
     static int firstTime=1;
     if ( !Quiet_Flag && firstTime){
       printf( "Sharing memory.\n" );
       firstTime=0;
     } 
   }
 }
 else
  {
   shmemerror:
   Shmem_Flag=0;
#endif
   myximage=XGetImage( mDisplay,vo_window,0,0,
   image_width,image_height,AllPlanes,ZPixmap );
   ImageData=myximage->data;
#ifdef HAVE_SHM
  }
#endif
}

static void freeMyXImage()
{
#ifdef HAVE_SHM
 if ( Shmem_Flag )
  {
   XShmDetach( mDisplay,&Shminfo[0] );
   XDestroyImage( myximage );
   shmdt( Shminfo[0].shmaddr );
  }
  else
#endif
  {
   XDestroyImage( myximage );
  }
  myximage=NULL;
}

static uint32_t config( uint32_t width,uint32_t height,uint32_t d_width,uint32_t d_height,uint32_t flags,char *title,uint32_t format,const vo_tune_info_t *info)
{
// int screen;
 int fullscreen=0;
 int vm=0;
// int interval, prefer_blank, allow_exp, nothing;
 unsigned int fg,bg;
 XSizeHints hint;
 XEvent xev;
 XGCValues xgcv;
 Colormap theCmap;
 XSetWindowAttributes xswa;
 unsigned long xswamask;
#ifdef HAVE_XF86VM
 unsigned int modeline_width, modeline_height;
 static uint32_t vm_width;
 static uint32_t vm_height;
#endif



 if (!title)
    title = strdup("MPlayer X11 (XImage/Shm) render");

 in_format=format;
 srcW= width;
 srcH= height;
 
 if( flags&0x03 ) fullscreen = 1;
 if( flags&0x02 ) vm = 1;
 if( flags&0x08 ) Flip_Flag = 1;
 zoomFlag = flags&0x04;
// if(!fullscreen) zoomFlag=1; //it makes no sense to avoid zooming on windowd mode
 
//printf( "w: %d h: %d\n\n",vo_dwidth,vo_dheight );

 XGetWindowAttributes( mDisplay,DefaultRootWindow( mDisplay ),&attribs );
 depth=attribs.depth;

 if ( depth != 15 && depth != 16 && depth != 24 && depth != 32 ) depth=24;
 XMatchVisualInfo( mDisplay,mScreen,depth,TrueColor,&vinfo );

 /* set image size (which is indeed neither the input nor output size), 
    if zoom is on it will be changed during draw_slice anyway so we dont dupplicate the aspect code here 
 */
 image_width=(width + 7) & (~7);
 image_height=height;

 aspect= ((1<<16)*d_width + d_height/2)/d_height;

#ifdef HAVE_NEW_GUI
 if ( vo_window == None )
#endif   
   {
    hint.x=0;
    hint.y=0;
    if(zoomFlag){
	hint.width=d_width;
	hint.height=d_height;
    }else{
	hint.width=width;
	hint.height=height;
    }
 
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
   }
    else
#endif
    if ( fullscreen )
     {
      hint.width=vo_screenwidth;
      hint.height=vo_screenheight;
     }
    hint.flags=PPosition | PSize;

    bg=WhitePixel( mDisplay,mScreen );
    fg=BlackPixel( mDisplay,mScreen );
    vo_dwidth=hint.width;
    vo_dheight=hint.height;

    theCmap  =XCreateColormap( mDisplay,RootWindow( mDisplay,mScreen ),
    vinfo.visual,AllocNone );

    xswa.background_pixel=0;
    xswa.border_pixel=0;
    xswa.colormap=theCmap;
    xswamask=CWBackPixel | CWBorderPixel | CWColormap;

#ifdef HAVE_XF86VM
    if ( vm )
     {
      xswa.override_redirect=True;
      xswamask|=CWOverrideRedirect;
     }
#endif
 
    if ( WinID>=0 ){
      vo_window = WinID ? ((Window)WinID) : RootWindow( mDisplay,mScreen );
      XUnmapWindow( mDisplay,vo_window );
      XChangeWindowAttributes( mDisplay,vo_window,xswamask,&xswa );
    }
    else
      vo_window=XCreateWindow( mDisplay,RootWindow( mDisplay,mScreen ),
                         hint.x,hint.y,
                         hint.width,hint.height,
                         xswa.border_pixel,depth,CopyFromParent,vinfo.visual,xswamask,&xswa );

    vo_x11_classhint( mDisplay,vo_window,"x11" );
    vo_hidecursor(mDisplay,vo_window);
    if ( fullscreen ) vo_x11_decoration( mDisplay,vo_window,0 );
    XSelectInput( mDisplay,vo_window,StructureNotifyMask );
    XSetStandardProperties( mDisplay,vo_window,title,title,None,NULL,0,&hint );
    XMapWindow( mDisplay,vo_window );
#ifdef HAVE_XINERAMA
   vo_x11_xinerama_move(mDisplay,vo_window);
#endif
    do { XNextEvent( mDisplay,&xev ); } while ( xev.type != MapNotify || xev.xmap.event != vo_window );
    XSelectInput( mDisplay,vo_window,NoEventMask );

    XFlush( mDisplay );
    XSync( mDisplay,False );
    vo_gc=XCreateGC( mDisplay,vo_window,0L,&xgcv );

    XSelectInput( mDisplay,vo_window,StructureNotifyMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask);

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

   getMyXImage();

  switch ((bpp=myximage->bits_per_pixel)){
         case 24: draw_alpha_fnc=draw_alpha_24; 
	 	  out_format= IMGFMT_BGR24; break;
         case 32: draw_alpha_fnc=draw_alpha_32;
	 	  out_format= IMGFMT_BGR32; break;
         case 15:
         case 16: if (depth==15){
	 	     draw_alpha_fnc=draw_alpha_15;
	 	     out_format= IMGFMT_BGR15;
       		  }else{
	 	     draw_alpha_fnc=draw_alpha_16;
	 	     out_format= IMGFMT_BGR16;
          	  }break;
   	default:  draw_alpha_fnc=draw_alpha_null;
  }

  /* always allocate swsContext as size could change between frames */
  swsContext= getSwsContextFromCmdLine(width, height, in_format, width, height, out_format );

//  printf( "X11 color mask:  R:%lX  G:%lX  B:%lX\n",myximage->red_mask,myximage->green_mask,myximage->blue_mask );

  // If we have blue in the lowest bit then obviously RGB
  mode=( ( myximage->blue_mask & 0x01 ) != 0 ) ? MODE_RGB : MODE_BGR;
#ifdef WORDS_BIGENDIAN
  if ( myximage->byte_order != MSBFirst )
#else
  if ( myximage->byte_order != LSBFirst )
#endif
  {
    mode=( ( myximage->blue_mask & 0x01 ) != 0 ) ? MODE_BGR : MODE_RGB;
//   printf( "No support for non-native XImage byte order!\n" );
//   return -1;
  }

#ifdef WORDS_BIGENDIAN
  if(mode==MODE_BGR && bpp!=32){
    printf("BGR%d not supported, please contact the developers\n", bpp);
    return -1;
  }
  if(mode==MODE_RGB && bpp==32){
    printf("RGB32 not supported on big-endian systems, please contact the developers\n");
    return -1;
  }
#else
  if(mode==MODE_BGR){
    printf("BGR not supported, please contact the developers\n");
    return -1;
  }
#endif  

 saver_off(mDisplay);
 return 0;
}

static const vo_info_t* get_info( void )
{ return &vo_info; }

static void Display_Image( XImage *myximage,uint8_t *ImageData )
{
#ifdef DISP
#ifdef HAVE_SHM
 if ( Shmem_Flag )
  {
   XShmPutImage( mDisplay,vo_window,vo_gc,myximage,
                 0,0,
                 ( vo_dwidth - swsContext->dstW ) / 2,( vo_dheight - myximage->height ) / 2,
                 swsContext->dstW,myximage->height,True );
  }
  else
#endif
   {
    XPutImage( mDisplay,vo_window,vo_gc,myximage,
               0,0,
               ( vo_dwidth - swsContext->dstW ) / 2,( vo_dheight - myximage->height ) / 2,
               swsContext->dstW,myximage->height);
  }
#endif
}

static void draw_osd(void)
{ vo_draw_text(image_width,image_height,draw_alpha_fnc); }

static void flip_page( void ){
    Display_Image( myximage,ImageData );
    XSync(mDisplay, False);
}

static uint32_t draw_slice( uint8_t *src[],int stride[],int w,int h,int x,int y )
{
  uint8_t *dst[3];
  int dstStride[3];
  static int old_vo_dwidth=-1;
  static int old_vo_dheight=-1;
  
  if((old_vo_dwidth != vo_dwidth || old_vo_dheight != vo_dheight) /*&& y==0*/ && zoomFlag)
  {
    int newW= vo_dwidth;
    int newH= vo_dheight;
    int newAspect= (newW*(1<<16) + (newH>>1))/newH;
    SwsContext *oldContext= swsContext;
    
    if(newAspect>aspect) newW= (newH*aspect + (1<<15))>>16;
    else                 newH= ((newW<<16) + (aspect>>1)) /aspect;

    old_vo_dwidth=  vo_dwidth;
    old_vo_dheight= vo_dheight;

    if(sws_flags==0) newW&= (~31); // not needed but, if the user wants the FAST_BILINEAR SCALER, then its needed

    swsContext= getSwsContextFromCmdLine(srcW, srcH, in_format, 
    					 newW, newH, out_format);
    if(swsContext)
    {
	image_width= (newW+7)&(~7);
	image_height= newH;

	freeMyXImage();
	getMyXImage();
	freeSwsContext(oldContext);
    }    
    else
    {
	swsContext= oldContext;
    }
  }
  dstStride[1]=
  dstStride[2]=0;
  dst[1]=
  dst[2]=NULL;

  if(Flip_Flag)
  {
	dstStride[0]= -image_width*((bpp+7)/8);
	dst[0]=ImageData - dstStride[0]*(image_height-1);
	swsContext->swScale(swsContext,src,stride,y,h,dst, dstStride);
  }
  else
  {
	dstStride[0]=image_width*((bpp+7)/8);
	dst[0]=ImageData;
	swsContext->swScale(swsContext,src,stride,y,h,dst, dstStride);
  }
  return 0;
}

static uint32_t draw_frame( uint8_t *src[] ){
      int stride[3]= {0,0,0};
      
      if     (in_format==IMGFMT_YUY2)  stride[0]=srcW*2;
      else if(in_format==IMGFMT_BGR15) stride[0]=srcW*2;
      else if(in_format==IMGFMT_BGR16) stride[0]=srcW*2;
      else if(in_format==IMGFMT_BGR24) stride[0]=srcW*3;
      else if(in_format==IMGFMT_BGR32) stride[0]=srcW*4;
      
      return draw_slice(src, stride, srcW, srcH, 0, 0);
}

static uint32_t get_image(mp_image_t *mpi)
{
    if (zoomFlag ||
	!IMGFMT_IS_BGR(mpi->imgfmt) ||
	(IMGFMT_BGR_DEPTH(mpi->imgfmt) != vo_depthonscreen) ||
	((mpi->type != MP_IMGTYPE_STATIC) && (mpi->type != MP_IMGTYPE_TEMP)) ||
	(mpi->flags & MP_IMGFLAG_PLANAR) ||
	(mpi->flags & MP_IMGFLAG_YUV) ||
	(mpi->width != image_width) ||
	(mpi->height != image_height)
    )
	return(VO_FALSE);

    if (Flip_Flag)
    {
	mpi->stride[0] = -image_width*((bpp+7)/8);
	mpi->planes[0] = ImageData - mpi->stride[0]*(image_height-1);
    }
    else
    {
	mpi->stride[0] = image_width*((bpp+7)/8);
	mpi->planes[0] = ImageData;
    }
    mpi->flags |= MP_IMGFLAG_DIRECT;
    
    return(VO_TRUE);
}

static uint32_t query_format( uint32_t format )
{
    if(verbose > 2)
        printf("vo_x11: query_format was called: %x (%s)\n",format,vo_format_name(format));
    if (IMGFMT_IS_BGR(format))
    {
	if (IMGFMT_BGR_DEPTH(format) == vo_depthonscreen)
	    return 3|VFCAP_OSD|VFCAP_SWSCALE|VFCAP_FLIP;
	else
	    return 1|VFCAP_OSD|VFCAP_SWSCALE|VFCAP_FLIP;
    }

 switch( format )
  {
//   case IMGFMT_BGR15:
//   case IMGFMT_BGR16:
//   case IMGFMT_BGR24:
//   case IMGFMT_BGR32:
//    return 0x2;
//   case IMGFMT_YUY2: 
   case IMGFMT_I420:
   case IMGFMT_IYUV:
   case IMGFMT_YV12:
    return 1|VFCAP_OSD|VFCAP_SWSCALE;
  }
 return 0;
}


static void
uninit(void)
{
 if(!myximage) return;
 
 freeMyXImage();
 saver_on(mDisplay); // screen saver back on

#ifdef HAVE_XF86VM
 vo_vm_close(mDisplay);
#endif

 vo_x11_uninit(mDisplay, vo_window);

 freeSwsContext(swsContext);
}

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("vo_x11: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }

#ifdef HAVE_NEW_GUI
    if ( vo_window == None )
#endif   
	if( !vo_init() ) return -1; // Can't open X11

    return 0;
}

#if 0
/* for runtime fullscreen switching */
static int vo_fs_oldx = -1;
static int vo_fs_oldy = -1;
static int vo_fs_oldwidth = -1;
static int vo_fs_oldheight = -1;
#endif

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_GUISUPPORT:
    return VO_TRUE;
  case VOCTRL_GET_IMAGE:
    return get_image(data);
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen();
/*
    if ((vo_fs_oldwidth == -1) && (vo_fs_oldheight == -1))
    {
	int foo;
	Window root;

#ifdef LOCAL_LOOKUP
	XGetGeometry(mDisplay, vo_window, &root, &foo, &foo,
		     &vo_fs_oldwidth, &vo_fs_oldheight, &foo, &foo);
	    
        XTranslateCoordinates(mDisplay, vo_window, root, 0, 0,
				  &vo_fs_oldx, &vo_fs_oldy,(Window *) &foo);
#else
	vo_fs_oldwidth = vo_dwidth;
	vo_fs_oldheight = vo_dheight;
	vo_fs_oldx = vo_dx;
	vo_fs_oldy = vo_dy;
#endif

	mp_msg(MSGT_VO,MSGL_V,"X11 Fullscreen: saved old place: %dx%d-%dx%d\n",
	    vo_fs_oldx, vo_fs_oldy, vo_fs_oldwidth, vo_fs_oldheight);
	
	// resize 
	vo_dwidth = vo_screenwidth;
	vo_dheight = vo_screenheight;
	vo_x11_decoration( mDisplay,vo_window,0 );
	XMoveResizeWindow(mDisplay, vo_window, 0, 0,
	    vo_screenwidth, vo_screenheight);
	XSync(mDisplay, False);
    }
    else
    {
	vo_x11_decoration( mDisplay,vo_window,1 );
	XMoveResizeWindow(mDisplay, vo_window, vo_fs_oldx, vo_fs_oldy, 
	    vo_fs_oldwidth, vo_fs_oldheight);
	

#ifdef LOCAL_LOOKUP
	// restore 
	vo_dwidth = vo_fs_oldwidth;
	vo_dheight = vo_fs_oldheight;
#endif
	
	// clean
	vo_fs_oldwidth = -1;
	vo_fs_oldheight = -1;
	XSync(mDisplay, False);
    }
*/
    return VO_TRUE;
  }
  return VO_NOTIMPL;
}
