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
 * $Log$
 * Revision 1.47  2002/09/25 21:08:44  arpi
 * Attached patch improves the vertical retrace synchronisation (vsync)
 * of the mplayer DGA driver.
 * It implements a more general buffer scheme than double buffers that
 * allows for deeper buffer queues.
 *
 * by Fredrik Noring <noring@nocrew.org>
 *
 * Revision 1.46  2002/08/28 21:32:31  alex
 * finally removed query_vaa, bes_da and vo_tune_info - the obsoleted libvo api
 *
 * Revision 1.45  2002/05/27 17:14:13  alex
 * revised query_format. added support for native/conversion detection
 *
 * Revision 1.44  2002/02/17 08:24:43  nick
 * I don't like such reports: '-vo dga:vidix or -vo x11:vidix works fine for me'
 *
 * Revision 1.42  2002/02/12 23:19:37  michael
 * use mem2agpcpy() instead of fast_memcpy()
 *
 * Revision 1.41  2002/02/09 01:21:48  arpi
 * 10000hl to Holm... control MUST BE static...
 *
 * Revision 1.40  2002/02/09 00:47:26  arpi
 * query_ stuff replaced by new control() - patch by David Holm
 *
 * Revision 1.39  2002/01/31 11:45:25  alex
 * removed obsoleted Terminate_Display_Process
 *
 * Revision 1.38  2002/01/31 09:52:45  nick
 * new info for tuning
 *
 * Revision 1.35  2001/12/28 20:52:54  alex
 * use XF86VidMode later in init (at line 1031) only if we've got support (if have_vm==1)
 *
 * Revision 1.34  2001/12/04 17:24:25  alex
 * do not crash if can't get modelines (dga2.0)
 *
 * Revision 1.33  2001/11/06 11:21:08  nick
 * Move yuv2rgb to postprocess
 *
 *
 * Revision 1.31  2001/10/30 17:04:31  nick
 * Using new stuff of rgb15to16
 *
 * Revision 1.30  2001/08/13 11:08:18  atlka
 * changes according to -utf8 option, draw_osd() function added
 *
 * Revision 1.29  2001/07/16 18:41:52  jkeil
 * vo_dga doesn't compile on non-x86 architecture due to x86 asm usage.
 *
 * Revision 1.28  2001/07/03 23:45:49  arpi
 * extern vo_doublebuffering cleanup
 *
 * Revision 1.27  2001/06/22 19:51:25  atmosfear
 * Fixed pointer->integer cast warning.
 *
 * Revision 1.26  2001/06/18 16:38:06  acki2
 * - just modified an error message
 *
 * Revision 1.25  2001/06/17 22:21:47  acki2
 * - if DGA fails to report some valid modes, vo_dga now exits gracefully
 *   instead of crashing ... (100000x100000 bug ...)
 *
 * Revision 1.24  2001/06/17 20:59:39  acki2
 * - doublebuffering now can be switched on and off with the -(no)double switch.
 *   Default in libvo is disabled.
 *
 * Revision 1.23  2001/05/24 20:48:45  arpi_esp
 * removed redundant osd.h includes
 *
 * Revision 1.22  2001/05/07 19:16:04  acki2
 * - now chooses mode with highest ymax (enables doublebuffering in some cases
 *   it didn't work before)
 * - use my own memcopy() on non MMX machines again
 * - do memcpy() in one single block if stride==0
 *
 * Revision 1.21  2001/05/03 22:39:38  acki2
 * - finally: 15to16 conversion included!!!
 *
 * Revision 1.20  2001/05/02 23:21:27  acki2
 * - now we use fastmemcpy() for copying. Saves about 25% of copying time on K6-2+
 *
 * Revision 1.19  2001/05/01 22:37:37  acki2
 * - now features 24->32 conversion (this is actually faster than letting the
 *   codec produce depth 32 in the first place for avis :-))) )
 *
 * Revision 1.18  2001/05/01 20:24:31  acki2
 * - now mpeg is fast again (no more offscreen buffer rubbish) But is it really ok?
 *
 * Revision 1.17  2001/04/24 11:42:04  pontscho
 * clean up
 *
 * Revision 1.16  2001/04/24 10:21:12  szabii
 * some warnings killed
 *
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
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "../postproc/swscale.h"
#include "../postproc/rgb2rgb.h"
#include "aspect.h"

