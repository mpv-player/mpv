#define DISP

/*
 * $Id$
 * 
 * video_out_dga.c, X11 interface
 *
 *
 * Copyright ( C ) 2001, Andreas Ackermann. All Rights Reserved.
 *
 * <acki@acki-netz.de>
 *
 * Sourceforge username: acki2
 * 
 * note well: 
 *   
 * o this is alpha
 * o covers only common video card formats
 * o works only on intel architectures
 *
 * $Log$
 * Revision 1.10  2001/04/01 22:01:28  acki2
 * - still more debug output to be able to fix 15/16 bpp problem
 *
 * Revision 1.9  2001/04/01 08:07:14  acki2
 * - added detection of memsize of graphics card to check if double buffering is possible
 * - fixed resolution switching a little and added more debug output
 * - resolution switching is still according to d_width and d_height which
 *   is not always a good idea ...
 *
 * 
 * 30/02/2001
 *
 * o query_format(): with DGA 2.0 it returns all depths it supports
 *   (even 16 when running 32 and vice versa)
 *   Checks for (hopefully!) compatible RGBmasks in 15/16 bit modes
 * o added some more criterions for resolution switching
 * o cleanup
 * o with DGA2.0 present, ONLY DGA2.0 functions are used
 * o for 15/16 modes ONLY RGB 555 is supported, since the divx-codec
 *   happens to map the data this way. If your graphics card supports
 *   this, you're well off and may use these modes; for mpeg 
 *   movies things could be different, but I was too lazy to implement 
 *   it ...
 * o you may define VO_DGA_FORCE_DEPTH to the depth you desire 
 *   if you don't like the choice the driver makes
 *   Beware: unless you can use DGA2.0 this has to be your X Servers
 *           depth!!!
 * o Added double buffering :-))
 * o included VidMode switching support for DGA1.0, written by  Michael Graffam
 *    mgraffam@idsi.net
 * 
 */

//#define VO_DGA_FORCE_DEPTH 32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "yuv2rgb.h"


//#undef HAVE_DGA2
//#undef HAVE_XF86VM


LIBVO_EXTERN( dga )

#include <X11/Xlib.h>
#include <X11/extensions/xf86dga.h>

#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif


#include "x11_common.h"

static vo_info_t vo_info =
{
        "DGA ( Direct Graphic Access )",
        "dga",
        "Andreas Ackermann <acki@acki-netz.de>",
        ""
};

#ifdef HAVE_XF86VM
static XF86VidModeModeInfo **vo_dga_vidmodes=NULL;
#endif


//extern int       verbose;           // shouldn't someone remove the static from 
                                      // its definition in mplayer.c ???

static int       vo_dga_width;           // bytes per line in framebuffer
static int       vo_dga_vp_width;        // visible pixels per line in 
                                         // framebuffer
static int       vo_dga_vp_height;       // visible lines in framebuffer
static int       vo_dga_is_running = 0; 
static int       vo_dga_src_width;       // width of video in pixels
static int       vo_dga_src_height;      // height of video in pixels
static int       vo_dga_bpp;             // bytes per pixel in framebuffer
static int       vo_dga_src_offset=0;    // offset in src
static int       vo_dga_vp_offset=0;     // offset in dest
static int       vo_dga_bytes_per_line;  // bytes per line to copy
static int       vo_dga_src_skip;        // bytes to skip after copying one line 
                                         // (not supported yet) in src
static int       vo_dga_vp_skip;         // dto. for dest 
static int       vo_dga_lines;           // num of lines to copy
static int       vo_dga_src_format;                                 
static int       vo_dga_planes;          // bits per pixel on screen

static int       vo_dga_dbf_mem_offset;  // offset in bytes for alternative 
                                         // framebuffer (0 if dbf is not 
					 // possible)
static int       vo_dga_dbf_y_offset;    // y offset (in scanlines)
static int       
                 vo_dga_dbf_current;     // current buffer (0 or 1)

static unsigned char     *vo_dga_base;
static Display  *vo_dga_dpy;

//---------------------------------------------------------

// I had tried to work with mmx/3dnow copy code but
// there was not much speed gain and I didn't know
// how to save the FPU/mmx registers - so the copy
// code interferred with sound output ...
// removed the leftovers
// acki2 on 30/3/2001


