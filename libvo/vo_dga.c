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
 * - covers only common video card formats i.e. 
 *      BGR_16_15_555
 *      BGR_16_16_565
 *      BGR_24_24_888
 *      BGR_32_24_888
 *
 * - works only on x86 architectures
 *
 * $Log$
 * Revision 1.15  2001/04/19 21:39:10  arpi_esp
 * driver info now depends on detected DGA version
 *
 * Revision 1.14  2001/04/17 22:28:09  acki2
 * - now also supports OSD for YV12 (big speed penalty by having to build image
 *   in offscreen memory and then copying;
 * - OSD still works just with doublebuffering enabled :-(
 *
 * Revision 1.13  2001/04/17 20:51:58  acki2
 * - query_format() now uses new return value concept
 * - now support for OSD :-))) for RGB modes
 *   YV12 is flickering in quite an ugly fashion; have to fix this, but
 *   will cost an extra copying of image data ... :-(((
 *
 * Revision 1.12  2001/04/13 22:11:08  acki2
 * - fixed bug with depth and mpg when current bpp of XServer was != 32
 * - when -bpp is selected, I accept only query_modes() for THIS particular depth
 *   (if it's supported by hardware)
 *
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

//#define VO_DGA_DBG 1
//#undef HAVE_DGA2
//#undef HAVE_XF86VM

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "yuv2rgb.h"

LIBVO_EXTERN( dga )

#include <X11/Xlib.h>
#include <X11/extensions/xf86dga.h>

#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif


#include "x11_common.h"

static vo_info_t vo_info =
{
#ifdef HAVE_DGA2
        "DGA ( Direct Graphic Access V2.0 )",
#else
        "DGA ( Direct Graphic Access V1.0+XF86VidModeExtension )",
#endif
        "dga",
        "Andreas Ackermann <acki@acki-netz.de>",
        ""
};


//------------------------------------------------------------------


#define BITSPP (vo_dga_modes[vo_dga_active_mode].vdm_bitspp)
#define BYTESPP (vo_dga_modes[vo_dga_active_mode].vdm_bytespp)

#define HW_MODE (vo_dga_modes[vo_dga_active_mode])


struct vd_modes {
  int    vdm_mplayer_depth;
  int    vdm_supported;
  int    vdm_depth;
  int    vdm_bitspp;
  int    vdm_bytespp;
  int    vdm_rmask;
  int    vdm_gmask;
  int    vdm_bmask;
};

//------------------------------------------------------------------

static struct vd_modes vo_dga_modes[] = {

  {  0,  0,  0,  0, 0,          0,          0, 0},
  { 15,  0, 15, 16, 2,     0x7c00,     0x03e0, 0x001f },
  { 16,  0, 16, 16, 2,     0xf800,     0x07e0, 0x001f },
  { 24,  0, 24, 24, 3,   0xff0000,   0x00ff00, 0x0000ff},
  { 32,  0, 24, 32, 4, 0x00ff0000, 0x0000ff00, 0x000000ff}
};

static int vo_dga_mode_num = sizeof(vo_dga_modes)/sizeof(struct vd_modes);

int vd_EnableMode( int depth, int bitspp, 
                    int rmask, int gmask, int bmask){
  int i;
  for(i=1; i<vo_dga_mode_num; i++){
    if(vo_dga_modes[i].vdm_depth == depth &&
       vo_dga_modes[i].vdm_bitspp == bitspp &&
       vo_dga_modes[i].vdm_rmask == rmask &&
       vo_dga_modes[i].vdm_gmask == gmask &&
       vo_dga_modes[i].vdm_bmask == bmask){
       vo_dga_modes[i].vdm_supported = 1;
       return i;
    }
  }
  return 0;
}

int vd_ModeEqual(int depth, int bitspp, 
		 int rmask, int gmask, int bmask, int index){
  return (
     (vo_dga_modes[index].vdm_depth == depth &&
     vo_dga_modes[index].vdm_bitspp == bitspp &&
     vo_dga_modes[index].vdm_rmask == rmask &&
     vo_dga_modes[index].vdm_gmask == gmask &&
     vo_dga_modes[index].vdm_bmask == bmask)
     ? 1 : 0); 
}


int vd_ValidateMode( int mplayer_depth){
  int i;
  if(mplayer_depth == 0)return 0;
  for(i=1; i<vo_dga_mode_num; i++){
    if(vo_dga_modes[i].vdm_mplayer_depth == mplayer_depth ){ 
      vo_dga_modes[i].vdm_supported = 1;
      return i;
    }
  }
  return 0;
}

int vd_ModeValid( int mplayer_depth){
  int i;
  if(mplayer_depth == 0)return 0;
  for(i=1; i<vo_dga_mode_num; i++){
    if(vo_dga_modes[i].vdm_mplayer_depth == mplayer_depth && 
       vo_dga_modes[i].vdm_supported != 0){
      return i;
    }
  }
  return 0;
}

char *vd_GetModeString(int index){

#define VO_DGA_MAX_STRING_LEN 100
  static char stringbuf[VO_DGA_MAX_STRING_LEN]; 
  stringbuf[VO_DGA_MAX_STRING_LEN-1]=0;
  snprintf(stringbuf, VO_DGA_MAX_STRING_LEN-2, 
    "depth=%d, bpp=%d, r=%06x, g=%06x, b=%06x (-bpp %d)",
    vo_dga_modes[index].vdm_depth,
    vo_dga_modes[index].vdm_bitspp,
    vo_dga_modes[index].vdm_rmask,
    vo_dga_modes[index].vdm_gmask,
    vo_dga_modes[index].vdm_bmask,
    vo_dga_modes[index].vdm_mplayer_depth);
  return stringbuf;
}

//-----------------------------------------------------------------

#ifdef HAVE_XF86VM
static XF86VidModeModeInfo **vo_dga_vidmodes=NULL;
#endif


extern int       verbose;          
extern int       vo_dbpp;

static int       vo_dga_src_format;
static int       vo_dga_width;           // bytes per line in framebuffer
static int       vo_dga_vp_width;        // visible pixels per line in 
                                         // framebuffer
static int       vo_dga_vp_height;       // visible lines in framebuffer
static int       vo_dga_is_running = 0; 
static int       vo_dga_src_width;       // width of video in pixels
static int       vo_dga_src_height;      // height of video in pixels
static int       vo_dga_src_offset=0;    // offset in src
static int       vo_dga_vp_offset=0;     // offset in dest
static int       vo_dga_bytes_per_line;  // bytes per line to copy
static int       vo_dga_src_skip;        // bytes to skip after copying one 
                                         // line 
                                         // (not supported yet) in src
static int       vo_dga_vp_skip;         // dto. for dest 
static int       vo_dga_lines;           // num of lines to copy                                
static int       vo_dga_active_mode = 0; // index in mode list that is used
                                         // for movie
static int       vo_dga_XServer_mode = 0;// index in mode list for resolution
                                         // XServer is running

static int       vo_dga_dbf_mem_offset;  // offset in bytes for alternative 
                                         // framebuffer (0 if dbf is not 
					 // possible)
static int       vo_dga_dbf_y_offset;    // y offset (in scanlines)
static int       
                 vo_dga_dbf_current;     // current buffer (0 or 1)

static unsigned char     *vo_dga_base;
static unsigned char     *vo_dga_yv12_base = NULL;
static Display  *vo_dga_dpy;

//---------------------------------------------------------

#define VD_INFO  0
#define VD_ERR   0
#define VD_DBG   2
#define VD_RES   1

void vd_printf( int level, const char *str, ...){
  
#ifndef VO_DGA_DBG
  // show resolution and DBG-messages only in verbose mode ...
  if( !verbose && level)return;         
#endif
  vprintf( str, (&str)+1 );
}

//---------------------------------------------------------


static void draw_alpha( int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride ){

  char *d;
  unsigned int offset;
  unsigned int buffer_stride;

  offset = vo_dga_width * y0 +x0;
  buffer_stride = vo_dga_width;
  d = (&((char *)vo_dga_base)[vo_dga_vp_offset + vo_dga_dbf_current * vo_dga_dbf_mem_offset]);
     
  switch( HW_MODE.vdm_mplayer_depth ){

  case 32: 
    vo_draw_alpha_rgb32(w,h,src,srca,stride, d+4*offset , 4*buffer_stride); 
    break;
  case 24: 
    vo_draw_alpha_rgb24(w,h,src,srca,stride, d+3*offset , 3*buffer_stride);
    break;
  case 15:
    vo_draw_alpha_rgb15(w,h,src,srca,stride, d+2*offset , 2*buffer_stride);
    break;
  case 16:        
    vo_draw_alpha_rgb16(w,h,src,srca,stride, d+2*offset , 2*buffer_stride);
    break;
  }
}


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
1:                     \n\t\
                  movl %%edx, %%ecx \n\t \
                  cld\n\t \
                  rep\n\t \
                  movsl \n\t\
                  add %%eax, %%edi \n\t\
                  dec %%ebx \n\t\
                  jnz 1b \n\t\
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

  if(vo_dga_src_format ==IMGFMT_YV12 ){
       draw_frame( &vo_dga_yv12_base);
  }

  vo_draw_text(vo_dga_src_width,vo_dga_src_height,draw_alpha);
  
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

  // for osd, we need a separate buffer here ... :-()

    yuv2rgb( vo_dga_yv12_base + (vo_dga_src_width * y +x) * BYTESPP,
         src[0], src[1], src[2],
         w,h, vo_dga_src_width * BYTESPP,
         stride[0],stride[1] );

  //  yuv2rgb( vo_dga_base + vo_dga_vp_offset + 
  //        (vo_dga_width * y +x) * BYTESPP,
  //         src[0], src[1], src[2],
  //         w,h, vo_dga_width * BYTESPP,
  //         stride[0],stride[1] );
  return 0;
};

//---------------------------------------------------------


static void Terminate_Display_Process( void ){
  vd_printf(VD_DBG, "vo_dga: Terminating display process\n");
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
#endif
 Display  *qdisp;

 int i;
 static int dga_depths_init = 0;

 if(dga_depths_init == 0){

   if((qdisp = XOpenDisplay(0))==NULL){
     vd_printf(VD_ERR, "vo_dga: Can't open display!\n");
     return 0;
   }
   if( !vo_init() ){
    vd_printf(VD_ERR, "vo_dga: vo_init() failed!\n");
    return 1; 
  }
  vo_dga_XServer_mode = vd_ValidateMode(vo_depthonscreen);
 
   if(vo_dga_XServer_mode ==0){
#ifndef HAVE_DGA2
     vd_printf(VD_ERR, "vo_dga: Your X-Server is not running in a ");
     vd_printf(VD_ERR, "resolution supported by DGA driver!\n");
#endif     
   }else{
     vd_printf(VD_INFO, "vo_dga: X running at: %s\n", 
               vd_GetModeString(vo_dga_XServer_mode));
   }                                
 
#ifdef HAVE_DGA2
   modelines=XDGAQueryModes(qdisp, XDefaultScreen(qdisp),&modecount);
   if(modelines){
     for(i=0; i< modecount; i++){
        vd_printf(VD_DBG, "vo_dga: (%03d) depth=%d, bpp=%d, r=%08x, g=%08x, b=%08x, %d x %d\n",
	  	i,
		modelines[i].depth,
		modelines[i].bitsPerPixel,
		modelines[i].redMask,
		modelines[i].greenMask,
	        modelines[i].blueMask,
	 	modelines[i].viewportWidth,
		modelines[i].viewportHeight);			  
        vd_EnableMode(
		modelines[i].depth,
		modelines[i].bitsPerPixel,
		modelines[i].redMask,
		modelines[i].greenMask,
	        modelines[i].blueMask);
     }
     XFree(modelines);
     dga_depths_init = 1;
   }
#endif

   XCloseDisplay(qdisp);

   for(i=0; i<vo_dga_mode_num; i++){
     if(vo_dga_modes[i].vdm_supported != 0){
       vd_printf(VD_INFO, "vo_dga: Supporting mode: %s", vd_GetModeString(i));
       if(vo_dbpp && vo_dbpp != vo_dga_modes[i].vdm_mplayer_depth){
         vo_dga_modes[i].vdm_supported = 0;
         vd_printf(VD_INFO, " ...disabled by -bpp %d", vo_dbpp );
       }
       vd_printf(VD_INFO, "\n");
     }
   }
 }

 if( format==IMGFMT_YV12 ) return 7;
 
 if( (format&IMGFMT_BGR_MASK) == IMGFMT_BGR && 
     vd_ModeValid(format&0xff)) return 7;
 
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
    if(vo_dga_yv12_base)free(vo_dga_yv12_base);
    vo_dga_yv12_base = NULL;
    vo_dga_is_running = 0;
    vd_printf( VD_DBG, "vo_dga: in uninit\n");
    XUngrabPointer (vo_dga_dpy, CurrentTime);
    XUngrabKeyboard (vo_dga_dpy, CurrentTime);
#ifdef HAVE_DGA2
    XDGACloseFramebuffer(vo_dga_dpy, XDefaultScreen(vo_dga_dpy));
    dgadevice = XDGASetMode(vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 0);
    if(dgadevice != NULL){
      XFree(dgadevice);	
    }
#else
    XF86DGADirectVideo (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 0);
    // first disable DirectVideo and then switch mode back!	
#ifdef HAVE_XF86VM
    if (vo_dga_vidmodes != NULL ){
      int screen; screen=XDefaultScreen( vo_dga_dpy );
      vd_printf(VD_DBG, "vo_dga: VidModeExt: Switching back..\n");
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
// TODO: check for larger maxy value 
// (useful for double buffering!!!)

int check_res( int num, int x, int y, int bpp,  
                int new_x, int new_y, int new_vbi, 
                int *old_x, int *old_y, int *old_vbi){

  vd_printf(VD_RES, "vo_dga: (%3d) Trying %4d x %4d @ %3d Hz @ depth %2d ..",
          num, new_x, new_y, new_vbi, bpp );
  vd_printf(VD_RES, "(old: %dx%d@%d).", *old_x, *old_y, *old_vbi);	
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
      vd_printf(VD_RES, ".ok!!\n");
      return 1;
    }else{
      vd_printf(VD_RES, ".no\n");
      return 0;
    }
}



//---------------------------------------------------------

static uint32_t init( uint32_t width,  uint32_t height,
                      uint32_t d_width,uint32_t d_height,
                      uint32_t fullscreen,char *title,uint32_t format )
{

  int x_off, y_off;
  int wanted_width, wanted_height;

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
  vo_dga_src_format = format;  
  wanted_width = d_width;
  wanted_height = d_height;

  if(!wanted_height) wanted_height = height;
  if(!wanted_width)  wanted_width = width;

  if( !vo_init() ){
    vd_printf(VD_ERR, "vo_dga: vo_init() failed!\n");
    return 1; 
  }

  if( !vo_dbpp ){
 
    if (format == IMGFMT_YV12){
      vo_dga_active_mode = vo_dga_XServer_mode;
    }else if((format & IMGFMT_BGR_MASK) == IMGFMT_BGR){
      vo_dga_active_mode = vd_ModeValid( format & 0xff );
    }
  }else{
    vo_dga_active_mode = vd_ModeValid(vo_dbpp);
  }

  if(!vo_dga_active_mode){ 
    vd_printf(VD_ERR, "vo_dga: unsupported video format!\n");
    return 1;
  }
  
  if((vo_dga_dpy = XOpenDisplay(0))==NULL){
    vd_printf (VD_ERR, "vo_dga: Can't open display\n");
    return 1;
  } 

  if(format ==IMGFMT_YV12 ){
    vo_dga_yv12_base = malloc(wanted_width * wanted_height * BYTESPP);
    if(vo_dga_yv12_base== NULL){
      vd_printf(VD_ERR, "vo_dga: Not enough memory for offscreen YV12 buffer!\n");
      return 1;
    }
  }


  vo_dga_vp_width = DisplayWidth( vo_dga_dpy, DefaultScreen(vo_dga_dpy));
  vo_dga_vp_height = DisplayHeight( vo_dga_dpy, DefaultScreen(vo_dga_dpy));

  vd_printf(VD_DBG, "vo_dga: XServer res: %dx%d\n", 
                     vo_dga_vp_width, vo_dga_vp_height);

// choose a suitable mode ...
  
#ifdef HAVE_DGA2
// Code to change the video mode added by Michael Graffam
// mgraffam@idsi.net
  if (modelines==NULL)
    modelines=XDGAQueryModes(vo_dga_dpy, XDefaultScreen(vo_dga_dpy),&modecount);
  
  vd_printf(VD_INFO, 
            "vo_dga: DGA 2.0 available :-) Can switch resolution AND depth!\n");	
  for (i=0; i<modecount; i++)
  {
    if(vd_ModeEqual( modelines[i].depth, 
                     modelines[i].bitsPerPixel,
                     modelines[i].redMask,
		     modelines[i].greenMask,
	             modelines[i].blueMask,
                     vo_dga_active_mode)){

       vd_printf(VD_DBG, "maxy: %4d, depth: %2d, %4dx%4d, ", 
                       modelines[i].maxViewportY, modelines[i].depth,
		       modelines[i].imageWidth, modelines[i].imageHeight );
       if ( check_res(i, wanted_width, wanted_height, modelines[i].depth,  
                  modelines[i].viewportWidth, 
                  modelines[i].viewportHeight, 
                  (unsigned) modelines[i].verticalRefresh,
                   &mX, &mY, &mVBI )) j = i;
     }
  }
  vd_printf(VD_INFO, 
  "vo_dga: Selected video mode %4d x %4d @ %3d Hz @ depth %2d, bitspp %2d, video %3d x %3d.\n", 
     mX, mY, mVBI,
     vo_dga_modes[vo_dga_active_mode].vdm_depth,
     vo_dga_modes[vo_dga_active_mode].vdm_bitspp,
     width, height);  

  vo_dga_vp_width =mX;
  vo_dga_vp_height = mY;
  vo_dga_width = modelines[j].bytesPerScanline / BYTESPP ;
  dga_modenum =  modelines[j].num;
  max_vpy_pos =  modelines[j].maxViewportY;
  
  XFree(modelines);
  modelines = NULL;
  
#else

#ifdef HAVE_XF86VM

  vd_printf( VD_INFO, 
     "vo_dga: DGA 1.0 compatibility code: Using XF86VidMode for mode switching!\n");

  if (XF86VidModeQueryExtension(vo_dga_dpy, &vm_event, &vm_error)) {
    XF86VidModeQueryVersion(vo_dga_dpy, &vm_ver, &vm_rev);
    vd_printf(VD_INFO, "vo_dga: XF86VidMode Extension v%i.%i\n", vm_ver, vm_rev);
    have_vm=1;
  } else {
    vd_printf(VD_ERR, "vo_dga: XF86VidMode Extension not available.\n");
  }

#define GET_VREFRESH(dotclk, x, y)( (((dotclk)/(x))*1000)/(y) )
  
  if (have_vm) {
    int screen;
    screen=XDefaultScreen(vo_dga_dpy);
    XF86VidModeGetAllModeLines(vo_dga_dpy,screen,&modecount,&vo_dga_vidmodes);

    if(vo_dga_vidmodes != NULL ){
      for (i=0; i<modecount; i++){
	if ( check_res(i, wanted_width, wanted_height, 
                        vo_dga_modes[vo_dga_active_mode].vdm_depth,  
			vo_dga_vidmodes[i]->hdisplay, 
			vo_dga_vidmodes[i]->vdisplay,
			GET_VREFRESH(vo_dga_vidmodes[i]->dotclock, 
				     vo_dga_vidmodes[i]->htotal,
				     vo_dga_vidmodes[i]->vtotal),
			&mX, &mY, &mVBI )) j = i;
      }
    
      vd_printf(VD_INFO, 
 "vo_dga: Selected video mode %4d x %4d @ %3d Hz @ depth %2d, bitspp %2d, video %3d x %3d.\n", 
	mX, mY, mVBI, 
	vo_dga_modes[vo_dga_active_mode].vdm_depth,
	vo_dga_modes[vo_dga_active_mode].vdm_bitspp,
        width, height);  
    }else{
      vd_printf(VD_INFO, "vo_dga: XF86VidMode returned no screens - using current resolution.\n");
    }
    dga_modenum = j;
    vo_dga_vp_width = mX;
    vo_dga_vp_height = mY;
  }


#else
  vd_printf( VD_INFO, "vo_dga: Only have DGA 1.0 extension and no XF86VidMode :-(\n");
  vd_printf( VD_INFO, "        Thus, resolution switching is NOT possible.\n");

#endif
#endif

  vo_dga_src_width = width;
  vo_dga_src_height = height;
	
  if(vo_dga_src_width > vo_dga_vp_width ||
     vo_dga_src_height > vo_dga_vp_height)
  {
     vd_printf( VD_ERR, "vo_dga: Sorry, video larger than viewport is not yet supported!\n");
     // ugly, do something nicer in the future ...
#ifndef HAVE_DGA2
#ifdef HAVE_XF86VM
     if(vo_dga_vidmodes){
	XFree(vo_dga_vidmodes);
	vo_dga_vidmodes = NULL;
     }
#endif
#endif
     if(vo_dga_yv12_base){
       free(vo_dga_yv12_base);
       vo_dga_yv12_base = NULL;
     }
     return 1;
  }
		         
// now lets start the DGA thing 

#ifdef HAVE_DGA2
    
  if (!XDGAOpenFramebuffer(vo_dga_dpy, XDefaultScreen(vo_dga_dpy))){
    vd_printf(VD_ERR, "vo_dga: Framebuffer mapping failed!!!\n");
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

  if( format==IMGFMT_YV12 ){ 
    yuv2rgb_init( vo_dga_modes[vo_dga_active_mode].vdm_mplayer_depth , MODE_RGB );
    vd_printf( VD_DBG, "vo_dga: Using mplayer depth %d for YV12\n", 
               vo_dga_modes[vo_dga_active_mode].vdm_mplayer_depth);
  }

  vd_printf(VD_DBG, "vo_dga: bytes/line: %d, screen res: %dx%d, depth: %d, base: %08x, bpp: %d\n", 
          vo_dga_width, vo_dga_vp_width, 
          vo_dga_vp_height, BYTESPP, vo_dga_base,
          BITSPP);

  x_off = (vo_dga_vp_width - vo_dga_src_width)>>1; 
  y_off = (vo_dga_vp_height - vo_dga_src_height)>>1;

  vo_dga_bytes_per_line = vo_dga_src_width * BYTESPP; 
  vo_dga_lines = vo_dga_src_height;                     

  vo_dga_src_offset = 0;
  vo_dga_vp_offset = (y_off * vo_dga_width + x_off ) * BYTESPP;

  vo_dga_vp_skip = (vo_dga_width - vo_dga_src_width) * BYTESPP;  // todo
    
  vd_printf(VD_DBG, "vo_dga: vp_off=%d, vp_skip=%d, bpl=%d\n", 
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
  vo_dga_dbf_mem_offset = vo_dga_width * BYTESPP *  vo_dga_dbf_y_offset;
  vo_dga_dbf_current = 0;
  
  // if(format ==IMGFMT_YV12 )
  //vo_dga_dbf_mem_offset = 0;
  // disable doublebuffering for YV12

#ifdef HAVE_DGA2
      if(vo_dga_vp_height>max_vpy_pos){
        vo_dga_dbf_mem_offset = 0;
	vd_printf(VD_INFO, "vo_dga: Not enough memory for double buffering!\n");
      }
#endif  
  
  // now clear screen
  {
    int size = vo_dga_width *
	(vo_dga_vp_height + (vo_dga_dbf_mem_offset != 0 ?
	(vo_dga_src_height+y_off) : 0)) *
	BYTESPP;
#ifndef HAVE_DGA2
    vd_printf(VD_DBG, "vo_dga: wanted size=%d, fb-size=%d\n", size, ram);
    if(size>ram*1024){
      vo_dga_dbf_mem_offset = 0;
      vd_printf(VD_INFO, "vo_dga: Not enough memory for double buffering!\n");
      size -= (vo_dga_src_height+y_off) * vo_dga_width * BYTESPP;
    }				        
#endif
    
    vd_printf(VD_INFO, "vo_dga: Clearing framebuffer (%d bytes). If mplayer exits", size);
    vd_printf(VD_INFO, " here, you haven't enough memory on your card.\n");   
    fflush(stdout);
    memset(vo_dga_base, 0, size);  
  }
  vd_printf(VD_INFO, "vo_dga: Doublebuffering is %s.\n", vo_dga_dbf_mem_offset ? "enabled" : "disabled");
  vo_dga_is_running = 1;
  return 0;
}

//---------------------------------------------------------

// deleted the old vo_dga_query_event() routine 'cause it is obsolete  
// since using check_events()
// acki2 on 30/3/2001