LIBVO_EXTERN( dga )

#include <X11/Xlib.h>
#include <X11/extensions/xf86dga.h>

#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif

#include "x11_common.h"
#include "../postproc/rgb2rgb.h"
#include "fastmemcpy.h"

#include "../mp_msg.h"

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


//#define BITSPP (vo_dga_modes[vo_dga_active_mode].vdm_bitspp)
//#define BYTESPP (vo_dga_modes[vo_dga_active_mode].vdm_bytespp)

#define VO_DGA_INVALID_RES 100000

#define HW_MODE (vo_dga_modes[vo_dga_hw_mode])
#define SRC_MODE (vo_dga_modes[vo_dga_src_mode]) 

struct vd_modes {
  int    vdm_mplayer_depth;
  int    vdm_supported;
  int    vdm_depth;
  int    vdm_bitspp;
  int    vdm_bytespp;
  int    vdm_rmask;
  int    vdm_gmask;
  int    vdm_bmask;
  int    vdm_hw_mode;
  int    vdm_conversion_func;
};

//------------------------------------------------------------------

#define VDM_CONV_NATIVE 0
#define VDM_CONV_15TO16 1
#define VDM_CONV_24TO32 2

static struct vd_modes vo_dga_modes[] = {
  // these entries describe HW modes
  // however, we use the same entries to tell mplayer what we support
  // so the last two values describe, which HW mode to use and which conversion 
  // function to use for a mode that is not supported by HW

  {  0,  0,  0,  0, 0,          0,          0, 0,      0, 0},
  { 15,  0, 15, 16, 2,     0x7c00,     0x03e0, 0x001f, 2, VDM_CONV_15TO16 },
  { 16,  0, 16, 16, 2,     0xf800,     0x07e0, 0x001f, 2, VDM_CONV_NATIVE },
  { 24,  0, 24, 24, 3,   0xff0000,   0x00ff00, 0x0000ff, 4, VDM_CONV_24TO32},
  { 32,  0, 24, 32, 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 4, VDM_CONV_NATIVE}
};

static int vo_dga_mode_num = sizeof(vo_dga_modes)/sizeof(struct vd_modes);

// enable a HW mode (by description)
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
       vo_dga_modes[i].vdm_hw_mode = i;
       vo_dga_modes[i].vdm_conversion_func = VDM_CONV_NATIVE;
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


// enable a HW mode (mplayer_depth decides which)
int vd_ValidateMode( int mplayer_depth){
  int i;
  if(mplayer_depth == 0)return 0;
  for(i=1; i<vo_dga_mode_num; i++){
    if(vo_dga_modes[i].vdm_mplayer_depth == mplayer_depth ){ 
      vo_dga_modes[i].vdm_supported = 1;
      vo_dga_modes[i].vdm_hw_mode = i;
      vo_dga_modes[i].vdm_conversion_func = VDM_CONV_NATIVE;
      return i;
    }
  }
  return 0;
}

// do we support this mode? (not important whether native or conversion)
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

int vd_ModeSupportedMethod( int mplayer_depth){
  int i;
  if(mplayer_depth == 0)return 0;
  for(i=1; i<vo_dga_mode_num; i++){
    if(vo_dga_modes[i].vdm_mplayer_depth == mplayer_depth && 
       vo_dga_modes[i].vdm_supported != 0){
      return vo_dga_modes[i].vdm_conversion_func;
    }
  }
  return 0;
}

char *vd_GetModeString(int index){

#define VO_DGA_MAX_STRING_LEN 100
  static char stringbuf[VO_DGA_MAX_STRING_LEN]; 
  stringbuf[VO_DGA_MAX_STRING_LEN-1]=0;
  snprintf(stringbuf, VO_DGA_MAX_STRING_LEN-2, 
    "depth=%d, bpp=%d, r=%06x, g=%06x, b=%06x, %s (-bpp %d)",
    vo_dga_modes[index].vdm_depth,
    vo_dga_modes[index].vdm_bitspp,
    vo_dga_modes[index].vdm_rmask,
    vo_dga_modes[index].vdm_gmask,
    vo_dga_modes[index].vdm_bmask,
    vo_dga_modes[index].vdm_supported ? 
    (vo_dga_modes[index].vdm_conversion_func == VDM_CONV_NATIVE ? 
        "native (fast),    " : "conversion (slow),") :
        "not supported :-( ",
    vo_dga_modes[index].vdm_mplayer_depth);
  return stringbuf;
}