#define rep_movsl(dest, src, numwords, d_add, count) \
__asm__ __volatile__( \
" \
xfer:                     \n\t\
                  movl %%edx, %%ecx \n\t \
                  cld\n\t \
                  rep\n\t \
                  movsl \n\t\
                  add %%eax, %%edi \n\t\
                  dec %%ebx \n\t\
                  jnz xfer \n\t\
" \
                  : \
                  : "a" (d_add), "b" (count), "S" (src), "D" (dest), \
		    "d" (numwords) \
                  : "memory" )


//---------------------------------------------------------

static uint32_t draw_frame( uint8_t *src[] ){

  int vp_skip = vo_dga_vp_skip;
  int lpl = vo_dga_bytes_per_line >> 2; 
  int numlines = vo_dga_lines;     

  char *s, *d;

  s = *src;
  d = (&((char *)vo_dga_base)[vo_dga_vp_offset + vo_dga_dbf_current * vo_dga_dbf_mem_offset]);
  rep_movsl(d, s, lpl, vo_dga_vp_skip, numlines );

  return 0;
}

//---------------------------------------------------------

static void check_events(void)
{
  int e=vo_x11_check_events(vo_dga_dpy);
}

//---------------------------------------------------------

static void flip_page( void ){

  
  if(vo_dga_dbf_mem_offset != 0){

#ifdef HAVE_DGA2
    XDGASetViewport (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 
		    0, vo_dga_dbf_current * vo_dga_dbf_y_offset, 
		    XDGAFlipRetrace);
#else
    XF86DGASetViewPort (vo_dga_dpy, XDefaultScreen(vo_dga_dpy),
		                        0, vo_dga_dbf_current * vo_dga_dbf_y_offset);
#endif
    vo_dga_dbf_current = 1 - vo_dga_dbf_current;
  }
  check_events();
}

//---------------------------------------------------------

static uint32_t draw_slice( uint8_t *src[],int stride[],
                            int w,int h,int x,int y )
{
  yuv2rgb( vo_dga_base + vo_dga_vp_offset + 
          (vo_dga_width * y +x) * vo_dga_bpp,
           src[0], src[1], src[2],
           w,h, vo_dga_width * vo_dga_bpp,
           stride[0],stride[1] );
  return 0;
};

//---------------------------------------------------------


static void Terminate_Display_Process( void ){
  printf("vo_dga: Terminating display process\n");
}

//---------------------------------------------------------

static const vo_info_t* get_info( void )
{ return &vo_info; }

//---------------------------------------------------------

static uint32_t query_format( uint32_t format )
{

#ifdef HAVE_DGA2	
 XDGAMode *modelines;
 int       modecount;
 Display  *qdisp;
#endif
 
 int i,k,dummy;
 static int dga_depths_init = 0;
 static int dga_depths = 0;      // each bit that is set represents
                                 // a depth the X-Server is capable
				 // of displaying

 
 if( !vo_init() ) return 0;     // Can't open X11

 if(dga_depths_init == 0){

#ifdef HAVE_DGA2
	 
   if((qdisp = XOpenDisplay(0))==NULL){
     printf("vo_dga: Can't open display!\n");
     return 0;
   }
   modelines=XDGAQueryModes(qdisp, XDefaultScreen(qdisp),&modecount);
   for(i=0; i< modecount; i++){
     if( ( (modelines[i].bitsPerPixel == 15 || 
	    modelines[i].bitsPerPixel == 16) && 
            modelines[i].redMask      == 0x7c00 &&
	    modelines[i].greenMask    == 0x03e0 &&
	    modelines[i].blueMask     == 0x001f
	  ) || 
          ( modelines[i].bitsPerPixel != 15 &&
	    modelines[i].bitsPerPixel != 16 
	  )
	)
        {
          // this only for debug reasons ...
	  if(modelines[i].bitsPerPixel == 15 || modelines[i].bitsPerPixel == 16){
              printf("vo_dga: num: %d, depth: %d, bpp: %d, %08x, %08x, %08x, %dx%d\n",
		       i,
		       modelines[i].depth,
		       modelines[i].bitsPerPixel,
		       modelines[i].redMask,
		       modelines[i].greenMask,
	               modelines[i].blueMask,
		       modelines[i].viewportWidth,
		       modelines[i].viewportHeight);			  
          }
          for(k=0, dummy=1; k<modelines[i].bitsPerPixel-1; k++)dummy <<=1;
	  dga_depths |= dummy;
        }

   }
   XCloseDisplay(qdisp);
 
#else

   for(k=0, dummy=1; k<vo_depthonscreen-1; k++)dummy <<=1;
   dga_depths |= dummy;
   // hope this shift is ok; heard that on some systems only up to 8 digits 
   // may be shifted at a time. SIGH! It IS so.
   // test for RGB masks !!!! (if depthonscreen != 24 or 32 !!!)
   if( !(vo_depthonscreen == 24 || vo_depthonscreen == 32 ) ){
      printf("vo_dga: You're running 15/16 bit X Server; your hardware might use unsuitable RGB-mask!\n");
   }
#endif
#ifdef VO_DGA_FORCE_DEPTH
   dga_depths = 1<<(VO_DGA_FORCE_DEPTH-1); 
#endif
   
   dga_depths_init = 1;
 
   if( dga_depths == 0){
     printf("vo_dga: Sorry, there seems to be no suitable depth available!\n");
     printf("        Try running X in 24 or 32 bit mode!!!\n");
     return 0;
   }else{
     for(i=0, dummy=1; i< 32; i++){
       if(dummy& dga_depths){
         printf("vo_dga: may use %2d bits per pixel\n", i+1);
       }
       dummy <<= 1;
     }
   }
 }
 if( format==IMGFMT_YV12 ) return 1;
 for(k=0, dummy=1; k<(format&0xFF)-1; k++)dummy<<=1;
 
 if( ( format&IMGFMT_BGR_MASK )==IMGFMT_BGR && 
     ( dummy & dga_depths )) return 1;
    
 return 0;
}