//-----------------------------------------------------------------

#ifdef HAVE_XF86VM
static XF86VidModeModeInfo **vo_dga_vidmodes=NULL;
#endif


extern int       verbose;          

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
static int       vo_dga_hw_mode = 0;     // index in mode list that is actually
                                         // used by framebuffer
static int       vo_dga_src_mode = 0;    // index in mode list that is used by 
                                         // codec
static int       vo_dga_XServer_mode = 0;// index in mode list for resolution
                                         // XServer is running

#define MAX_NR_VIDEO_BUFFERS 3

#define VIDEO_BUFFER_DRAW \
        (vo_dga_video_buffer[vo_dga_current_video_buffer % \
			     vo_dga_nr_video_buffers])

#define VIDEO_BUFFER_DISPLAY \
        (vo_dga_video_buffer[(vo_dga_current_video_buffer + \
			      vo_dga_nr_video_buffers - 1) % \
			     vo_dga_nr_video_buffers])

static int vo_dga_nr_video_buffers;      // Total number of frame buffers.
static int vo_dga_current_video_buffer;  // Buffer available for rendering.

static struct video_buffer
{
	int y;
	uint8_t *data;
} vo_dga_video_buffer[MAX_NR_VIDEO_BUFFERS];

static Display  *vo_dga_dpy;

/* saved src and dst dimensions for SwScaler */
static unsigned int scale_srcW = 0,
                    scale_dstW = 0,
                    scale_srcH = 0,
                    scale_dstH = 0;


//---------------------------------------------------------

#define VD_INFO  0
#define VD_ERR   0
#define VD_DBG   2
#define VD_RES   1

void vd_printf( int level, const char *str, ...){
  va_list ap;

#ifndef VO_DGA_DBG
  // show resolution and DBG-messages only in verbose mode ...
  if( !verbose && level)return;         
#endif
  
  va_start(ap, str);
  vprintf(mp_gettext(str), ap);
  va_end(ap);
}

//---------------------------------------------------------

static void draw_alpha( int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride ){

  char *d;
  unsigned int offset;
  unsigned int buffer_stride;

  offset = vo_dga_width * y0 +x0;
  buffer_stride = vo_dga_width;
  d = VIDEO_BUFFER_DRAW.data;
     
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




// quick & dirty - for debugging only 

void fillblock(char *strt, int yoff, int lines, int val){
  char *i;
  for(i = strt + yoff * vo_dga_width *HW_MODE.vdm_bytespp; 
      i< strt + (lines+yoff) * vo_dga_width *HW_MODE.vdm_bytespp;  ){
    *i++ = val;
  }
}


//---------------------------------------------------------

static uint32_t draw_frame( uint8_t *src[] ){

  int vp_skip = vo_dga_vp_skip;
  int lpl = vo_dga_bytes_per_line >> 2; 
  int numlines = vo_dga_lines;     

  char *s, *d;

  s = *src;
  d = VIDEO_BUFFER_DRAW.data;
  
  switch(SRC_MODE.vdm_conversion_func){
  case VDM_CONV_NATIVE:

    mem2agpcpy_pic(
    	d, s, 
	vo_dga_bytes_per_line, 
	numlines,
	vo_dga_bytes_per_line+vo_dga_vp_skip, 
	vo_dga_bytes_per_line);
    
  // DBG-COde

#if 0
  d = VIDEO_BUFFER_DRAW.data;
  fillblock(d, 0, 10, 0x800000ff);
  fillblock(d, 10, 10, 0x8000ff00);
  fillblock(d, 20, 10, 0x80ff0000);
  fillblock(d, 30, 10, 0xff0000ff);
  fillblock(d, 40, 10, 0x800000ff);
  fillblock(d, 50, 10, 0x0f0000ff);
#endif
    break;
  case VDM_CONV_15TO16:
        {
	  int i;
	  char *e;
	  for(i=0; i< vo_dga_lines; i++){
            rgb15to16( s, d, vo_dga_bytes_per_line);
	    d+=vo_dga_bytes_per_line;
	    s+=vo_dga_bytes_per_line;
            d+= vo_dga_vp_skip;
	  }
	}
	break;
  case VDM_CONV_24TO32:

    {
      int i,k,l,m;
      for(i = 0; i< vo_dga_lines; i++ ){
	for(k = 0; k< vo_dga_src_width; k+=2 ){
          l = *(((uint32_t *)s)++);
          m = (l & 0xff000000)>> 24 ;
          *(((uint32_t *)d)++) = (l & 0x00ffffff); // | 0x80000000;
          m |= *(((uint16_t *)s)++) << 8;           
          *(((uint32_t *)d)++) = m; // | 0x80000000 ;
	}
        d+= vp_skip;
      }
    }
    //printf("vo_dga: 24 to 32 not implemented yet!!!\n");
    break;
  }
  return 0;
}

//---------------------------------------------------------

static void check_events(void)
{
  int e=vo_x11_check_events(vo_dga_dpy);
}

//---------------------------------------------------------

#include "sub.h"

static void draw_osd(void)
{ vo_draw_text(vo_dga_src_width,vo_dga_src_height,draw_alpha); }

static void switch_video_buffers(void)
{
	vo_dga_current_video_buffer =
		(vo_dga_current_video_buffer + 1) % vo_dga_nr_video_buffers;
}

static void flip_page( void )
{
	if(1 < vo_dga_nr_video_buffers)
	{
#ifdef HAVE_DGA2
		XDGASetViewport(vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 
				0,
				VIDEO_BUFFER_DISPLAY.y, 
				XDGAFlipRetrace);
#else
		XF86DGASetViewPort(vo_dga_dpy, XDefaultScreen(vo_dga_dpy),
				   0,
				   VIDEO_BUFFER_DISPLAY.y);
#endif
		switch_video_buffers();
	}
}

//---------------------------------------------------------

static uint32_t draw_slice( uint8_t *src[],int stride[],
                            int w,int h,int x,int y )
{
  if (scale_srcW) {
    uint8_t *dst[3] =
    {
	    VIDEO_BUFFER_DRAW.data + vo_dga_vp_offset,
	    0,
	    0
    };
    SwScale_YV12slice(src,stride,y,h,
          dst,
          /*scale_dstW*/ vo_dga_width * HW_MODE.vdm_bytespp, HW_MODE.vdm_bitspp,
		      scale_srcW, scale_srcH, scale_dstW, scale_dstH);
  } else {
    yuv2rgb(VIDEO_BUFFER_DRAW.data + vo_dga_vp_offset + 
          (vo_dga_width * y +x) * HW_MODE.vdm_bytespp,
           src[0], src[1], src[2],
           w,h, vo_dga_width * HW_MODE.vdm_bytespp,
           stride[0],stride[1] );
  }
  return 0;
};

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
    return 0; 
   }
   vo_dga_XServer_mode = vd_ValidateMode(vo_depthonscreen);
 
   if(vo_dga_XServer_mode ==0){
#ifndef HAVE_DGA2
     vd_printf(VD_ERR, "vo_dga: Your X-Server is not running in a ");
     vd_printf(VD_ERR, "resolution supported by DGA driver!\n");
#endif     
   }//else{
   //  vd_printf(VD_INFO, "vo_dga: X running at: %s\n", 
   //            vd_GetModeString(vo_dga_XServer_mode));
   //}                                
 
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

   }