//---------------------------------------------------------

static void
uninit(void)
{

#ifdef HAVE_DGA2
  XDGADevice *dgadevice;
#endif

  if(vo_dga_is_running){	
    vo_dga_is_running = 0;
    printf("vo_dga: in uninit\n");
    XUngrabPointer (vo_dga_dpy, CurrentTime);
    XUngrabKeyboard (vo_dga_dpy, CurrentTime);
#ifdef HAVE_DGA2
    dgadevice = XDGASetMode(vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 0);
    if(dgadevice != NULL){
      XFree(dgadevice);	
    }
    XDGACloseFramebuffer(vo_dga_dpy, XDefaultScreen(vo_dga_dpy));
#else
    XF86DGADirectVideo (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 0);
    // first disable DirectVideo and then switch mode back!	
#ifdef HAVE_XF86VM
    if (vo_dga_vidmodes != NULL ){
      int screen; screen=XDefaultScreen( vo_dga_dpy );
      printf("vo_dga: VidModeExt: Switching back..\n");
      // seems some graphics adaptors need this more than once ...
      XF86VidModeSwitchToMode(vo_dga_dpy,screen,vo_dga_vidmodes[0]);
      XF86VidModeSwitchToMode(vo_dga_dpy,screen,vo_dga_vidmodes[0]);
      XF86VidModeSwitchToMode(vo_dga_dpy,screen,vo_dga_vidmodes[0]);
      XF86VidModeSwitchToMode(vo_dga_dpy,screen,vo_dga_vidmodes[0]);
      XFree(vo_dga_vidmodes);
    }
#endif
#endif
    XCloseDisplay(vo_dga_dpy);
  }
}


//----------------------------------------------------------