#endif
   dga_depths_init = 1;
   XCloseDisplay(qdisp);

  if( !vo_dga_modes[1].vdm_supported && vo_dga_modes[2].vdm_supported ){
    vo_dga_modes[1].vdm_supported = 1;
  }

  if( !vo_dga_modes[3].vdm_supported && vo_dga_modes[4].vdm_supported ){
    vo_dga_modes[3].vdm_supported = 1;
  }

   for(i=1; i<vo_dga_mode_num; i++){
     vd_printf(VD_INFO, "vo_dga: Mode: %s", vd_GetModeString(i));
     if(vo_dbpp && vo_dbpp != vo_dga_modes[i].vdm_mplayer_depth){
       vo_dga_modes[i].vdm_supported = 0;
       vd_printf(VD_INFO, " ...disabled by -bpp %d", vo_dbpp );
     }
     vd_printf(VD_INFO, "\n");
   }
 }

 if( format==IMGFMT_YV12 ) return VFCAP_CSP_SUPPORTED;
 
 if( (format&IMGFMT_BGR_MASK) == IMGFMT_BGR && 
     vd_ModeValid(format&0xff))
 {
    if (vd_ModeSupportedMethod(format&0xff) == VDM_CONV_NATIVE)
	return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_OSD;
    else
	return VFCAP_CSP_SUPPORTED|VFCAP_OSD;
 }
 
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
	       int new_x, int new_y, int new_vbi, int new_maxy,
                int *old_x, int *old_y, int *old_vbi, int *old_maxy){

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
        ||
        // if everything is equal, then use the mode with the lower 
        // stride 
        (
	 (new_x == *old_x) &&
	 (new_y == *old_y) &&
         (new_vbi == *old_vbi) &&
         (new_maxy > *old_maxy)
        )
       )
      )
     )  
    {
      *old_x = new_x;
      *old_y = new_y;
      *old_maxy = new_maxy;
      *old_vbi = new_vbi;
      vd_printf(VD_RES, ".ok!!\n");
      return 1;
    }else{
      vd_printf(VD_RES, ".no\n");
      return 0;
    }
}



//---------------------------------------------------------

static void init_video_buffers(XDGAMode *modeline, uint8_t *buffer_base,
			       int use_multiple_buffers)
{
	int bytes_per_buffer =
		modeline->viewportHeight * modeline->bytesPerScanline;
	int i;

	if(use_multiple_buffers)
		vo_dga_nr_video_buffers =
			modeline->pixmapHeight / modeline->viewportHeight;
	else
		vo_dga_nr_video_buffers = 1;

	vo_dga_current_video_buffer = 0;
	
	if(MAX_NR_VIDEO_BUFFERS < vo_dga_nr_video_buffers)
		vo_dga_nr_video_buffers = MAX_NR_VIDEO_BUFFERS;
	
	for(i = 0; i < vo_dga_nr_video_buffers; i++)
	{
		vo_dga_video_buffer[i].y = i * modeline->viewportHeight;
		vo_dga_video_buffer[i].data =
			buffer_base + i * bytes_per_buffer;
		
		// Clear video buffer.
		memset(vo_dga_video_buffer[i].data, 0, bytes_per_buffer);
	}
}