int check_mode( int num, int x, int y, int bpp,  
                int new_x, int new_y, int new_vbi, 
                int *old_x, int *old_y, int *old_vbi){

  printf("vo_dga: (%3d) Trying %4d x %4d @ %3d Hz @ %2d bpp ..",
          num, new_x, new_y, new_vbi, bpp );
  printf("(old: %dx%d@%d).", *old_x, *old_y, *old_vbi);	
  if (
      (new_x >= x) && 
      (new_y >= y) &&
      ( 
       // prefer a better resolution either in X or in Y
       // as long as the other dimension is at least the same
       // 
       // hmm ... MAYBE it would be more clever to focus on the 
       // x-resolution; I had 712x400 and 640x480 and the movie 
       // was 640x360; 640x480 would be the 'right thing' here
       // but since 712x400 was queried first I got this one. 
       // I think there should be a cmd-line switch to let the
       // user choose the mode he likes ...   (acki2)
	   
       (
	((new_x < *old_x) &&
	 !(new_y > *old_y)) ||
	((new_y < *old_y) &&
	 !(new_x > *old_x)) 
       ) 
       // but if we get an identical resolution choose
       // the one with the lower refreshrate (saves bandwidth !!!)
       // as long as it's above 50 Hz (acki2 on 30/3/2001)
       ||
       (
	(new_x == *old_x) &&
	(new_y == *old_y) &&
	(
	 (
	  new_vbi >= *old_vbi && *old_vbi < 50
	 )  
	 ||
	 (
	  *old_vbi >= 50 && 
	  new_vbi < *old_vbi &&
	  new_vbi >= 50
	 )
	)
       )
      )
     )  
    {
      *old_x = new_x;
      *old_y = new_y;
      *old_vbi = new_vbi;
      printf(".ok!!\n");
      return 1;
    }else{
      printf(".no\n");
      return 0;
    }
}



//---------------------------------------------------------

static uint32_t init( uint32_t width,  uint32_t height,
                      uint32_t d_width,uint32_t d_height,
                      uint32_t fullscreen,char *title,uint32_t format )
{

  int x_off, y_off;

#ifdef HAVE_DGA2
  // needed to change DGA video mode
  int modecount, mX=100000, mY=100000 , mVBI=100000, i,j=0;
  int dga_modenum;
  XDGAMode   *modelines=NULL;
  XDGADevice *dgadevice;
  int max_vpy_pos;
#else
#ifdef HAVE_XF86VM
  unsigned int vm_event, vm_error;
  unsigned int vm_ver, vm_rev;
  int i, j=0, have_vm=0;
  int modecount, mX=100000, mY=100000, mVBI=100000, dga_modenum;  
#endif
  int bank, ram;
#endif

  if( vo_dga_is_running )return -1;

  
  if( !vo_init() ){
    printf("vo_dga: vo_init() failed!\n");
    return 0; 
  }
 
  if (format == IMGFMT_YV12 ){
    vo_dga_planes = vo_depthonscreen;
    vo_dga_planes = vo_dga_planes == 15 ? 16 : vo_dga_planes;
  }else{
    vo_dga_planes = (format & 0xff);

    // hack!!! here we should only get what we told we can handle in 
    // query_format() but mplayer is somewhat generous about 
    // 15/16bit depth ...
    
    vo_dga_planes = vo_dga_planes == 15 ? 16 : vo_dga_planes;
  }
  
  if((vo_dga_dpy = XOpenDisplay(0))==NULL)
  {
    printf ("vo_dga: Can't open display\n");
    return 1;
  } 

  vo_dga_bpp = (vo_dga_planes+7) >> 3;

  // TODO: find out screen resolution of X-Server here and
  //       provide them as default values (used only in case
  //       DGA1.0 and no VidMode Ext or VidModeExt doesn't return
  //       any screens to check if video is larger than current screen)

  vo_dga_vp_width = 1280;
  vo_dga_vp_height = 1024;



// choose a suitable mode ...
  
#ifdef HAVE_DGA2
// Code to change the video mode added by Michael Graffam
// mgraffam@idsi.net
  if (modelines==NULL)
    modelines=XDGAQueryModes(vo_dga_dpy, XDefaultScreen(vo_dga_dpy),&modecount);
  
  printf("vo_dga: Using DGA 2.0 mode changing support\n");	
  // offbyone-error !!! i<=modecount is WRONG !!!
  for (i=0; i<modecount; i++)
  {
     if( modelines[i].bitsPerPixel == vo_dga_planes)
     {

       printf("maxy: %4d, depth: %2d, %4dx%4d, ", modelines[i].maxViewportY, modelines[i].depth,
		       modelines[i].imageWidth, modelines[i].imageHeight );
       if ( check_mode(i, d_width, d_height, modelines[i].bitsPerPixel,  
                  modelines[i].viewportWidth, 
                  modelines[i].viewportHeight, 
                  (unsigned) modelines[i].verticalRefresh,
                   &mX, &mY, &mVBI )) j = i;
     }
  }
  printf("vo_dga: Selected video mode %4d x %4d @ %3d Hz for image size %3d x %3d.\n", 
		  mX, mY, mVBI, width, height);  

  vo_dga_vp_width =mX;
  vo_dga_vp_height = mY;
  vo_dga_width = modelines[j].bytesPerScanline / vo_dga_bpp;
  dga_modenum =  modelines[j].num;
  max_vpy_pos = modelines[j].maxViewportY;
  
  XFree(modelines);
  modelines = NULL;
  
#else

#ifdef HAVE_XF86VM

  printf("vo_dga: DGA 1.0 compatibility code: Using XF86VidMode for mode switching!\n");

  if (XF86VidModeQueryExtension(vo_dga_dpy, &vm_event, &vm_error)) {
    XF86VidModeQueryVersion(vo_dga_dpy, &vm_ver, &vm_rev);
    printf("vo_dga: XF86VidMode Extension v%i.%i\n", vm_ver, vm_rev);
    have_vm=1;
  } else {
    printf("vo_dga: XF86VidMode Extension not available.\n");
  }

#define GET_VREFRESH(dotclk, x, y)( (((dotclk)/(x))*1000)/(y) )
  
  if (have_vm) {
    int screen;
    screen=XDefaultScreen(vo_dga_dpy);
    XF86VidModeGetAllModeLines(vo_dga_dpy,screen,&modecount,&vo_dga_vidmodes);

    if(vo_dga_vidmodes != NULL ){
      for (i=0; i<modecount; i++){
	if ( check_mode(i, d_width, d_height, vo_dga_planes,  
			vo_dga_vidmodes[i]->hdisplay, 
			vo_dga_vidmodes[i]->vdisplay,
			GET_VREFRESH(vo_dga_vidmodes[i]->dotclock, 
				     vo_dga_vidmodes[i]->htotal,
				     vo_dga_vidmodes[i]->vtotal),
			&mX, &mY, &mVBI )) j = i;
      }
    
      printf("vo_dga: Selected video mode %4d x %4d @ %3d Hz for image size %3d x %3d.\n", 
	     mX, mY, mVBI, width, height);  
    }else{
      printf("vo_dga: XF86VidMode returned no screens - using current resolution.\n");
    }
    dga_modenum = j;
    vo_dga_vp_width = mX;
    vo_dga_vp_height = mY;
  }


#else
  printf("vo_dga: Only have DGA 1.0 extension and no XF86VidMode :-(\n");
#endif
#endif

  vo_dga_src_format = format;
  vo_dga_src_width = width;
  vo_dga_src_height = height;
	
  if(vo_dga_src_width > vo_dga_vp_width ||
     vo_dga_src_height > vo_dga_vp_height)
  {
     printf("vo_dga: Sorry, video larger than viewport is not yet supported!\n");
     // ugly, do something nicer in the future ...
#ifndef HAVE_DGA2
#ifdef HAVE_XF86VM
     if(vo_dga_vidmodes){
	XFree(vo_dga_vidmodes);
	vo_dga_vidmodes = NULL;
     }
#endif
#endif
     return 1;
  }
		         
// now lets start the DGA thing 

#ifdef HAVE_DGA2
    
  if (!XDGAOpenFramebuffer(vo_dga_dpy, XDefaultScreen(vo_dga_dpy))){
    printf("vo_dga: Framebuffer mapping failed!!!\n");
    XCloseDisplay(vo_dga_dpy);
    return 1;
  }
  dgadevice=XDGASetMode(vo_dga_dpy, XDefaultScreen(vo_dga_dpy), dga_modenum);
  XDGASync(vo_dga_dpy, XDefaultScreen(vo_dga_dpy));

  vo_dga_base = dgadevice->data;
  XFree(dgadevice);

  XDGASetViewport (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 0, 0, XDGAFlipRetrace);
  
#else
  
#ifdef HAVE_XF86VM
    XF86VidModeLockModeSwitch(vo_dga_dpy,XDefaultScreen(vo_dga_dpy),0);
    // Two calls are needed to switch modes on my ATI Rage 128. Why?
    // for riva128 one call is enough!
    XF86VidModeSwitchToMode(vo_dga_dpy,XDefaultScreen(vo_dga_dpy),vo_dga_vidmodes[dga_modenum]);
    XF86VidModeSwitchToMode(vo_dga_dpy,XDefaultScreen(vo_dga_dpy),vo_dga_vidmodes[dga_modenum]);
#endif
  
  XF86DGAGetViewPortSize(vo_dga_dpy,XDefaultScreen(vo_dga_dpy),
		         &vo_dga_vp_width,
			 &vo_dga_vp_height); 

  XF86DGAGetVideo (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 
		   (char **)&vo_dga_base, &vo_dga_width, &bank, &ram);

  XF86DGADirectVideo (vo_dga_dpy, XDefaultScreen(vo_dga_dpy),
                      XF86DGADirectGraphics | XF86DGADirectMouse |
                      XF86DGADirectKeyb);
  
  XF86DGASetViewPort (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 0, 0);

#endif

  // do some more checkings here ...

  if( format==IMGFMT_YV12 ) 
    yuv2rgb_init( vo_dga_planes == 16 ? 15 : vo_dga_planes , MODE_RGB );

  printf("vo_dga: bytes/line: %d, screen res: %dx%d, depth: %d, base: %08x, bpp: %d\n", 
          vo_dga_width, vo_dga_vp_width, 
          vo_dga_vp_height, vo_dga_planes, vo_dga_base,
          vo_dga_bpp);

  x_off = (vo_dga_vp_width - vo_dga_src_width)>>1; 
  y_off = (vo_dga_vp_height - vo_dga_src_height)>>1;

  vo_dga_bytes_per_line = vo_dga_src_width * vo_dga_bpp; 
  vo_dga_lines = vo_dga_src_height;                     

  vo_dga_src_offset = 0;
  vo_dga_vp_offset = (y_off * vo_dga_width + x_off ) * vo_dga_bpp;

  vo_dga_vp_skip = (vo_dga_width - vo_dga_src_width) * vo_dga_bpp;  // todo
    
  printf("vo_dga: vp_off=%d, vp_skip=%d, bpl=%d\n", 
         vo_dga_vp_offset, vo_dga_vp_skip, vo_dga_bytes_per_line);

  
  XGrabKeyboard (vo_dga_dpy, DefaultRootWindow(vo_dga_dpy), True, 
                 GrabModeAsync,GrabModeAsync, CurrentTime);
  XGrabPointer (vo_dga_dpy, DefaultRootWindow(vo_dga_dpy), True, 
                ButtonPressMask,GrabModeAsync, GrabModeAsync, 
                None, None, CurrentTime);
// TODO: chekc if mem of graphics adaptor is large enough for dbf

  // set up variables for double buffering ...
  // note: set vo_dga_dbf_mem_offset to NULL to disable doublebuffering
  
  vo_dga_dbf_y_offset = y_off + vo_dga_src_height;
  vo_dga_dbf_mem_offset = vo_dga_width * vo_dga_bpp *  vo_dga_dbf_y_offset;
  vo_dga_dbf_current = 0;
  
  if(format ==IMGFMT_YV12 )vo_dga_dbf_mem_offset = 0;
  // disable doublebuffering for YV12

#ifdef HAVE_DGA2
      if(vo_dga_vp_height>max_vpy_pos){
        vo_dga_dbf_mem_offset = 0;
	printf("vo_dga: Not enough memory for double buffering!\n");
      }
#endif  
  
  // now clear screen
  {
    int size = vo_dga_width *
	(vo_dga_vp_height + (vo_dga_dbf_mem_offset != 0 ?
	(vo_dga_src_height+y_off) : 0)) *
	vo_dga_bpp;
#ifndef HAVE_DGA2
    printf("%d, %d\n", size, ram);
    if(size>ram*1024){
      vo_dga_dbf_mem_offset = 0;
      printf("vo_dga: Not enough memory for double buffering!\n");
      size -= (vo_dga_src_height+y_off) * vo_dga_width * vo_dga_bpp;
    }				        
#endif
    
    printf("vo_dga: Clearing framebuffer (%d bytes). If mplayer exits", size);
    printf(" here, you haven't enough memory on your card.\n");   
    fflush(stdout);
    memset(vo_dga_base, 0, size);  
  }
  printf("vo_dga: Doublebuffering %s.\n", vo_dga_dbf_mem_offset ? "enabled" : "disabled");
  vo_dga_is_running = 1;
  return 0;
}

//---------------------------------------------------------

// deleted the old vo_dga_query_event() routine 'cause it is obsolete  
// since using check_events()
// acki2 on 30/3/2001