static uint32_t config( uint32_t width,  uint32_t height,
                      uint32_t d_width,uint32_t d_height,
                      uint32_t flags,char *title,uint32_t format)
{

  int x_off, y_off;
  int wanted_width, wanted_height;

#ifdef HAVE_DGA2
  // needed to change DGA video mode
  int modecount, mX=VO_DGA_INVALID_RES, mY=VO_DGA_INVALID_RES , mVBI=100000, mMaxY=0, i,j=0;
  int dga_modenum;
  XDGAMode   *modelines=NULL, *modeline;
  XDGADevice *dgadevice;
  unsigned char *vo_dga_base;
  int max_vpy_pos;
#else
#ifdef HAVE_XF86VM
  unsigned int vm_event, vm_error;
  unsigned int vm_ver, vm_rev;
  int i, j=0, have_vm=0;
  int modecount, mX=VO_DGA_INVALID_RES, mY=VO_DGA_INVALID_RES, mVBI=100000, mMaxY=0, dga_modenum;  
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
      vo_dga_src_mode = vo_dga_XServer_mode;
    }else if((format & IMGFMT_BGR_MASK) == IMGFMT_BGR){
      vo_dga_src_mode = vd_ModeValid( format & 0xff );
    }
  }else{
    vo_dga_src_mode = vd_ModeValid(vo_dbpp);
  }
  vo_dga_hw_mode = SRC_MODE.vdm_hw_mode;

  if( format == IMGFMT_YV12 && vo_dga_src_mode != vo_dga_hw_mode ){
    vd_printf(VD_ERR, 
    "vo_dga: YV12 supports native modes only. Using %d instead of selected %d.\n",
       HW_MODE.vdm_mplayer_depth,
       SRC_MODE.vdm_mplayer_depth );
    vo_dga_src_mode = vo_dga_hw_mode;
  }

  if(!vo_dga_src_mode){ 
    vd_printf(VD_ERR, "vo_dga: unsupported video format!\n");
    return 1;
  }
  
  if((vo_dga_dpy = XOpenDisplay(0))==NULL){
    vd_printf (VD_ERR, "vo_dga: Can't open display\n");
    return 1;
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

  vd_printf(VD_DBG,
	    "vo_dga: modelines=%p, modecount=%d\n", modelines, modecount);

  if (modelines == NULL)
  {
    vd_printf(VD_ERR, "vo_dga: can't get modelines\n");
    return 1;
  }
  
  vd_printf(VD_INFO, 
            "vo_dga: DGA 2.0 available :-) Can switch resolution AND depth!\n");	
  for (i=0; i<modecount; i++)
  {
    if(vd_ModeEqual( modelines[i].depth, 
                     modelines[i].bitsPerPixel,
                     modelines[i].redMask,
		     modelines[i].greenMask,
	             modelines[i].blueMask,
                     vo_dga_hw_mode)){

       vd_printf(VD_DBG, "maxy: %4d, depth: %2d, %4dx%4d, ", 
                       modelines[i].maxViewportY, modelines[i].depth,
		       modelines[i].imageWidth, modelines[i].imageHeight );
       if ( check_res(i, wanted_width, wanted_height, modelines[i].depth,  
                  modelines[i].viewportWidth, 
                  modelines[i].viewportHeight, 
                  (unsigned) modelines[i].verticalRefresh, 
		  modelines[i].maxViewportY,
                   &mX, &mY, &mVBI, &mMaxY )) j = i;
     }
  }
  vd_printf(VD_INFO, 
     "vo_dga: Selected hardware mode %4d x %4d @ %3d Hz @ depth %2d, bitspp %2d.\n", 
     mX, mY, mVBI,
     HW_MODE.vdm_depth,
     HW_MODE.vdm_bitspp);  
  vd_printf(VD_INFO, 
     "vo_dga: Video parameters by codec: %3d x %3d, depth %2d, bitspp %2d.\n", 
     width, height,
     SRC_MODE.vdm_depth,
     SRC_MODE.vdm_bitspp);
  vo_dga_vp_width = mX;
  vo_dga_vp_height = mY;

  if((flags&0x04)||(flags&0x01)) { /* -zoom or -fs */
    scale_dstW = (d_width + 7) & ~7;
    scale_dstH = d_height;
    scale_srcW = width;
	  scale_srcH = height;
    aspect_save_screenres(mX,mY);
    aspect_save_orig(scale_srcW,scale_srcH);
	  aspect_save_prescale(scale_dstW,scale_dstH);
    SwScale_Init();
    if(flags&0x01) /* -fs */
      aspect(&scale_dstW,&scale_dstH,A_ZOOM);
    else if(flags&0x04) /* -fs */
      aspect(&scale_dstW,&scale_dstH,A_NOZOOM);
    vd_printf(VD_INFO,
       "vo_dga: Aspect corrected size for SwScaler: %4d x %4d.\n",
       scale_dstW, scale_dstH);
    /* XXX this is a hack, but I'm lazy ;-) :: atmos */
    width = scale_dstW;
    height = scale_dstH;
  }

  vo_dga_width = modelines[j].bytesPerScanline / HW_MODE.vdm_bytespp ;
  dga_modenum =  modelines[j].num;
  max_vpy_pos =  modelines[j].maxViewportY;
  modeline = modelines + j;
  
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
                        vo_dga_modes[vo_dga_hw_mode].vdm_depth,  
			vo_dga_vidmodes[i]->hdisplay, 
			vo_dga_vidmodes[i]->vdisplay,
			GET_VREFRESH(vo_dga_vidmodes[i]->dotclock, 
				     vo_dga_vidmodes[i]->htotal,
				     vo_dga_vidmodes[i]->vtotal),
		        0,
			&mX, &mY, &mVBI, &mMaxY )) j = i;
      }
    
      vd_printf(VD_INFO, 
 "vo_dga: Selected video mode %4d x %4d @ %3d Hz @ depth %2d, bitspp %2d, video %3d x %3d.\n", 
	mX, mY, mVBI, 
	vo_dga_modes[vo_dga_hw_mode].vdm_depth,
	vo_dga_modes[vo_dga_hw_mode].vdm_bitspp,
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
     return 1;
  }

  if(vo_dga_vp_width == VO_DGA_INVALID_RES){
    vd_printf( VD_ERR, "vo_dga: Something is wrong with your DGA. There doesn't seem to be a\n"
		       "         single suitable mode!\n"
		       "         Please file a bug report (see DOCS/DGA)\n");
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
  if (have_vm)
  {
    XF86VidModeLockModeSwitch(vo_dga_dpy,XDefaultScreen(vo_dga_dpy),0);
    // Two calls are needed to switch modes on my ATI Rage 128. Why?
    // for riva128 one call is enough!
    XF86VidModeSwitchToMode(vo_dga_dpy,XDefaultScreen(vo_dga_dpy),vo_dga_vidmodes[dga_modenum]);
    XF86VidModeSwitchToMode(vo_dga_dpy,XDefaultScreen(vo_dga_dpy),vo_dga_vidmodes[dga_modenum]);
  }
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
    yuv2rgb_init( vo_dga_modes[vo_dga_hw_mode].vdm_mplayer_depth , MODE_RGB );
    vd_printf( VD_DBG, "vo_dga: Using mplayer depth %d for YV12\n", 
               vo_dga_modes[vo_dga_hw_mode].vdm_mplayer_depth);
  }

  vd_printf(VD_DBG, "vo_dga: bytes/line: %d, screen res: %dx%d, depth: %d, base: %08x, bpp: %d\n", 
          vo_dga_width, vo_dga_vp_width, 
          vo_dga_vp_height, HW_MODE.vdm_bytespp, vo_dga_base,
          HW_MODE.vdm_bitspp);

  x_off = (vo_dga_vp_width - vo_dga_src_width)>>1; 
  y_off = (vo_dga_vp_height - vo_dga_src_height)>>1;

  vo_dga_bytes_per_line = vo_dga_src_width * HW_MODE.vdm_bytespp; 
  vo_dga_lines = vo_dga_src_height;                     

  vo_dga_src_offset = 0;
  vo_dga_vp_offset = (y_off * vo_dga_width + x_off ) * HW_MODE.vdm_bytespp;

  vo_dga_vp_skip = (vo_dga_width - vo_dga_src_width) * HW_MODE.vdm_bytespp;  // todo
    
  vd_printf(VD_DBG, "vo_dga: vp_off=%d, vp_skip=%d, bpl=%d\n", 
         vo_dga_vp_offset, vo_dga_vp_skip, vo_dga_bytes_per_line);

  
  XGrabKeyboard (vo_dga_dpy, DefaultRootWindow(vo_dga_dpy), True, 
                 GrabModeAsync,GrabModeAsync, CurrentTime);
  XGrabPointer (vo_dga_dpy, DefaultRootWindow(vo_dga_dpy), True, 
                ButtonPressMask,GrabModeAsync, GrabModeAsync, 
                None, None, CurrentTime);

  init_video_buffers(modeline, vo_dga_base, vo_doublebuffering);

  vd_printf(VD_DBG, "vo_dga: Using %d frame buffer%s.\n",
	    vo_dga_nr_video_buffers, vo_dga_nr_video_buffers == 1 ? "" : "s");
  
  vo_dga_is_running = 1;
  return 0;
}

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	vd_printf(VD_INFO, "vo_dga: Unknown subdevice: %s\n",arg);
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

//---------------------------------------------------------

// deleted the old vo_dga_query_event() routine 'cause it is obsolete  
// since using check_events()
// acki2 on 30/3/2001










